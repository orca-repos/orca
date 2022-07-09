// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakebuildsystem.hpp"

#include "builddirparameters.hpp"
#include "cmakebuildconfiguration.hpp"
#include "cmakebuildstep.hpp"
#include "cmakebuildtarget.hpp"
#include "cmakekitinformation.hpp"
#include "cmakeproject.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmakeprojectnodes.hpp"
#include "cmakeprojectplugin.hpp"
#include "cmakespecificsettings.hpp"
#include "projecttreehelper.hpp"
#include "utils/algorithm.hpp"

#include <constants/android/androidconstants.hpp>
#include <core/icore.hpp>
#include <core/progressmanager/progressmanager.hpp>
#include <cppeditor/cppeditorconstants.hpp>
#include <cppeditor/cppprojectupdater.hpp>
#include <cppeditor/generatedcodemodelsupport.hpp>
#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/session.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/taskhub.hpp>
#include <qmljs/qmljsmodelmanagerinterface.h>
#include <qtsupport/qtcppkitinfo.hpp>
#include <qtsupport/qtkitinformation.hpp>

#include <app/app_version.hpp>

#include <utils/checkablemessagebox.hpp>
#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>
#include <utils/mimetypes/mimetype.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/runextensions.hpp>

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

static auto copySourcePathsToClipboard(const FilePaths &srcPaths, const ProjectNode *node) -> void
{
  auto clip = QGuiApplication::clipboard();

  auto data = Utils::transform(srcPaths, [projDir = node->filePath()](const FilePath &path) {
    return path.relativePath(projDir).cleanPath().toString();
  }).join(" ");
  clip->setText(data);
}

static auto noAutoAdditionNotify(const FilePaths &filePaths, const ProjectNode *node) -> void
{
  const auto srcPaths = Utils::filtered(filePaths, [](const FilePath &file) {
    const auto mimeType = Utils::mimeTypeForFile(file).name();
    return mimeType == CppEditor::Constants::C_SOURCE_MIMETYPE || mimeType == CppEditor::Constants::C_HEADER_MIMETYPE || mimeType == CppEditor::Constants::CPP_SOURCE_MIMETYPE || mimeType == CppEditor::Constants::CPP_HEADER_MIMETYPE || mimeType == ProjectExplorer::Constants::FORM_MIMETYPE || mimeType == ProjectExplorer::Constants::RESOURCE_MIMETYPE || mimeType == ProjectExplorer::Constants::SCXML_MIMETYPE;
  });

  if (!srcPaths.empty()) {
    auto settings = CMakeProjectPlugin::projectTypeSpecificSettings();
    switch (settings->afterAddFileSetting.value()) {
    case AskUser: {
      auto checkValue{false};
      auto reply = CheckableMessageBox::question(Core::ICore::dialogParent(), QMessageBox::tr("Copy to Clipboard?"), QMessageBox::tr("Files are not automatically added to the " "CMakeLists.txt file of the CMake project." "\nCopy the path to the source files to the clipboard?"), "Remember My Choice", &checkValue, QDialogButtonBox::Yes | QDialogButtonBox::No, QDialogButtonBox::Yes);
      if (checkValue) {
        if (QDialogButtonBox::Yes == reply)
          settings->afterAddFileSetting.setValue(CopyFilePath);
        else if (QDialogButtonBox::No == reply)
          settings->afterAddFileSetting.setValue(NeverCopyFilePath);

        settings->writeSettings(Core::ICore::settings());
      }

      if (QDialogButtonBox::Yes == reply)
        copySourcePathsToClipboard(srcPaths, node);

      break;
    }

    case CopyFilePath: {
      copySourcePathsToClipboard(srcPaths, node);
      break;
    }

    case NeverCopyFilePath:
      break;
    }
  }
}

static Q_LOGGING_CATEGORY(cmakeBuildSystemLog, "qtc.cmake.buildsystem", QtWarningMsg);

// --------------------------------------------------------------------
// CMakeBuildSystem:
// --------------------------------------------------------------------

CMakeBuildSystem::CMakeBuildSystem(CMakeBuildConfiguration *bc) : BuildSystem(bc), m_cppCodeModelUpdater(new CppEditor::CppProjectUpdater)
{
  // TreeScanner:
  connect(&m_treeScanner, &TreeScanner::finished, this, &CMakeBuildSystem::handleTreeScanningFinished);

  m_treeScanner.setFilter([this](const MimeType &mimeType, const FilePath &fn) {
    // Mime checks requires more resources, so keep it last in check list
    auto isIgnored = TreeScanner::isWellKnownBinary(mimeType, fn);

    // Cache mime check result for speed up
    if (!isIgnored) {
      auto it = m_mimeBinaryCache.find(mimeType.name());
      if (it != m_mimeBinaryCache.end()) {
        isIgnored = *it;
      } else {
        isIgnored = TreeScanner::isMimeBinary(mimeType, fn);
        m_mimeBinaryCache[mimeType.name()] = isIgnored;
      }
    }

    return isIgnored;
  });

  m_treeScanner.setTypeFactory([](const MimeType &mimeType, const FilePath &fn) {
    auto type = TreeScanner::genericFileType(mimeType, fn);
    if (type == FileType::Unknown) {
      if (mimeType.isValid()) {
        const auto mt = mimeType.name();
        if (mt == CMakeProjectManager::Constants::CMAKE_PROJECT_MIMETYPE || mt == CMakeProjectManager::Constants::CMAKE_MIMETYPE)
          type = FileType::Project;
      }
    }
    return type;
  });

  connect(&m_reader, &FileApiReader::configurationStarted, this, [this]() {
    cmakeBuildConfiguration()->clearError(CMakeBuildConfiguration::ForceEnabledChanged::True);
  });

  connect(&m_reader, &FileApiReader::dataAvailable, this, &CMakeBuildSystem::handleParsingSucceeded);
  connect(&m_reader, &FileApiReader::errorOccurred, this, &CMakeBuildSystem::handleParsingFailed);
  connect(&m_reader, &FileApiReader::dirty, this, &CMakeBuildSystem::becameDirty);

  wireUpConnections();
}

