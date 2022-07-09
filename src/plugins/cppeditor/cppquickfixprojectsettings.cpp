// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppquickfixprojectsettings.hpp"
#include "cppeditorconstants.hpp"
#include <core/icore.hpp>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QtDebug>

namespace CppEditor {
namespace Internal {

using namespace Constants;

static constexpr char SETTINGS_FILE_NAME[] = ".cppQuickFix";
static constexpr char USE_GLOBAL_SETTINGS[] = "UseGlobalSettings";

CppQuickFixProjectsSettings::CppQuickFixProjectsSettings(ProjectExplorer::Project *project)
{
  m_project = project;
  const auto settings = m_project->namedSettings(QUICK_FIX_SETTINGS_ID).toMap();
  // if no option is saved try to load settings from a file
  m_useGlobalSettings = settings.value(USE_GLOBAL_SETTINGS, false).toBool();
  if (!m_useGlobalSettings) {
    m_settingsFile = searchForCppQuickFixSettingsFile();
    if (!m_settingsFile.isEmpty()) {
      loadOwnSettingsFromFile();
      m_useGlobalSettings = false;
    } else {
      m_useGlobalSettings = true;
    }
  }
  connect(project, &ProjectExplorer::Project::aboutToSaveSettings, [this] {
    auto settings = m_project->namedSettings(QUICK_FIX_SETTINGS_ID).toMap();
    settings.insert(USE_GLOBAL_SETTINGS, m_useGlobalSettings);
    m_project->setNamedSettings(QUICK_FIX_SETTINGS_ID, settings);
  });
}

auto CppQuickFixProjectsSettings::getSettings() -> CppQuickFixSettings*
{
  if (m_useGlobalSettings)
    return CppQuickFixSettings::instance();

  return &m_ownSettings;
}

auto CppQuickFixProjectsSettings::isUsingGlobalSettings() const -> bool
{
  return m_useGlobalSettings;
}

auto CppQuickFixProjectsSettings::filePathOfSettingsFile() const -> const Utils::FilePath&
{
  return m_settingsFile;
}

auto CppQuickFixProjectsSettings::getSettings(ProjectExplorer::Project *project) -> CppQuickFixProjectsSettings::CppQuickFixProjectsSettingsPtr
{
  const QString key = "CppQuickFixProjectsSettings";
  auto v = project->extraData(key);
  if (v.isNull()) {
    v = QVariant::fromValue(CppQuickFixProjectsSettingsPtr{new CppQuickFixProjectsSettings(project)});
    project->setExtraData(key, v);
  }
  return v.value<QSharedPointer<CppQuickFixProjectsSettings>>();
}

auto CppQuickFixProjectsSettings::getQuickFixSettings(ProjectExplorer::Project *project) -> CppQuickFixSettings*
{
  if (project)
    return getSettings(project)->getSettings();
  return CppQuickFixSettings::instance();
}

auto CppQuickFixProjectsSettings::searchForCppQuickFixSettingsFile() -> Utils::FilePath
{
  auto cur = m_project->projectDirectory();
  while (!cur.isEmpty()) {
    const auto path = cur / SETTINGS_FILE_NAME;
    if (path.exists())
      return path;

    cur = cur.parentDir();
  }
  return cur;
}

auto CppQuickFixProjectsSettings::useGlobalSettings() -> void
{
  m_useGlobalSettings = true;
}

auto CppQuickFixProjectsSettings::useCustomSettings() -> bool
{
  if (m_settingsFile.isEmpty()) {
    m_settingsFile = searchForCppQuickFixSettingsFile();
    const auto defaultLocation = m_project->projectDirectory() / SETTINGS_FILE_NAME;
    if (m_settingsFile.isEmpty()) {
      m_settingsFile = defaultLocation;
    } else if (m_settingsFile != defaultLocation) {
      QMessageBox msgBox(Core::ICore::dialogParent());
      msgBox.setText(tr("Quick Fix settings are saved in a file. Existing settings file " "\"%1\" found. Should this file be used or a " "new one be created?").arg(m_settingsFile.toString()));
      auto cancel = msgBox.addButton(QMessageBox::Cancel);
      cancel->setToolTip(tr("Switch Back to Global Settings"));
      auto useExisting = msgBox.addButton(tr("Use Existing"), QMessageBox::AcceptRole);
      useExisting->setToolTip(m_settingsFile.toString());
      auto createNew = msgBox.addButton(tr("Create New"), QMessageBox::ActionRole);
      createNew->setToolTip(defaultLocation.toString());
      msgBox.exec();
      if (msgBox.clickedButton() == createNew) {
        m_settingsFile = defaultLocation;
      } else if (msgBox.clickedButton() != useExisting) {
        m_settingsFile.clear();
        return false;
      }
    }

    resetOwnSettingsToGlobal();
  }
  if (m_settingsFile.exists())
    loadOwnSettingsFromFile();

  m_useGlobalSettings = false;
  return true;
}

auto CppQuickFixProjectsSettings::resetOwnSettingsToGlobal() -> void
{
  m_ownSettings = *CppQuickFixSettings::instance();
}

auto CppQuickFixProjectsSettings::saveOwnSettings() -> bool
{
  if (m_settingsFile.isEmpty())
    return false;

  QSettings settings(m_settingsFile.toString(), QSettings::IniFormat);
  if (settings.status() == QSettings::NoError) {
    m_ownSettings.saveSettingsTo(&settings);
    settings.sync();
    return settings.status() == QSettings::NoError;
  }
  m_settingsFile.clear();
  return false;
}

auto CppQuickFixProjectsSettings::loadOwnSettingsFromFile() -> void
{
  QSettings settings(m_settingsFile.toString(), QSettings::IniFormat);
  if (settings.status() == QSettings::NoError) {
    m_ownSettings.loadSettingsFrom(&settings);
    return;
  }
  m_settingsFile.clear();
}

} // namespace Internal
} // namespace CppEditor
