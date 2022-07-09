// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"
#include "runconfiguration.hpp"

#include <extensionsystem/iplugin.hpp>

#include <QPair>

QT_BEGIN_NAMESPACE
class QPoint;
class QAction;
class QThreadPool;
QT_END_NAMESPACE

namespace Core {
class OutputWindow;
} // namespace Core

namespace Utils {
class ProcessHandle;
class FilePath;
}

namespace ProjectExplorer {

class BuildPropertiesSettings;
class CustomParserSettings;
class RunControl;
class RunConfiguration;
class Project;
class Node;
class FolderNode;
class FileNode;

namespace Internal {
class AppOutputSettings;
class MiniProjectTargetSelector;
class ProjectExplorerSettings;
} // namespace Internal

class PROJECTEXPLORER_EXPORT ProjectExplorerPlugin : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.qt-project.Qt.OrcaPlugin" FILE "ProjectExplorer.json")

  friend class ProjectExplorerPluginPrivate;

public:
  ProjectExplorerPlugin();
  ~ProjectExplorerPlugin() override;

  static auto instance() -> ProjectExplorerPlugin*;

  class OpenProjectResult {
  public:
    OpenProjectResult(const QList<Project*> &projects, const QList<Project*> &alreadyOpen, const QString &errorMessage) : m_projects(projects), m_alreadyOpen(alreadyOpen), m_errorMessage(errorMessage) { }

    explicit operator bool() const
    {
      return m_errorMessage.isEmpty() && m_alreadyOpen.isEmpty();
    }

    auto project() const -> Project*
    {
      return m_projects.isEmpty() ? nullptr : m_projects.first();
    }

    auto projects() const -> QList<Project*>
    {
      return m_projects;
    }

    auto errorMessage() const -> QString
    {
      return m_errorMessage;
    }

    auto alreadyOpen() const -> QList<Project*>
    {
      return m_alreadyOpen;
    }

  private:
    QList<Project*> m_projects;
    QList<Project*> m_alreadyOpen;
    QString m_errorMessage;
  };

  static auto openProject(const Utils::FilePath &filePath) -> OpenProjectResult;
  static auto openProjects(const Utils::FilePaths &filePaths) -> OpenProjectResult;
  static auto showOpenProjectError(const OpenProjectResult &result) -> void;
  static auto openProjectWelcomePage(const QString &fileName) -> void;
  static auto unloadProject(Project *project) -> void;
  static auto saveModifiedFiles() -> bool;
  static auto showContextMenu(QWidget *view, const QPoint &globalPos, Node *node) -> void;

  //PluginInterface
  auto initialize(const QStringList &arguments, QString *errorMessage) -> bool override;
  auto extensionsInitialized() -> void override;
  auto restoreKits() -> void;
  auto aboutToShutdown() -> ShutdownFlag override;

  static auto setProjectExplorerSettings(const Internal::ProjectExplorerSettings &pes) -> void;
  static auto projectExplorerSettings() -> const Internal::ProjectExplorerSettings&;
  static auto setAppOutputSettings(const Internal::AppOutputSettings &settings) -> void;
  static auto appOutputSettings() -> const Internal::AppOutputSettings&;
  static auto buildPropertiesSettings() -> BuildPropertiesSettings&;
  static auto showQtSettings() -> void;
  static auto setCustomParsers(const QList<CustomParserSettings> &settings) -> void;
  static auto addCustomParser(const CustomParserSettings &settings) -> void;
  static auto removeCustomParser(Utils::Id id) -> void;
  static auto customParsers() -> const QList<CustomParserSettings>;
  static auto startRunControl(RunControl *runControl) -> void;
  static auto showOutputPaneForRunControl(RunControl *runControl) -> void;