CMakeBuildSystem::~CMakeBuildSystem()
{
  m_futureSynchronizer.waitForFinished();
  if (!m_treeScanner.isFinished()) {
    auto future = m_treeScanner.future();
    future.cancel();
    future.waitForFinished();
  }

  delete m_cppCodeModelUpdater;
  qDeleteAll(m_extraCompilers);
}

auto CMakeBuildSystem::triggerParsing() -> void
{
  qCDebug(cmakeBuildSystemLog) << cmakeBuildConfiguration()->displayName() << "Parsing has been triggered";

  if (!cmakeBuildConfiguration()->isActive()) {
    qCDebug(cmakeBuildSystemLog) << "Parsing has been triggered: SKIPPING since BC is not active -- clearing state.";
    stopParsingAndClearState();
    return; // ignore request, this build configuration is not active!
  }

  auto guard = guardParsingRun();

  if (!guard.guardsProject()) {
    // This can legitimately trigger if e.g. Build->Run CMake
    // is selected while this here is already running.

    // Stop old parse run and keep that ParseGuard!
    qCDebug(cmakeBuildSystemLog) << "Stopping current parsing run!";
    stopParsingAndClearState();
  } else {
    // Use new ParseGuard
    m_currentGuard = std::move(guard);
  }
  QTC_ASSERT(!m_reader.isParsing(), return);

  qCDebug(cmakeBuildSystemLog) << "ParseGuard acquired.";

  auto reparseParameters = takeReparseParameters();

  m_waitingForParse = true;
  m_combinedScanAndParseResult = true;

  QTC_ASSERT(m_parameters.isValid(), return);

  TaskHub::clearTasks(ProjectExplorer::Constants::TASK_CATEGORY_BUILDSYSTEM);

  qCDebug(cmakeBuildSystemLog) << "Parse called with flags:" << reparseParametersString(reparseParameters);

  const auto cache = m_parameters.buildDirectory.pathAppended("CMakeCache.txt").toString();
  if (!QFileInfo::exists(cache)) {
    reparseParameters |= REPARSE_FORCE_INITIAL_CONFIGURATION | REPARSE_FORCE_CMAKE_RUN;
    qCDebug(cmakeBuildSystemLog) << "No" << cache << "file found, new flags:" << reparseParametersString(reparseParameters);
  }

  if ((0 == (reparseParameters & REPARSE_FORCE_EXTRA_CONFIGURATION)) && mustApplyConfigurationChangesArguments(m_parameters)) {
    reparseParameters |= REPARSE_FORCE_CMAKE_RUN | REPARSE_FORCE_EXTRA_CONFIGURATION;
  }

  // The code model will be updated after the CMake run. There is no need to have an
  // active code model updater when the next one will be triggered.
  m_cppCodeModelUpdater->cancel();

  qCDebug(cmakeBuildSystemLog) << "Asking reader to parse";
  m_reader.parse(reparseParameters & REPARSE_FORCE_CMAKE_RUN, reparseParameters & REPARSE_FORCE_INITIAL_CONFIGURATION, reparseParameters & REPARSE_FORCE_EXTRA_CONFIGURATION);
}

auto CMakeBuildSystem::supportsAction(Node *context, ProjectAction action, const Node *node) const -> bool
{
  if (dynamic_cast<CMakeTargetNode*>(context))
    return action == ProjectAction::AddNewFile;

  if (dynamic_cast<CMakeListsNode*>(context))
    return action == ProjectAction::AddNewFile;

  return BuildSystem::supportsAction(context, action, node);
}

auto CMakeBuildSystem::addFiles(Node *context, const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  if (auto n = dynamic_cast<CMakeProjectNode*>(context)) {
    noAutoAdditionNotify(filePaths, n);
    return true; // Return always true as autoadd is not supported!
  }

  if (auto n = dynamic_cast<CMakeTargetNode*>(context)) {
    noAutoAdditionNotify(filePaths, n);
    return true; // Return always true as autoadd is not supported!
  }

  return BuildSystem::addFiles(context, filePaths, notAdded);
}

auto CMakeBuildSystem::filesGeneratedFrom(const FilePath &sourceFile) const -> FilePaths
{
  auto project = projectDirectory();
  auto baseDirectory = sourceFile.parentDir();

  while (baseDirectory.isChildOf(project)) {
    const auto cmakeListsTxt = baseDirectory.pathAppended("CMakeLists.txt");
    if (cmakeListsTxt.exists())
      break;
    baseDirectory = baseDirectory.parentDir();
  }

  const auto relativePath = baseDirectory.relativePath(project);
  auto generatedFilePath = cmakeBuildConfiguration()->buildDirectory().resolvePath(relativePath);

  if (sourceFile.suffix() == "ui") {
    generatedFilePath = generatedFilePath.pathAppended("ui_" + sourceFile.completeBaseName() + ".hpp").cleanPath();
    return {generatedFilePath};
  }
  if (sourceFile.suffix() == "scxml") {
    generatedFilePath = generatedFilePath.pathAppended(sourceFile.completeBaseName());
    return {generatedFilePath.stringAppended(".hpp"), generatedFilePath.stringAppended(".cpp")};
  }

  // TODO: Other types will be added when adapters for their compilers become available.
  return {};
}

auto CMakeBuildSystem::reparseParametersString(int reparseFlags) -> QString
{
  QString result;
  if (reparseFlags == REPARSE_DEFAULT) {
    result = "<NONE>";
  } else {
    if (reparseFlags & REPARSE_URGENT)
      result += " URGENT";
    if (reparseFlags & REPARSE_FORCE_CMAKE_RUN)
      result += " FORCE_CMAKE_RUN";
    if (reparseFlags & REPARSE_FORCE_INITIAL_CONFIGURATION)
      result += " FORCE_CONFIG";
  }
  return result.trimmed();
}

