// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codestylepool.hpp"
#include "icodestylepreferencesfactory.hpp"
#include "icodestylepreferences.hpp"
#include "tabsettings.hpp"

#include <core/icore.hpp>

#include <utils/fileutils.hpp>
#include <utils/persistentsettings.hpp>

#include <QMap>
#include <QDir>
#include <QDebug>
#include <QFileInfo>

using namespace TextEditor;

static const char codeStyleDataKey[] = "CodeStyleData";
static const char displayNameKey[] = "DisplayName";
static const char codeStyleDocKey[] = "QtCreatorCodeStyle";

namespace TextEditor {
namespace Internal {

class CodeStylePoolPrivate {
public:
  CodeStylePoolPrivate() = default;
  ~CodeStylePoolPrivate();

  auto generateUniqueId(const QByteArray &id) const -> QByteArray;

  ICodeStylePreferencesFactory *m_factory = nullptr;
  QList<ICodeStylePreferences*> m_pool;
  QList<ICodeStylePreferences*> m_builtInPool;
  QList<ICodeStylePreferences*> m_customPool;
  QMap<QByteArray, ICodeStylePreferences*> m_idToCodeStyle;
  QString m_settingsPath;
};

CodeStylePoolPrivate::~CodeStylePoolPrivate()
{
  delete m_factory;
}

auto CodeStylePoolPrivate::generateUniqueId(const QByteArray &id) const -> QByteArray
{
  if (!id.isEmpty() && !m_idToCodeStyle.contains(id))
    return id;

  int idx = id.size();
  while (idx > 0) {
    if (!isdigit(id.at(idx - 1)))
      break;
    idx--;
  }

  const auto baseName = id.left(idx);
  auto newName = baseName.isEmpty() ? QByteArray("codestyle") : baseName;
  auto i = 2;
  while (m_idToCodeStyle.contains(newName))
    newName = baseName + QByteArray::number(i++);

  return newName;
}

}}

static auto customCodeStylesPath() -> Utils::FilePath
{
  return Core::ICore::userResourcePath("codestyles");
}

CodeStylePool::CodeStylePool(ICodeStylePreferencesFactory *factory, QObject *parent) : QObject(parent), d(new Internal::CodeStylePoolPrivate)
{
  d->m_factory = factory;
}

CodeStylePool::~CodeStylePool()
{
  delete d;
}

auto CodeStylePool::settingsDir() const -> QString
{
  const auto suffix = d->m_factory ? d->m_factory->languageId().toString() : QLatin1String("default");
  return customCodeStylesPath().pathAppended(suffix).toString();
}

auto CodeStylePool::settingsPath(const QByteArray &id) const -> Utils::FilePath
{
  return Utils::FilePath::fromString(settingsDir()).pathAppended(QString::fromUtf8(id + ".xml"));
}

auto CodeStylePool::codeStyles() const -> QList<ICodeStylePreferences*>
{
  return d->m_pool;
}

auto CodeStylePool::builtInCodeStyles() const -> QList<ICodeStylePreferences*>
{
  return d->m_builtInPool;
}

auto CodeStylePool::customCodeStyles() const -> QList<ICodeStylePreferences*>
{
  return d->m_customPool;
}

auto CodeStylePool::cloneCodeStyle(ICodeStylePreferences *originalCodeStyle) -> ICodeStylePreferences*
{
  return createCodeStyle(originalCodeStyle->id(), originalCodeStyle->tabSettings(), originalCodeStyle->value(), originalCodeStyle->displayName());
}

auto CodeStylePool::createCodeStyle(const QByteArray &id, const TabSettings &tabSettings, const QVariant &codeStyleData, const QString &displayName) -> ICodeStylePreferences*
{
  if (!d->m_factory)
    return nullptr;

  const auto codeStyle = d->m_factory->createCodeStyle();
  codeStyle->setId(id);
  codeStyle->setTabSettings(tabSettings);
  codeStyle->setValue(codeStyleData);
  codeStyle->setDisplayName(displayName);

  addCodeStyle(codeStyle);

  saveCodeStyle(codeStyle);

  return codeStyle;
}

auto CodeStylePool::addCodeStyle(ICodeStylePreferences *codeStyle) -> void
{
  const auto newId = d->generateUniqueId(codeStyle->id());
  codeStyle->setId(newId);

  d->m_pool.append(codeStyle);
  if (codeStyle->isReadOnly())
    d->m_builtInPool.append(codeStyle);
  else
    d->m_customPool.append(codeStyle);
  d->m_idToCodeStyle.insert(newId, codeStyle);
  // take ownership
  codeStyle->setParent(this);

  connect(codeStyle, &ICodeStylePreferences::valueChanged, this, &CodeStylePool::slotSaveCodeStyle);
  connect(codeStyle, &ICodeStylePreferences::tabSettingsChanged, this, &CodeStylePool::slotSaveCodeStyle);
  connect(codeStyle, &ICodeStylePreferences::displayNameChanged, this, &CodeStylePool::slotSaveCodeStyle);
  emit codeStyleAdded(codeStyle);
}