  // internal public for FlatModel
  static auto renameFile(Node *node, const QString &newFilePath) -> void;
  static auto projectFilePatterns() -> QStringList;
  static auto isProjectFile(const Utils::FilePath &filePath) -> bool;
  static auto recentProjects() -> QList<QPair<QString, QString>>;
  static auto canRunStartupProject(Utils::Id runMode, QString *whyNot = nullptr) -> bool;
  static auto runProject(Project *pro, Utils::Id, const bool forceSkipDeploy = false) -> void;
  static auto runStartupProject(Utils::Id runMode, bool forceSkipDeploy = false) -> void;
  static auto runRunConfiguration(RunConfiguration *rc, Utils::Id runMode, const bool forceSkipDeploy = false) -> void;
  static auto runningRunControlProcesses() -> QList<QPair<Runnable, Utils::ProcessHandle>>;
  static auto allRunControls() -> QList<RunControl*>;
  static auto addExistingFiles(FolderNode *folderNode, const Utils::FilePaths &filePaths) -> void;
  static auto initiateInlineRenaming() -> void;
  static auto projectFileGlobs() -> QStringList;
  static auto sharedThreadPool() -> QThreadPool*;
  static auto targetSelector() -> Internal::MiniProjectTargetSelector*;
  static auto showSessionManager() -> void;
  static auto openNewProjectDialog() -> void;
  static auto openOpenProjectDialog() -> void;
  static auto buildDirectoryTemplate() -> QString;
  static auto defaultBuildDirectoryTemplate() -> QString;
  static auto updateActions() -> void;
  static auto activateProjectPanel(Utils::Id panelId) -> void;
  static auto clearRecentProjects() -> void;
  static auto removeFromRecentProjects(const QString &fileName, const QString &displayName) -> void;
  static auto updateRunActions() -> void;
  static auto buildSystemOutput() -> Core::OutputWindow*;

signals:
  auto finishedInitialization() -> void;
  // Is emitted when a project has been added/removed,
  // or the file list of a specific project has changed.
  auto fileListChanged() -> void;
  auto recentProjectsChanged() -> void;
  auto settingsChanged() -> void;
  auto customParsersChanged() -> void;
  auto runActionsUpdated() -> void;

private:
  static auto coreAboutToClose() -> bool;
  auto handleCommandLineArguments(const QStringList &arguments) -> void;

#ifdef WITH_TESTS
private slots:
  void testJsonWizardsEmptyWizard();
  void testJsonWizardsEmptyPage();
  void testJsonWizardsUnusedKeyAtFields_data();
  void testJsonWizardsUnusedKeyAtFields();
  void testJsonWizardsCheckBox();
  void testJsonWizardsLineEdit();
  void testJsonWizardsComboBox();
  void testJsonWizardsIconList();
  void testAnsiFilterOutputParser_data();
  void testAnsiFilterOutputParser();
  void testGccOutputParsers_data();
  void testGccOutputParsers();
  void testCustomOutputParsers_data();
  void testCustomOutputParsers();
  void testClangOutputParser_data();
  void testClangOutputParser();
  void testLinuxIccOutputParsers_data();
  void testLinuxIccOutputParsers();
  void testGnuMakeParserParsing_data();
  void testGnuMakeParserParsing();
  void testGnuMakeParserTaskMangling();
  void testXcodebuildParserParsing_data();
  void testXcodebuildParserParsing();
  void testMsvcOutputParsers_data();
  void testMsvcOutputParsers();
  void testClangClOutputParsers_data();
  void testClangClOutputParsers();
  void testGccAbiGuessing_data();
  void testGccAbiGuessing();
  void testAbiRoundTrips();
  void testAbiOfBinary_data();
  void testAbiOfBinary();
  void testAbiFromTargetTriplet_data();
  void testAbiFromTargetTriplet();
  void testAbiUserOsFlavor_data();
  void testAbiUserOsFlavor();
  void testDeviceManager();
  void testToolChainMerging_data();
  void testToolChainMerging();
  void deleteTestToolchains();
  void testUserFileAccessor_prepareToReadSettings();
  void testUserFileAccessor_prepareToReadSettingsObsoleteVersion();
  void testUserFileAccessor_prepareToReadSettingsObsoleteVersionNewVersion();
  void testUserFileAccessor_prepareToWriteSettings();
  void testUserFileAccessor_mergeSettings();
  void testUserFileAccessor_mergeSettingsEmptyUser();
  void testUserFileAccessor_mergeSettingsEmptyShared();
  void testProject_setup();
  void testProject_changeDisplayName();
  void testProject_parsingSuccess();
  void testProject_parsingFail();
  void testProject_projectTree();
  void testProject_multipleBuildConfigs();
  void testSessionSwitch();
#endif // WITH_TESTS
};

} // namespace ProjectExplorer