auto CMakeBuildSystem::setParametersAndRequestParse(const BuildDirParameters &parameters, const int reparseParameters) -> void
{
  project()->clearIssues();

  qCDebug(cmakeBuildSystemLog) << cmakeBuildConfiguration()->displayName() << "setting parameters and requesting reparse" << reparseParametersString(reparseParameters);

  if (!cmakeBuildConfiguration()->isActive()) {
    qCDebug(cmakeBuildSystemLog) << "setting parameters and requesting reparse: SKIPPING since BC is not active -- clearing state.";
    stopParsingAndClearState();
    return; // ignore request, this build configuration is not active!
  }

  if (!parameters.cmakeTool()) {
    TaskHub::addTask(BuildSystemTask(Task::Error, tr("The kit needs to define a CMake tool to parse this project.")));
    return;
  }
  if (!parameters.cmakeTool()->hasFileApi()) {
    TaskHub::addTask(BuildSystemTask(Task::Error, CMakeKitAspect::msgUnsupportedVersion(parameters.cmakeTool()->version().fullVersion)));
    return;
  }
  QTC_ASSERT(parameters.isValid(), return);

  m_parameters = parameters;
  m_parameters.buildDirectory = buildDirectory(parameters);
  updateReparseParameters(reparseParameters);

  m_reader.setParameters(m_parameters);

  if (reparseParameters & REPARSE_URGENT) {
    qCDebug(cmakeBuildSystemLog) << "calling requestReparse";
    requestParse();
  } else {
    qCDebug(cmakeBuildSystemLog) << "calling requestDelayedReparse";
    requestDelayedParse();
  }
}

auto CMakeBuildSystem::mustApplyConfigurationChangesArguments(const BuildDirParameters &parameters) const -> bool
{
  if (parameters.configurationChangesArguments.isEmpty())
    return false;

  auto answer = QMessageBox::question(Core::ICore::mainWindow(), tr("Apply configuration changes?"), "<p>" + tr("Run CMake with configuration changes?") + "</p><pre>" + parameters.configurationChangesArguments.join("\n") + "</pre>", QMessageBox::Apply | QMessageBox::Discard, QMessageBox::Apply);
  return answer == QMessageBox::Apply;
}

auto CMakeBuildSystem::runCMake() -> void
{
  BuildDirParameters parameters(cmakeBuildConfiguration());
  qCDebug(cmakeBuildSystemLog) << "Requesting parse due \"Run CMake\" command";
  setParametersAndRequestParse(parameters, REPARSE_FORCE_CMAKE_RUN | REPARSE_URGENT);
}

auto CMakeBuildSystem::runCMakeAndScanProjectTree() -> void
{
  BuildDirParameters parameters(cmakeBuildConfiguration());
  qCDebug(cmakeBuildSystemLog) << "Requesting parse due to \"Rescan Project\" command";
  setParametersAndRequestParse(parameters, REPARSE_FORCE_CMAKE_RUN | REPARSE_URGENT);
}

auto CMakeBuildSystem::runCMakeWithExtraArguments() -> void
{
  BuildDirParameters parameters(cmakeBuildConfiguration());
  qCDebug(cmakeBuildSystemLog) << "Requesting parse due to \"Rescan Project\" command";
  setParametersAndRequestParse(parameters, REPARSE_FORCE_CMAKE_RUN | REPARSE_FORCE_EXTRA_CONFIGURATION | REPARSE_URGENT);
}

auto CMakeBuildSystem::stopCMakeRun() -> void
{
  qCDebug(cmakeBuildSystemLog) << cmakeBuildConfiguration()->displayName() << "stopping CMake's run";
  m_reader.stopCMakeRun();
}

auto CMakeBuildSystem::buildCMakeTarget(const QString &buildTarget) -> void
{
  QTC_ASSERT(!buildTarget.isEmpty(), return);
  if (ProjectExplorerPlugin::saveModifiedFiles())
    cmakeBuildConfiguration()->buildTarget(buildTarget);
}

auto CMakeBuildSystem::persistCMakeState() -> bool
{
  BuildDirParameters parameters(cmakeBuildConfiguration());
  QTC_ASSERT(parameters.isValid(), return false);

  const auto hadBuildDirectory = parameters.buildDirectory.exists();
  parameters.buildDirectory = buildDirectory(parameters);

  int reparseFlags = REPARSE_DEFAULT;
  qCDebug(cmakeBuildSystemLog) << "Checking whether build system needs to be persisted:" << "buildDir:" << parameters.buildDirectory << "Has extraargs:" << !parameters.configurationChangesArguments.isEmpty();

  if (parameters.buildDirectory == parameters.buildDirectory && mustApplyConfigurationChangesArguments(parameters)) {
    reparseFlags = REPARSE_FORCE_EXTRA_CONFIGURATION;
    qCDebug(cmakeBuildSystemLog) << "   -> must run CMake with extra arguments.";
  }
  if (!hadBuildDirectory) {
    reparseFlags = REPARSE_FORCE_INITIAL_CONFIGURATION;
    qCDebug(cmakeBuildSystemLog) << "   -> must run CMake with initial arguments.";
  }

  if (reparseFlags == REPARSE_DEFAULT)
    return false;

  qCDebug(cmakeBuildSystemLog) << "Requesting parse to persist CMake State";
  setParametersAndRequestParse(parameters, REPARSE_URGENT | REPARSE_FORCE_CMAKE_RUN | reparseFlags);
  return true;
}

auto CMakeBuildSystem::clearCMakeCache() -> void
{
  QTC_ASSERT(m_parameters.isValid(), return);
  QTC_ASSERT(!m_isHandlingError, return);

  stopParsingAndClearState();

  const FilePath pathsToDelete[] = {m_parameters.buildDirectory / "CMakeCache.txt", m_parameters.buildDirectory / "CMakeCache.txt.prev", m_parameters.buildDirectory / "CMakeFiles", m_parameters.buildDirectory / ".cmake/api/v1/reply", m_parameters.buildDirectory / ".cmake/api/v1/reply.prev"};

  for (const auto &path : pathsToDelete)
    path.removeRecursively();

  emit configurationCleared();
}

