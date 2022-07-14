// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace Orca::Plugin::Core {

class SettingsDatabasePrivate;

class CORE_EXPORT SettingsDatabase final : public QObject {
public:
  SettingsDatabase(const QString &path, const QString &application, QObject *parent = nullptr);
  ~SettingsDatabase() override;

  auto setValue(const QString &key, const QVariant &value) const -> void;
  auto value(const QString &key, const QVariant &default_value = QVariant()) const -> QVariant;
  template <typename T>
  auto setValueWithDefault(const QString &key, const T &val, const T &default_value) -> void;
  template <typename T>
  auto setValueWithDefault(const QString &key, const T &val) -> void;
  auto contains(const QString &key) const -> bool;
  auto remove(const QString &key) const -> void;
  auto beginGroup(const QString &prefix) const -> void;
  auto endGroup() const -> void;
  auto group() const -> QString;
  auto childKeys() const -> QStringList;
  auto beginTransaction() const -> void;
  auto endTransaction() const -> void;
  static auto sync() -> void;

private:
  SettingsDatabasePrivate *d;
};

template <typename T>
auto SettingsDatabase::setValueWithDefault(const QString &key, const T &val, const T &default_value) -> void
{
  if (val == default_value)
    remove(key);
  else
    setValue(key, QVariant::fromValue(val));
}

template <typename T>
auto SettingsDatabase::setValueWithDefault(const QString &key, const T &val) -> void
{
  if (val == T())
    remove(key);
  else
    setValue(key, QVariant::fromValue(val));
}

} // namespace Orca::Plugin::Core
