// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "allprojectsfind.hpp"

#include "project.hpp"
#include "session.hpp"
#include "projectexplorer.hpp"
#include "editorconfiguration.hpp"

#include <texteditor/texteditor.hpp>
#include <texteditor/textdocument.hpp>
#include <core/core-editor-manager.hpp>
#include <utils/filesearch.hpp>
#include <utils/algorithm.hpp>

#include <QGridLayout>
#include <QLabel>
#include <QSettings>

using namespace ProjectExplorer;
using namespace Internal;
using namespace TextEditor;

AllProjectsFind::AllProjectsFind() : m_configWidget(nullptr)
{
  connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::fileListChanged, this, &AllProjectsFind::handleFileListChanged);
}

auto AllProjectsFind::id() const -> QString
{
  return QLatin1String("All Projects");
}

auto AllProjectsFind::displayName() const -> QString
{
  return tr("All Projects");
}

auto AllProjectsFind::isEnabled() const -> bool
{
  return BaseFileFind::isEnabled() && SessionManager::hasProjects();
}

auto AllProjectsFind::files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator*
{
  Q_UNUSED(additionalParameters)
  return filesForProjects(nameFilters, exclusionFilters, SessionManager::projects());
}

auto AllProjectsFind::filesForProjects(const QStringList &nameFilters, const QStringList &exclusionFilters, const QList<Project*> &projects) const -> Utils::FileIterator*
{
  const auto filterFiles = Utils::filterFilesFunction(nameFilters, exclusionFilters);
  const auto openEditorEncodings = TextDocument::openedTextDocumentEncodings();
  QMap<QString, QTextCodec*> encodings;
  foreach(const Project *project, projects) {
    const EditorConfiguration *config = project->editorConfiguration();
    const auto projectCodec = config->useGlobalSettings() ? Orca::Plugin::Core::EditorManager::defaultTextCodec() : config->textCodec();
    const auto filteredFiles = filterFiles(transform(project->files(Project::SourceFiles), &Utils::FilePath::toString));
    for (const auto &fileName : filteredFiles) {
      auto codec = openEditorEncodings.value(fileName);
      if (!codec)
        codec = projectCodec;
      encodings.insert(fileName, codec);
    }
  }
  return new Utils::FileListIterator(encodings.keys(), encodings.values());
}

auto AllProjectsFind::additionalParameters() const -> QVariant
{
  return QVariant();
}

auto AllProjectsFind::label() const -> QString
{
  return tr("All Projects:");
}

auto AllProjectsFind::toolTip() const -> QString
{
  // last arg is filled by BaseFileFind::runNewSearch
  return tr("Filter: %1\nExcluding: %2\n%3").arg(fileNameFilters().join(',')).arg(fileExclusionFilters().join(','));
}

auto AllProjectsFind::handleFileListChanged() -> void
{
  emit enabledChanged(isEnabled());
}

auto AllProjectsFind::createConfigWidget() -> QWidget*
{
  if (!m_configWidget) {
    m_configWidget = new QWidget;
    const auto gridLayout = new QGridLayout(m_configWidget);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    m_configWidget->setLayout(gridLayout);
    const auto patternWidgets = createPatternWidgets();
    auto row = 0;
    for (const auto &p : patternWidgets) {
      gridLayout->addWidget(p.first, row, 0, Qt::AlignRight);
      gridLayout->addWidget(p.second, row, 1);
      ++row;
    }
    m_configWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  }
  return m_configWidget;
}

auto AllProjectsFind::writeSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("AllProjectsFind"));
  writeCommonSettings(settings);
  settings->endGroup();
}

auto AllProjectsFind::readSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("AllProjectsFind"));
  readCommonSettings(settings, "*", "");
  settings->endGroup();
}