auto CMakeBuildSystem::combineScanAndParse(bool restoredFromBackup) -> void
{
  if (cmakeBuildConfiguration()->isActive()) {
    if (m_waitingForParse)
      return;

    if (m_combinedScanAndParseResult) {
      updateProjectData();
      m_currentGuard.markAsSuccess();

      if (restoredFromBackup)
        project()->addIssue(CMakeProject::IssueType::Warning, tr("<b>CMake configuration failed<b>" "<p>The backup of the previous configuration has been restored.</p>" "<p>Have a look at the Issues pane or in the \"Projects > Build\" settings " "for more information about the failure.</p"));

      m_reader.resetData();

      m_currentGuard = {};
      m_testNames.clear();

      emitBuildSystemUpdated();

      runCTest();
    } else {
      updateFallbackProjectData();

      project()->addIssue(CMakeProject::IssueType::Warning, tr("<b>Failed to load project<b>" "<p>Have a look at the Issues pane or in the \"Projects > Build\" settings " "for more information about the failure.</p"));
    }
  }
}

auto CMakeBuildSystem::checkAndReportError(QString &errorMessage) -> void
{
  if (!errorMessage.isEmpty()) {
    cmakeBuildConfiguration()->setError(errorMessage);
    errorMessage.clear();
  }
}

auto CMakeBuildSystem::updateProjectData() -> void
{
  qCDebug(cmakeBuildSystemLog) << "Updating CMake project data";

  QTC_ASSERT(m_treeScanner.isFinished() && !m_reader.isParsing(), return);

  cmakeBuildConfiguration()->project()->setExtraProjectFiles(m_reader.projectFilesToWatch());

  auto patchedConfig = cmakeBuildConfiguration()->configurationFromCMake();
  {
    QSet<QString> res;
    QStringList apps;
    for (const auto &target : qAsConst(m_buildTargets)) {
      if (target.targetType == DynamicLibraryType) {
        res.insert(target.executable.parentDir().toString());
        apps.push_back(target.executable.toUserOutput());
      }
      // ### shall we add also the ExecutableType ?
    }
    {
      CMakeConfigItem paths;
      paths.key = Android::Constants::ANDROID_SO_LIBS_PATHS;
      paths.values = Utils::toList(res);
      patchedConfig.append(paths);
    }

    apps.sort();
    {
      CMakeConfigItem appsPaths;
      appsPaths.key = "TARGETS_BUILD_PATH";
      appsPaths.values = apps;
      patchedConfig.append(appsPaths);
    }
  }

  Project *p = project();
  {
    auto newRoot = m_reader.rootProjectNode();
    if (newRoot) {
      setRootProjectNode(std::move(newRoot));

      if (QTC_GUARD(p->rootProjectNode())) {
        const auto nodeName = p->rootProjectNode()->displayName();
        p->setDisplayName(nodeName);

        // set config on target nodes
        const auto buildKeys = Utils::transform<QSet>(m_buildTargets, &CMakeBuildTarget::title);
        p->rootProjectNode()->forEachProjectNode([patchedConfig, buildKeys](const ProjectNode *node) {
          if (buildKeys.contains(node->buildKey())) {
            auto targetNode = const_cast<CMakeTargetNode*>(dynamic_cast<const CMakeTargetNode*>(node));
            if (QTC_GUARD(targetNode))
              targetNode->setConfig(patchedConfig);
          }
        });
      }
    }
  }

  {
    qDeleteAll(m_extraCompilers);
    m_extraCompilers = findExtraCompilers();
    qCDebug(cmakeBuildSystemLog) << "Extra compilers created.";
  }

  QtSupport::CppKitInfo kitInfo(kit());
  QTC_ASSERT(kitInfo.isValid(), return);

  QString errorMessage;
  auto rpps = m_reader.createRawProjectParts(errorMessage);
  if (!errorMessage.isEmpty())
    cmakeBuildConfiguration()->setError(errorMessage);
  qCDebug(cmakeBuildSystemLog) << "Raw project parts created." << errorMessage;

  {
    for (auto &rpp : rpps) {
      rpp.setQtVersion(kitInfo.projectPartQtVersion); // TODO: Check if project actually uses Qt.
      const auto includeFileBaseDir = buildConfiguration()->buildDirectory().toString();
      if (kitInfo.cxxToolChain) {
        rpp.setFlagsForCxx({kitInfo.cxxToolChain, rpp.flagsForCxx.commandLineFlags, includeFileBaseDir});
      }
      if (kitInfo.cToolChain) {
        rpp.setFlagsForC({kitInfo.cToolChain, rpp.flagsForC.commandLineFlags, includeFileBaseDir});
      }
    }

    m_cppCodeModelUpdater->update({p, kitInfo, cmakeBuildConfiguration()->environment(), rpps}, m_extraCompilers);
  }
  {
    const auto mergedHeaderPathsAndQmlImportPaths = kit()->value(QtSupport::KitHasMergedHeaderPathsWithQmlImportPaths::id(), false).toBool();
    QStringList extraHeaderPaths;
    QList<QByteArray> moduleMappings;
    for (const auto &rpp : qAsConst(rpps)) {
      auto moduleMapFile = cmakeBuildConfiguration()->buildDirectory().pathAppended("qml_module_mappings/" + rpp.buildSystemTarget);
      if (moduleMapFile.exists()) {
        QFile mmf(moduleMapFile.toString());
        if (mmf.open(QFile::ReadOnly)) {
          auto content = mmf.readAll();
          auto lines = content.split('\n');
          for (const auto &line : lines) {
            if (!line.isEmpty())
              moduleMappings.append(line.simplified());
          }
        }
      }

      if (mergedHeaderPathsAndQmlImportPaths) {
        for (const auto &headerPath : rpp.headerPaths) {
          if (headerPath.type == HeaderPathType::User)
            extraHeaderPaths.append(headerPath.path);
        }
      }
    }
    updateQmlJSCodeModel(extraHeaderPaths, moduleMappings);
  }
  updateInitialCMakeExpandableVars();

  emit cmakeBuildConfiguration()->buildTypeChanged();

  qCDebug(cmakeBuildSystemLog) << "All CMake project data up to date.";
}

auto CMakeBuildSystem::handleTreeScanningFinished() -> void
{
  auto result = m_treeScanner.release();
  m_allFiles = result.folderNode;
  qDeleteAll(result.allFiles);

  updateFileSystemNodes();
}

