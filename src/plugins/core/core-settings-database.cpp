// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-settings-database.hpp"

#include <QDebug>
#include <QDir>
#include <QMap>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>
#include <QVariant>

/*!
    \class Orca::Plugin::Core::SettingsDatabase
    \inheaderfile coreplugin/settingsdatabase.h
    \inmodule Orca

    \brief The SettingsDatabase class offers an alternative to the
    application-wide QSettings that is more
    suitable for storing large amounts of data.

    The settings database is SQLite based, and lazily retrieves data when it
    is asked for. It also does incremental updates of the database rather than
    rewriting the whole file each time one of the settings change.

    The SettingsDatabase API mimics that of QSettings.
*/

enum {
  debug_settings = 0
};

namespace Orca::Plugin::Core {

using SettingsMap = QMap<QString, QVariant>;

class SettingsDatabasePrivate {
public:
  auto effectiveGroup() const -> QString
  {
    return m_groups.join(QString(QLatin1Char('/')));
  }

  auto effectiveKey(const QString &key) const -> QString
  {
    auto g = effectiveGroup();
    if (!g.isEmpty() && !key.isEmpty())
      g += QLatin1Char('/');
    g += key;
    return g;
  }

  SettingsMap m_settings;
  QStringList m_groups;
  QStringList m_dirty_keys;
  QSqlDatabase m_db;
};

SettingsDatabase::SettingsDatabase(const QString &path, const QString &application, QObject *parent) : QObject(parent), d(new SettingsDatabasePrivate)
{
  constexpr QLatin1Char slash('/');

  // TODO: Don't rely on a path, but determine automatically
  if (const QDir path_dir(path); !path_dir.exists())
    path_dir.mkpath(path_dir.absolutePath());

  auto file_name = path;

  if (!file_name.endsWith(slash))
    file_name += slash;

  file_name += application;
  file_name += QLatin1String(".db");

  d->m_db = QSqlDatabase::addDatabase(QLatin1String("QSQLITE"), QLatin1String("settings"));
  d->m_db.setDatabaseName(file_name);

  if (!d->m_db.open()) {
    qWarning().nospace() << "Warning: Failed to open settings database at " << file_name << " (" << d->m_db.lastError().driverText() << ")";
  } else {
    // Create the settings table if it doesn't exist yet
    QSqlQuery query(d->m_db);
    query.prepare(QLatin1String("CREATE TABLE IF NOT EXISTS settings (" "key PRIMARY KEY ON CONFLICT REPLACE, " "value)"));
    if (!query.exec())
      qWarning().nospace() << "Warning: Failed to prepare settings database! (" << query.lastError().driverText() << ")";
    // Retrieve all available keys (values are retrieved lazily)
    if (query.exec(QLatin1String("SELECT key FROM settings"))) {
      while (query.next()) {
        d->m_settings.insert(query.value(0).toString(), QVariant());
      }
    }
    // syncing can be slow, especially on Linux and Windows
    d->m_db.exec(QLatin1String("PRAGMA synchronous = OFF;"));
  }
}

SettingsDatabase::~SettingsDatabase()
{
  sync();
  delete d;
  QSqlDatabase::removeDatabase(QLatin1String("settings"));
}

auto SettingsDatabase::setValue(const QString &key, const QVariant &value) const -> void
{
  const auto effective_key = d->effectiveKey(key);

  // Add to cache
  d->m_settings.insert(effective_key, value);

  if (!d->m_db.isOpen())
    return;

  // Instant apply (TODO: Delay writing out settings)
  QSqlQuery query(d->m_db);
  query.prepare(QLatin1String("INSERT INTO settings VALUES (?, ?)"));
  query.addBindValue(effective_key);
  query.addBindValue(value);
  query.exec();

  if constexpr (debug_settings)
    qDebug() << "Stored:" << effective_key << "=" << value;
}

auto SettingsDatabase::value(const QString &key, const QVariant &default_value) const -> QVariant
{
  const auto effective_key = d->effectiveKey(key);
  auto value = default_value;

  if (const auto i = d->m_settings.constFind(effective_key); i != d->m_settings.constEnd() && i.value().isValid()) {
    value = i.value();
  } else if (d->m_db.isOpen()) {
    // Try to read the value from the database
    QSqlQuery query(d->m_db);
    query.prepare(QLatin1String("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(effective_key);
    query.exec();
    if (query.next()) {
      value = query.value(0);
      if constexpr (debug_settings)
        qDebug() << "Retrieved:" << effective_key << "=" << value;
    }
    // Cache the result
    d->m_settings.insert(effective_key, value);
  }

  return value;
}

auto SettingsDatabase::contains(const QString &key) const -> bool
{
  // check exact key
  // this already caches the value
  if (value(key).isValid())
    return true;
  // check for group
  if (d->m_db.isOpen()) {
    const QString glob = d->effectiveKey(key) + "/?*";
    QSqlQuery query(d->m_db);
    query.prepare(QLatin1String("SELECT value FROM settings WHERE key GLOB '%1' LIMIT 1").arg(glob));
    query.exec();
    if (query.next())
      return true;
  }
  return false;
}

auto SettingsDatabase::remove(const QString &key) const -> void
{
  const auto effective_key = d->effectiveKey(key);

  // Remove keys from the cache
  for(const auto &k: d->m_settings.keys()) {
    // Either it's an exact match, or it matches up to a /
    if (k.startsWith(effective_key) && (k.length() == effective_key.length() || k.at(effective_key.length()) == QLatin1Char('/'))) {
      d->m_settings.remove(k);
    }
  }

  if (!d->m_db.isOpen())
    return;

  // Delete keys from the database
  QSqlQuery query(d->m_db);
  query.prepare(QLatin1String("DELETE FROM settings WHERE key = ? OR key LIKE ?"));
  query.addBindValue(effective_key);
  query.addBindValue(QString(effective_key + QLatin1String("/%")));
  query.exec();
}

auto SettingsDatabase::beginGroup(const QString &prefix) const -> void
{
  d->m_groups.append(prefix);
}

auto SettingsDatabase::endGroup() const -> void
{
  d->m_groups.removeLast();
}

auto SettingsDatabase::group() const -> QString
{
  return d->effectiveGroup();
}

auto SettingsDatabase::childKeys() const -> QStringList
{
  QStringList children;

  const auto g = group();
  for (auto i = d->m_settings.cbegin(), end = d->m_settings.cend(); i != end; ++i) {
    if (const auto &key = i.key(); key.startsWith(g) && key.indexOf(QLatin1Char('/'), g.length() + 1) == -1)
      children.append(key.mid(g.length() + 1));
  }

  return children;
}

auto SettingsDatabase::beginTransaction() const -> void
{
  if (!d->m_db.isOpen())
    return;
  d->m_db.transaction();
}

auto SettingsDatabase::endTransaction() const -> void
{
  if (!d->m_db.isOpen())
    return;
  d->m_db.commit();
}

auto SettingsDatabase::sync() -> void
{
  // TODO: Delay writing of dirty keys and save them here
}

} // namespace Orca::Plugin::Core