auto CodeStylePool::removeCodeStyle(ICodeStylePreferences *codeStyle) -> void
{
  const int idx = d->m_customPool.indexOf(codeStyle);
  if (idx < 0)
    return;

  if (codeStyle->isReadOnly())
    return;

  emit codeStyleRemoved(codeStyle);
  d->m_customPool.removeAt(idx);
  d->m_pool.removeOne(codeStyle);
  d->m_idToCodeStyle.remove(codeStyle->id());

  QDir dir(settingsDir());
  dir.remove(settingsPath(codeStyle->id()).fileName());

  delete codeStyle;
}

auto CodeStylePool::codeStyle(const QByteArray &id) const -> ICodeStylePreferences*
{
  return d->m_idToCodeStyle.value(id);
}

auto CodeStylePool::loadCustomCodeStyles() -> void
{
  const QDir dir(settingsDir());
  const auto codeStyleFiles = dir.entryList(QStringList() << QLatin1String("*.xml"), QDir::Files);
  for (auto i = 0; i < codeStyleFiles.count(); i++) {
    const auto codeStyleFile = codeStyleFiles.at(i);
    // filter out styles which id is the same as one of built-in styles
    if (!d->m_idToCodeStyle.contains(QFileInfo(codeStyleFile).completeBaseName().toUtf8()))
      loadCodeStyle(Utils::FilePath::fromString(dir.absoluteFilePath(codeStyleFile)));
  }
}

auto CodeStylePool::importCodeStyle(const Utils::FilePath &fileName) -> ICodeStylePreferences*
{
  const auto codeStyle = loadCodeStyle(fileName);
  if (codeStyle)
    saveCodeStyle(codeStyle);
  return codeStyle;
}

auto CodeStylePool::loadCodeStyle(const Utils::FilePath &fileName) -> ICodeStylePreferences*
{
  ICodeStylePreferences *codeStyle = nullptr;
  Utils::PersistentSettingsReader reader;
  reader.load(fileName);
  const auto m = reader.restoreValues();
  if (m.contains(QLatin1String(codeStyleDataKey))) {
    const auto id = fileName.completeBaseName().toUtf8();
    const auto displayName = reader.restoreValue(QLatin1String(displayNameKey)).toString();
    const auto map = reader.restoreValue(QLatin1String(codeStyleDataKey)).toMap();
    if (d->m_factory) {
      codeStyle = d->m_factory->createCodeStyle();
      codeStyle->setId(id);
      codeStyle->setDisplayName(displayName);
      codeStyle->fromMap(map);

      addCodeStyle(codeStyle);
    }
  }
  return codeStyle;
}

auto CodeStylePool::slotSaveCodeStyle() -> void
{
  const auto codeStyle = qobject_cast<ICodeStylePreferences*>(sender());
  if (!codeStyle)
    return;

  saveCodeStyle(codeStyle);
}

auto CodeStylePool::saveCodeStyle(ICodeStylePreferences *codeStyle) const -> void
{
  const auto codeStylesPath = customCodeStylesPath().toString();

  // Create the base directory when it doesn't exist
  if (!QFile::exists(codeStylesPath) && !QDir().mkpath(codeStylesPath)) {
    qWarning() << "Failed to create code style directory:" << codeStylesPath;
    return;
  }
  const auto languageCodeStylesPath = settingsDir();
  // Create the base directory for the language when it doesn't exist
  if (!QFile::exists(languageCodeStylesPath) && !QDir().mkpath(languageCodeStylesPath)) {
    qWarning() << "Failed to create language code style directory:" << languageCodeStylesPath;
    return;
  }

  exportCodeStyle(settingsPath(codeStyle->id()), codeStyle);
}

auto CodeStylePool::exportCodeStyle(const Utils::FilePath &fileName, ICodeStylePreferences *codeStyle) const -> void
{
  const auto map = codeStyle->toMap();
  const QVariantMap tmp = {{displayNameKey, codeStyle->displayName()}, {codeStyleDataKey, map}};
  Utils::PersistentSettingsWriter writer(fileName, QLatin1String(codeStyleDocKey));
  writer.save(tmp, Core::ICore::dialogParent());
}