auto CMakeBuildSystem::updateFileSystemNodes() -> void
{
  auto newRoot = std::make_unique<CMakeProjectNode>(m_parameters.sourceDirectory);
  newRoot->setDisplayName(m_parameters.sourceDirectory.fileName());

  if (!m_reader.topCmakeFile().isEmpty()) {
    auto node = std::make_unique<FileNode>(m_reader.topCmakeFile(), FileType::Project);
    node->setIsGenerated(false);

    std::vector<std::unique_ptr<FileNode>> fileNodes;
    fileNodes.emplace_back(std::move(node));

    addCMakeLists(newRoot.get(), std::move(fileNodes));
  }

  if (m_allFiles)
    addFileSystemNodes(newRoot.get(), m_allFiles);
  setRootProjectNode(std::move(newRoot));

  m_reader.resetData();

  m_currentGuard = {};
  emitBuildSystemUpdated();

  qCDebug(cmakeBuildSystemLog) << "All fallback CMake project data up to date.";
}

auto CMakeBuildSystem::updateFallbackProjectData() -> void
{
  qCDebug(cmakeBuildSystemLog) << "Updating fallback CMake project data";
  qCDebug(cmakeBuildSystemLog) << "Starting TreeScanner";
  QTC_CHECK(m_treeScanner.isFinished());
  if (m_treeScanner.asyncScanForFiles(projectDirectory()))
    Core::ProgressManager::addTask(m_treeScanner.future(), tr("Scan \"%1\" project tree").arg(project()->displayName()), "CMake.Scan.Tree");
}

auto CMakeBuildSystem::updateCMakeConfiguration(QString &errorMessage) -> void
{
  auto cmakeConfig = m_reader.takeParsedConfiguration(errorMessage);
  for (auto &ci : cmakeConfig)
    ci.inCMakeCache = true;
  if (!errorMessage.isEmpty()) {
    const auto changes = cmakeBuildConfiguration()->configurationChanges();
    for (const auto &ci : changes) {
      if (ci.isInitial)
        continue;
      const auto haveConfigItem = Utils::contains(cmakeConfig, [ci](const CMakeConfigItem &i) {
        return i.key == ci.key;
      });
      if (!haveConfigItem)
        cmakeConfig.append(ci);
    }
  }
  cmakeBuildConfiguration()->setConfigurationFromCMake(cmakeConfig);
}

auto CMakeBuildSystem::handleParsingSucceeded(bool restoredFromBackup) -> void
{
  if (!cmakeBuildConfiguration()->isActive()) {
    stopParsingAndClearState();
    return;
  }

  cmakeBuildConfiguration()->clearError();

  QString errorMessage;
  {
    m_buildTargets = Utils::transform(CMakeBuildStep::specialTargets(m_reader.usesAllCapsTargets()), [this](const QString &t) {
      CMakeBuildTarget result;
      result.title = t;
      result.workingDirectory = m_parameters.buildDirectory;
      result.sourceDirectory = m_parameters.sourceDirectory;
      return result;
    });
    m_buildTargets += m_reader.takeBuildTargets(errorMessage);
    checkAndReportError(errorMessage);
  }

  {
    updateCMakeConfiguration(errorMessage);
    checkAndReportError(errorMessage);
  }

  m_ctestPath = FilePath::fromString(m_reader.ctestPath());

  setApplicationTargets(appTargets());
  setDeploymentData(deploymentData());

  QTC_ASSERT(m_waitingForParse, return);
  m_waitingForParse = false;

  combineScanAndParse(restoredFromBackup);
}

auto CMakeBuildSystem::handleParsingFailed(const QString &msg) -> void
{
  cmakeBuildConfiguration()->setError(msg);

  QString errorMessage;
  updateCMakeConfiguration(errorMessage);
  // ignore errorMessage here, we already got one.

  m_ctestPath.clear();

  QTC_CHECK(m_waitingForParse);
  m_waitingForParse = false;
  m_combinedScanAndParseResult = false;

  combineScanAndParse(false);
}

auto CMakeBuildSystem::wireUpConnections() -> void
{
  // At this point the entire project will be fully configured, so let's connect everything and
  // trigger an initial parser run

  // Became active/inactive:
  connect(target(), &Target::activeBuildConfigurationChanged, this, [this]() {
    // Build configuration has changed:
    qCDebug(cmakeBuildSystemLog) << "Requesting parse due to active BC changed";
    setParametersAndRequestParse(BuildDirParameters(cmakeBuildConfiguration()), CMakeBuildSystem::REPARSE_DEFAULT);
  });
  connect(project(), &Project::activeTargetChanged, this, [this]() {
    // Build configuration has changed:
    qCDebug(cmakeBuildSystemLog) << "Requesting parse due to active target changed";
    setParametersAndRequestParse(BuildDirParameters(cmakeBuildConfiguration()), CMakeBuildSystem::REPARSE_DEFAULT);
  });

  // BuildConfiguration changed:
  connect(cmakeBuildConfiguration(), &CMakeBuildConfiguration::environmentChanged, this, [this]() {
    // The environment on our BC has changed, force CMake run to catch up with possible changes
    qCDebug(cmakeBuildSystemLog) << "Requesting parse due to environment change";
    setParametersAndRequestParse(BuildDirParameters(cmakeBuildConfiguration()), CMakeBuildSystem::REPARSE_FORCE_CMAKE_RUN);
  });
  connect(cmakeBuildConfiguration(), &CMakeBuildConfiguration::buildDirectoryChanged, this, [this]() {
    // The build directory of our BC has changed:
    // Does the directory contain a CMakeCache ? Existing build, just parse
    // No CMakeCache? Run with initial arguments!
    qCDebug(cmakeBuildSystemLog) << "Requesting parse due to build directory change";
    const BuildDirParameters parameters(cmakeBuildConfiguration());
    const auto cmakeCacheTxt = parameters.buildDirectory.pathAppended("CMakeCache.txt");
    const auto hasCMakeCache = QFile::exists(cmakeCacheTxt.toString());
    const auto options = ReparseParameters(hasCMakeCache ? REPARSE_DEFAULT : (REPARSE_FORCE_INITIAL_CONFIGURATION | REPARSE_FORCE_CMAKE_RUN));
    if (hasCMakeCache) {
      QString errorMessage;
      const auto config = CMakeBuildSystem::parseCMakeCacheDotTxt(cmakeCacheTxt, &errorMessage);
      if (!config.isEmpty() && errorMessage.isEmpty()) {
        auto cmakeBuildTypeName = config.stringValueOf("CMAKE_BUILD_TYPE");
        cmakeBuildConfiguration()->setCMakeBuildType(cmakeBuildTypeName, true);
      }
    }
    setParametersAndRequestParse(BuildDirParameters(cmakeBuildConfiguration()), options);
  });

  connect(project(), &Project::projectFileIsDirty, this, [this]() {
    if (cmakeBuildConfiguration()->isActive() && !isParsing()) {
      const auto cmake = CMakeKitAspect::cmakeTool(cmakeBuildConfiguration()->kit());
      if (cmake && cmake->isAutoRun()) {
        qCDebug(cmakeBuildSystemLog) << "Requesting parse due to dirty project file";
        setParametersAndRequestParse(BuildDirParameters(cmakeBuildConfiguration()), CMakeBuildSystem::REPARSE_FORCE_CMAKE_RUN);
      }
    }
  });

  // Force initial parsing run:
  if (cmakeBuildConfiguration()->isActive()) {
    qCDebug(cmakeBuildSystemLog) << "Initial run:";
    setParametersAndRequestParse(BuildDirParameters(cmakeBuildConfiguration()), CMakeBuildSystem::REPARSE_DEFAULT);
  }
}

auto CMakeBuildSystem::buildDirectory(const BuildDirParameters &parameters) -> FilePath
{
  const auto bdir = parameters.buildDirectory;

  if (!cmakeBuildConfiguration()->createBuildDirectory())
    handleParsingFailed(tr("Failed to create build directory \"%1\".").arg(bdir.toUserOutput()));

  return bdir;
}

auto CMakeBuildSystem::stopParsingAndClearState() -> void
{
  qCDebug(cmakeBuildSystemLog) << cmakeBuildConfiguration()->displayName() << "stopping parsing run!";
  m_reader.stop();
  m_reader.resetData();
}

auto CMakeBuildSystem::becameDirty() -> void
{
  qCDebug(cmakeBuildSystemLog) << "CMakeBuildSystem: becameDirty was triggered.";
  if (isParsing())
    return;

  setParametersAndRequestParse(BuildDirParameters(cmakeBuildConfiguration()), REPARSE_DEFAULT);
}

auto CMakeBuildSystem::updateReparseParameters(const int parameters) -> void
{
  m_reparseParameters |= parameters;
}

auto CMakeBuildSystem::takeReparseParameters() -> int
{
  auto result = m_reparseParameters;
  m_reparseParameters = REPARSE_DEFAULT;
  return result;
}

auto CMakeBuildSystem::runCTest() -> void
{
  if (!cmakeBuildConfiguration()->error().isEmpty() || m_ctestPath.isEmpty()) {
    qCDebug(cmakeBuildSystemLog) << "Cancel ctest run after failed cmake run";
    emit testInformationUpdated();
    return;
  }
  qCDebug(cmakeBuildSystemLog) << "Requesting ctest run after cmake run";

  const BuildDirParameters parameters(cmakeBuildConfiguration());
  QTC_ASSERT(parameters.isValid(), return);

  const CommandLine cmd{m_ctestPath, {"-N", "--show-only=json-v1"}};
  const auto workingDirectory = buildDirectory(parameters);
  const auto environment = cmakeBuildConfiguration()->environment();

  auto future = Utils::runAsync([cmd, workingDirectory, environment](QFutureInterface<QByteArray> &futureInterface) {
    QtcProcess process;
    process.setEnvironment(environment);
    process.setWorkingDirectory(workingDirectory);
    process.setCommand(cmd);
    process.start();

    if (!process.waitForStarted(1000) || !process.waitForFinished() || process.exitCode() || process.exitStatus() != QProcess::NormalExit) {
      return;
    }
    futureInterface.reportResult(process.readAllStandardOutput());
  });

  Utils::onFinished(future, this, [this](const QFuture<QByteArray> &future) {
    if (future.resultCount()) {
      const auto json = QJsonDocument::fromJson(future.result());
      if (!json.isEmpty() && json.isObject()) {
        const auto jsonObj = json.object();
        const auto btGraph = jsonObj.value("backtraceGraph").toObject();
        const auto cmakelists = btGraph.value("files").toArray();
        const auto nodes = btGraph.value("nodes").toArray();
        const auto tests = jsonObj.value("tests").toArray();
        auto counter = 0;
        for (const QJsonValue &testVal : tests) {
          ++counter;
          const auto test = testVal.toObject();
          QTC_ASSERT(!test.isEmpty(), continue);
          auto file = -1;
          auto line = -1;
          const auto bt = test.value("backtrace").toInt(-1);
          // we may have no real backtrace due to different registering
          if (bt != -1) {
            QSet<int> seen;
            std::function<QJsonObject(int)> findAncestor = [&](int index) {
              const auto node = nodes.at(index).toObject();
              const auto parent = node.value("parent").toInt(-1);
              if (seen.contains(parent) || parent < 0)
                return node;
              seen << parent;
              return findAncestor(parent);
            };
            const auto btRef = findAncestor(bt);
            file = btRef.value("file").toInt(-1);
            line = btRef.value("line").toInt(-1);
          }
          // we may have no CMakeLists.txt file reference due to different registering
          const auto cmakeFile = file != -1 ? FilePath::fromString(cmakelists.at(file).toString()) : FilePath();
          m_testNames.append({test.value("name").toString(), counter, cmakeFile, line});
        }
      }
    }
    emit testInformationUpdated();
  });

  m_futureSynchronizer.addFuture(future);
}

auto CMakeBuildSystem::cmakeBuildConfiguration() const -> CMakeBuildConfiguration*
{
  return static_cast<CMakeBuildConfiguration*>(BuildSystem::buildConfiguration());
}

static auto librarySearchPaths(const CMakeBuildSystem *bs, const QString &buildKey) -> Utils::FilePaths
{
  const auto cmakeBuildTarget = Utils::findOrDefault(bs->buildTargets(), Utils::equal(&CMakeBuildTarget::title, buildKey));

  return cmakeBuildTarget.libraryDirectories;
}

auto CMakeBuildSystem::appTargets() const -> const QList<BuildTargetInfo>
{
  QList<BuildTargetInfo> appTargetList;
  const auto forAndroid = DeviceTypeKitAspect::deviceTypeId(kit()) == Android::Constants::ANDROID_DEVICE_TYPE;
  for (const auto &ct : m_buildTargets) {
    if (CMakeBuildSystem::filteredOutTarget(ct))
      continue;

    if (ct.targetType == ExecutableType || (forAndroid && ct.targetType == DynamicLibraryType)) {
      const auto buildKey = ct.title;

      BuildTargetInfo bti;
      bti.displayName = ct.title;
      bti.targetFilePath = ct.executable;
      bti.projectFilePath = ct.sourceDirectory.cleanPath();
      bti.workingDirectory = ct.workingDirectory;
      bti.buildKey = buildKey;
      bti.usesTerminal = !ct.linksToQtGui;
      bti.isQtcRunnable = ct.qtcRunnable;

      // Workaround for QTCREATORBUG-19354:
      bti.runEnvModifier = [this, buildKey](Environment &env, bool enabled) {
        if (enabled)
          env.prependOrSetLibrarySearchPaths(librarySearchPaths(this, buildKey));
      };

      appTargetList.append(bti);
    }
  }

  return appTargetList;
}

auto CMakeBuildSystem::buildTargetTitles() const -> QStringList
{
  auto nonAutogenTargets = filtered(m_buildTargets, [](const CMakeBuildTarget &target) {
    return !CMakeBuildSystem::filteredOutTarget(target);
  });
  return transform(nonAutogenTargets, &CMakeBuildTarget::title);
}

auto CMakeBuildSystem::buildTargets() const -> const QList<CMakeBuildTarget>&
{
  return m_buildTargets;
}

auto CMakeBuildSystem::parseCMakeCacheDotTxt(const Utils::FilePath &cacheFile, QString *errorMessage) -> CMakeConfig
{
  if (!cacheFile.exists()) {
    if (errorMessage)
      *errorMessage = tr("CMakeCache.txt file not found.");
    return {};
  }
  auto result = CMakeConfig::fromFile(cacheFile, errorMessage);
  if (!errorMessage->isEmpty())
    return {};
  return result;
}

auto CMakeBuildSystem::filteredOutTarget(const CMakeBuildTarget &target) -> bool
{
  return target.title.endsWith("_autogen") || target.title.endsWith("_autogen_timestamp_deps");
}

auto CMakeBuildSystem::isMultiConfig() const -> bool
{
  return m_reader.isMultiConfig();
}

auto CMakeBuildSystem::usesAllCapsTargets() const -> bool
{
  return m_reader.usesAllCapsTargets();
}

auto CMakeBuildSystem::project() const -> CMakeProject*
{
  return static_cast<CMakeProject*>(ProjectExplorer::BuildSystem::project());
}

auto CMakeBuildSystem::testcasesInfo() const -> const QList<TestCaseInfo>
{
  return m_testNames;
}

auto CMakeBuildSystem::commandLineForTests(const QList<QString> &tests, const QStringList &options) const -> CommandLine
{
  auto args = options;
  const auto testsSet = Utils::toSet(tests);
  auto current = Utils::transform<QSet<QString>>(m_testNames, &TestCaseInfo::name);
  if (tests.isEmpty() || current == testsSet)
    return {m_ctestPath, args};

  QString testNumbers("0,0,0"); // start, end, stride
  for (const auto &info : m_testNames) {
    if (testsSet.contains(info.name))
      testNumbers += QString(",%1").arg(info.number);
  }
  args << "-I" << testNumbers;
  return {m_ctestPath, args};
}

auto CMakeBuildSystem::deploymentData() const -> DeploymentData
{
  DeploymentData result;

  QDir sourceDir = project()->projectDirectory().toString();
  QDir buildDir = cmakeBuildConfiguration()->buildDirectory().toString();

  QString deploymentPrefix;
  auto deploymentFilePath = sourceDir.filePath("QtCreatorDeployment.txt");

  auto hasDeploymentFile = QFileInfo::exists(deploymentFilePath);
  if (!hasDeploymentFile) {
    deploymentFilePath = buildDir.filePath("QtCreatorDeployment.txt");
    hasDeploymentFile = QFileInfo::exists(deploymentFilePath);
  }
  if (!hasDeploymentFile)
    return result;

  deploymentPrefix = result.addFilesFromDeploymentFile(deploymentFilePath, sourceDir.absolutePath());
  for (const auto &ct : m_buildTargets) {
    if (ct.targetType == ExecutableType || ct.targetType == DynamicLibraryType) {
      if (!ct.executable.isEmpty() && result.deployableForLocalFile(ct.executable).localFilePath() != ct.executable) {
        result.addFile(ct.executable, deploymentPrefix + buildDir.relativeFilePath(ct.executable.toFileInfo().dir().path()), DeployableFile::TypeExecutable);
      }
    }
  }

  return result;
}

auto CMakeBuildSystem::findExtraCompilers() -> QList<ExtraCompiler*>
{
  qCDebug(cmakeBuildSystemLog) << "Finding Extra Compilers: start.";

  QList<ExtraCompiler*> extraCompilers;
  const auto factories = ExtraCompilerFactory::extraCompilerFactories();

  qCDebug(cmakeBuildSystemLog) << "Finding Extra Compilers: Got factories.";

  const auto fileExtensions = Utils::transform<QSet>(factories, &ExtraCompilerFactory::sourceTag);

  qCDebug(cmakeBuildSystemLog) << "Finding Extra Compilers: Got file extensions:" << fileExtensions;

  // Find all files generated by any of the extra compilers, in a rather crude way.
  Project *p = project();
  const auto fileList = p->files([&fileExtensions](const Node *n) {
    if (!Project::SourceFiles(n) || !n->isEnabled()) // isEnabled excludes nodes from the file system tree
      return false;
    const auto fp = n->filePath().toString();
    const int pos = fp.lastIndexOf('.');
    return pos >= 0 && fileExtensions.contains(fp.mid(pos + 1));
  });

  qCDebug(cmakeBuildSystemLog) << "Finding Extra Compilers: Got list of files to check.";

  // Generate the necessary information:
  for (const auto &file : fileList) {
    qCDebug(cmakeBuildSystemLog) << "Finding Extra Compilers: Processing" << file.toUserOutput();
    auto factory = Utils::findOrDefault(factories, [&file](const ExtraCompilerFactory *f) {
      return file.endsWith('.' + f->sourceTag());
    });
    QTC_ASSERT(factory, continue);

    auto generated = filesGeneratedFrom(file);
    qCDebug(cmakeBuildSystemLog) << "Finding Extra Compilers:     generated files:" << generated;
    if (generated.isEmpty())
      continue;

    extraCompilers.append(factory->create(p, file, generated));
    qCDebug(cmakeBuildSystemLog) << "Finding Extra Compilers:     done with" << file.toUserOutput();
  }

  qCDebug(cmakeBuildSystemLog) << "Finding Extra Compilers: done.";

  return extraCompilers;
}

auto CMakeBuildSystem::updateQmlJSCodeModel(const QStringList &extraHeaderPaths, const QList<QByteArray> &moduleMappings) -> void
{
  auto modelManager = QmlJS::ModelManagerInterface::instance();

  if (!modelManager)
    return;

  Project *p = project();
  auto projectInfo = modelManager->defaultProjectInfoForProject(p);

  projectInfo.importPaths.clear();

  auto addImports = [&projectInfo](const QString &imports) {
    foreach(const QString &import, CMakeConfigItem::cmakeSplitValue(imports))
      projectInfo.importPaths.maybeInsert(FilePath::fromString(import), QmlJS::Dialect::Qml);
  };

  const auto &cm = cmakeBuildConfiguration()->configurationFromCMake();
  addImports(cm.stringValueOf("QML_IMPORT_PATH"));
  addImports(kit()->value(QtSupport::KitQmlImportPath::id()).toString());

  for (const auto &extraHeaderPath : extraHeaderPaths)
    projectInfo.importPaths.maybeInsert(FilePath::fromString(extraHeaderPath), QmlJS::Dialect::Qml);

  for (const auto &mm : moduleMappings) {
    auto kvPair = mm.split('=');
    if (kvPair.size() != 2)
      continue;
    auto from = QString::fromUtf8(kvPair.at(0).trimmed());
    auto to = QString::fromUtf8(kvPair.at(1).trimmed());
    if (!from.isEmpty() && !to.isEmpty() && from != to) {
      // The QML code-model does not support sub-projects, so if there are multiple mappings for a single module,
      // choose the shortest one.
      if (projectInfo.moduleMappings.contains(from)) {
        if (to.size() < projectInfo.moduleMappings.value(from).size())
          projectInfo.moduleMappings.insert(from, to);
      } else {
        projectInfo.moduleMappings.insert(from, to);
      }
    }
  }

  project()->setProjectLanguage(ProjectExplorer::Constants::QMLJS_LANGUAGE_ID, !projectInfo.sourceFiles.isEmpty());
  modelManager->updateProjectInfo(projectInfo, p);
}

auto CMakeBuildSystem::updateInitialCMakeExpandableVars() -> void
{
  const auto &cm = cmakeBuildConfiguration()->configurationFromCMake();
  const auto &initialConfig = cmakeBuildConfiguration()->initialCMakeConfiguration();

  CMakeConfig config;

  const auto projectDirectory = project()->projectDirectory();
  const auto samePath = [projectDirectory](const FilePath &first, const FilePath &second) {
    // if a path is relative, resolve it relative to the project directory
    // this is not 100% correct since CMake resolve them to CMAKE_CURRENT_SOURCE_DIR
    // depending on context, but we cannot do better here
    return first == second || projectDirectory.resolvePath(first) == projectDirectory.resolvePath(second) || projectDirectory.resolvePath(first).canonicalPath() == projectDirectory.resolvePath(second).canonicalPath();
  };

  // Replace path values that do not  exist on file system
  const QByteArrayList singlePathList = {"CMAKE_C_COMPILER", "CMAKE_CXX_COMPILER", "QT_QMAKE_EXECUTABLE", "QT_HOST_PATH", "CMAKE_PROJECT_INCLUDE_BEFORE", "CMAKE_TOOLCHAIN_FILE"};
  for (const auto &var : singlePathList) {
    auto it = std::find_if(cm.cbegin(), cm.cend(), [var](const CMakeConfigItem &item) {
      return item.key == var && !item.isInitial;
    });

    if (it != cm.cend()) {
      const auto initialValue = initialConfig.expandedValueOf(kit(), var).toUtf8();
      const auto initialPath = FilePath::fromString(QString::fromUtf8(initialValue));
      const auto path = FilePath::fromString(QString::fromUtf8(it->value));

      if (!initialValue.isEmpty() && !samePath(path, initialPath) && !path.exists()) {
        auto item(*it);
        item.value = initialValue;

        config << item;
      }
    }
  }

  // Prepend new values to existing path lists
  const QByteArrayList multiplePathList = {"CMAKE_PREFIX_PATH", "CMAKE_FIND_ROOT_PATH"};
  for (const auto &var : multiplePathList) {
    auto it = std::find_if(cm.cbegin(), cm.cend(), [var](const CMakeConfigItem &item) {
      return item.key == var && !item.isInitial;
    });

    if (it != cm.cend()) {
      const auto initialValueList = initialConfig.expandedValueOf(kit(), var).toUtf8().split(';');

      for (const auto &initialValue : initialValueList) {
        const auto initialPath = FilePath::fromString(QString::fromUtf8(initialValue));

        const auto pathIsContained = Utils::contains(it->value.split(';'), [samePath, initialPath](const QByteArray &p) {
          return samePath(FilePath::fromString(QString::fromUtf8(p)), initialPath);
        });
        if (!initialValue.isEmpty() && !pathIsContained) {
          auto item(*it);
          item.value = initialValue;
          item.value.append(";");
          item.value.append(it->value);

          config << item;
        }
      }
    }
  }

  if (!config.isEmpty()) emit cmakeBuildConfiguration()->configurationChanged(config);
}

} // namespace Internal
} // namespace CMakeProjectManager
