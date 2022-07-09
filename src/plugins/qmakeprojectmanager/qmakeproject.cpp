// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakeproject.hpp"

#include "qmakebuildconfiguration.hpp"
#include "qmakebuildinfo.hpp"
#include "qmakenodes.hpp"
#include "qmakenodetreebuilder.hpp"
#include "qmakeprojectimporter.hpp"
#include "qmakeprojectmanagerconstants.hpp"
#include "qmakestep.hpp"

#include <core/documentmanager.hpp>
#include <core/icontext.hpp>
#include <core/icore.hpp>
#include <core/progressmanager/progressmanager.hpp>

#include <cppeditor/cppmodelmanager.hpp>
#include <cppeditor/cppprojectupdater.hpp>
#include <cppeditor/generatedcodemodelsupport.hpp>
#include <cppeditor/projectinfo.hpp>

#include <projectexplorer/buildinfo.hpp>
#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/buildtargetinfo.hpp>
#include <projectexplorer/deploymentdata.hpp>
#include <projectexplorer/headerpath.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/rawprojectpart.hpp>
#include <projectexplorer/runconfiguration.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/taskhub.hpp>
#include <projectexplorer/toolchain.hpp>
#include <projectexplorer/toolchainmanager.hpp>

#include <proparser/qmakevfs.h>
#include <proparser/qmakeglobals.h>

#include <qtsupport/profilereader.hpp>
#include <qtsupport/qtcppkitinfo.hpp>
#include <qtsupport/qtkitinformation.hpp>
#include <qtsupport/qtversionmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/runextensions.hpp>
#include <qmljs/qmljsmodelmanagerinterface.h>

#include <QDebug>
#include <QDir>
#include <QFileSystemWatcher>
#include <QFuture>
#include <QLoggingCategory>
#include <QTimer>

using namespace QmakeProjectManager::Internal;
using namespace ProjectExplorer;
using namespace QtSupport;
using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

const int UPDATE_INTERVAL = 3000;

static Q_LOGGING_CATEGORY(qmakeBuildSystemLog, "qtc.qmake.buildsystem", QtWarningMsg);

#define TRACE(msg)                                                   \
    if (qmakeBuildSystemLog().isDebugEnabled()) {                    \
        qCDebug(qmakeBuildSystemLog)                                 \
            << qPrintable(buildConfiguration()->displayName())       \
            << ", guards project: " << int(m_guard.guardsProject())  \
            << ", isParsing: " << int(isParsing())                   \
            << ", hasParsingData: " << int(hasParsingData())         \
            << ", " << __FUNCTION__                                  \
            << msg;                                                  \
    }

class QmakePriFileDocument : public Core::IDocument {
public:
  QmakePriFileDocument(QmakePriFile *qmakePriFile, const Utils::FilePath &filePath) : IDocument(nullptr), m_priFile(qmakePriFile)
  {
    setId("Qmake.PriFile");
    setMimeType(QLatin1String(QmakeProjectManager::Constants::PROFILE_MIMETYPE));
    setFilePath(filePath);
    Core::DocumentManager::addDocument(this);
  }

  auto reloadBehavior(ChangeTrigger state, ChangeType type) const -> ReloadBehavior override
  {
    Q_UNUSED(state)
    Q_UNUSED(type)
    return BehaviorSilent;
  }

  auto reload(QString *errorString, ReloadFlag flag, ChangeType type) -> bool override
  {
    Q_UNUSED(errorString)
    Q_UNUSED(flag)
    Q_UNUSED(type)
    if (m_priFile)
      m_priFile->scheduleUpdate();
    return true;
  }

  auto setPriFile(QmakePriFile *priFile) -> void { m_priFile = priFile; }

private:
  QmakePriFile *m_priFile;
};

/// Watches folders for QmakePriFile nodes
/// use one file system watcher to watch all folders
/// such minimizing system ressouce usage

class CentralizedFolderWatcher : public QObject {
  Q_OBJECT public:
  CentralizedFolderWatcher(QmakeBuildSystem *BuildSystem);

  auto watchFolders(const QList<QString> &folders, QmakePriFile *file) -> void;
  auto unwatchFolders(const QList<QString> &folders, QmakePriFile *file) -> void;

private:
  auto folderChanged(const QString &folder) -> void;
  auto onTimer() -> void;
  auto delayedFolderChanged(const QString &folder) -> void;

  QmakeBuildSystem *m_buildSystem;
  auto recursiveDirs(const QString &folder) -> QSet<QString>;
  QFileSystemWatcher m_watcher;
  QMultiMap<QString, QmakePriFile*> m_map;

  QSet<QString> m_recursiveWatchedFolders;
  QTimer m_compressTimer;
  QSet<QString> m_changedFolders;
};

} // namespace Internal

/*!
  \class QmakeProject

  QmakeProject manages information about an individual qmake project file (.pro).
  */

QmakeProject::QmakeProject(const FilePath &fileName) : Project(QmakeProjectManager::Constants::PROFILE_MIMETYPE, fileName)
{
  setId(Constants::QMAKEPROJECT_ID);
  setProjectLanguages(Core::Context(ProjectExplorer::Constants::CXX_LANGUAGE_ID));
  setDisplayName(fileName.completeBaseName());
  setCanBuildProducts();
  setHasMakeInstallEquivalent(true);
}

QmakeProject::~QmakeProject()
{
  delete m_projectImporter;
  m_projectImporter = nullptr;

  // Make sure root node (and associated readers) are shut hown before proceeding
  setRootProjectNode(nullptr);
}

auto QmakeProject::fromMap(const QVariantMap &map, QString *errorMessage) -> Project::RestoreResult
{
  auto result = Project::fromMap(map, errorMessage);
  if (result != RestoreResult::Ok)
    return result;

  // Prune targets without buildconfigurations:
  // This can happen esp. when updating from a old version of Qt Creator
  auto ts = targets();
  foreach(Target *t, ts) {
    if (t->buildConfigurations().isEmpty()) {
      qWarning() << "Removing" << t->id().name() << "since it has no buildconfigurations!";
      removeTarget(t);
    }
  }

  return RestoreResult::Ok;
}

auto QmakeProject::deploymentKnowledge() const -> DeploymentKnowledge
{
  return DeploymentKnowledge::Approximative; // E.g. QTCREATORBUG-21855
}

//
// QmakeBuildSystem
//

QmakeBuildSystem::QmakeBuildSystem(QmakeBuildConfiguration *bc) : BuildSystem(bc), m_qmakeVfs(new QMakeVfs), m_cppCodeModelUpdater(new CppEditor::CppProjectUpdater)
{
  setParseDelay(0);

  m_rootProFile = std::make_unique<QmakeProFile>(this, projectFilePath());

  connect(BuildManager::instance(), &BuildManager::buildQueueFinished, this, &QmakeBuildSystem::buildFinished);

  connect(bc->target(), &Target::activeBuildConfigurationChanged, this, [this](BuildConfiguration *bc) {
    if (bc == buildConfiguration())
      scheduleUpdateAllNowOrLater();
    // FIXME: This is too eager in the presence of not handling updates
    // when the build configuration is not active, see startAsyncTimer
    // below.
    //        else
    //            m_cancelEvaluate = true;
  });

  connect(bc->project(), &Project::activeTargetChanged, this, &QmakeBuildSystem::activeTargetWasChanged);

  connect(bc->project(), &Project::projectFileIsDirty, this, &QmakeBuildSystem::scheduleUpdateAllLater);

  connect(bc, &BuildConfiguration::buildDirectoryChanged, this, &QmakeBuildSystem::scheduleUpdateAllNowOrLater);
  connect(bc, &BuildConfiguration::environmentChanged, this, &QmakeBuildSystem::scheduleUpdateAllNowOrLater);

  connect(ToolChainManager::instance(), &ToolChainManager::toolChainUpdated, this, [this](ToolChain *tc) {
    if (ToolChainKitAspect::cxxToolChain(kit()) == tc)
      scheduleUpdateAllNowOrLater();
  });

  connect(QtVersionManager::instance(), &QtVersionManager::qtVersionsChanged, this, [this](const QList<int> &, const QList<int> &, const QList<int> &changed) {
    if (changed.contains(QtKitAspect::qtVersionId(kit())))
      scheduleUpdateAllNowOrLater();
  });
}

QmakeBuildSystem::~QmakeBuildSystem()
{
  m_guard = {};
  delete m_cppCodeModelUpdater;
  m_cppCodeModelUpdater = nullptr;
  m_asyncUpdateState = ShuttingDown;

  // Make sure root node (and associated readers) are shut hown before proceeding
  m_rootProFile.reset();
  if (m_qmakeGlobalsRefCnt > 0) {
    m_qmakeGlobalsRefCnt = 0;
    deregisterFromCacheManager();
  }

  m_cancelEvaluate = true;
  QTC_CHECK(m_qmakeGlobalsRefCnt == 0);
  delete m_qmakeVfs;
  m_qmakeVfs = nullptr;

  if (m_asyncUpdateFutureInterface) {
    m_asyncUpdateFutureInterface->reportCanceled();
    m_asyncUpdateFutureInterface->reportFinished();
    m_asyncUpdateFutureInterface.reset();
  }
}

auto QmakeBuildSystem::updateCodeModels() -> void
{
  if (!buildConfiguration()->isActive())
    return;

  updateCppCodeModel();
  updateQmlJSCodeModel();
}

auto QmakeBuildSystem::updateDocuments() -> void
{
  QSet<FilePath> projectDocuments;
  project()->rootProjectNode()->forEachProjectNode([&projectDocuments](const ProjectNode *n) {
    projectDocuments.insert(n->filePath());

  });
  const auto priFileForPath = [p = project()](const FilePath &fp) -> QmakePriFile* {
    const auto n = p->nodeForFilePath(fp, [](const Node *n) {
      return dynamic_cast<const QmakePriFileNode*>(n);
    });
    QTC_ASSERT(n, return nullptr);
    return static_cast<const QmakePriFileNode*>(n)->priFile();
  };
  const auto docGenerator = [&](const FilePath &fp) -> std::unique_ptr<Core::IDocument> {
    const auto priFile = priFileForPath(fp);
    QTC_ASSERT(priFile, return std::make_unique<Core::IDocument>());
    return std::make_unique<QmakePriFileDocument>(priFile, fp);
  };
  const auto docUpdater = [&](Core::IDocument *doc) {
    const auto priFile = priFileForPath(doc->filePath());
    QTC_ASSERT(priFile, return);
    static_cast<QmakePriFileDocument*>(doc)->setPriFile(priFile);
  };
  project()->setExtraProjectFiles(projectDocuments, docGenerator, docUpdater);
}

auto QmakeBuildSystem::updateCppCodeModel() -> void
{
  m_toolChainWarnings.clear();

  QtSupport::CppKitInfo kitInfo(kit());
  QTC_ASSERT(kitInfo.isValid(), return);

  QList<ProjectExplorer::ExtraCompiler*> generators;
  RawProjectParts rpps;
  for (const QmakeProFile *pro : rootProFile()->allProFiles()) {
    warnOnToolChainMismatch(pro);

    RawProjectPart rpp;
    rpp.setDisplayName(pro->displayName());
    rpp.setProjectFileLocation(pro->filePath().toString());
    rpp.setBuildSystemTarget(pro->filePath().toString());
    switch (pro->projectType()) {
    case ProjectType::ApplicationTemplate:
      rpp.setBuildTargetType(BuildTargetType::Executable);
      break;
    case ProjectType::SharedLibraryTemplate:
    case ProjectType::StaticLibraryTemplate:
      rpp.setBuildTargetType(BuildTargetType::Library);
      break;
    default:
      rpp.setBuildTargetType(BuildTargetType::Unknown);
      break;
    }
    const auto includeFileBaseDir = pro->sourceDir().toString();
    rpp.setFlagsForCxx({kitInfo.cxxToolChain, pro->variableValue(Variable::CppFlags), includeFileBaseDir});
    rpp.setFlagsForC({kitInfo.cToolChain, pro->variableValue(Variable::CFlags), includeFileBaseDir});
    rpp.setMacros(ProjectExplorer::Macro::toMacros(pro->cxxDefines()));
    rpp.setPreCompiledHeaders(pro->variableValue(Variable::PrecompiledHeader));
    rpp.setSelectedForBuilding(pro->includedInExactParse());

    // Qt Version
    if (pro->variableValue(Variable::Config).contains(QLatin1String("qt")))
      rpp.setQtVersion(kitInfo.projectPartQtVersion);
    else
      rpp.setQtVersion(Utils::QtMajorVersion::None);

    // Header paths
    ProjectExplorer::HeaderPaths headerPaths;
    foreach(const QString &inc, pro->variableValue(Variable::IncludePath)) {
      const auto headerPath = HeaderPath::makeUser(inc);
      if (!headerPaths.contains(headerPath))
        headerPaths += headerPath;
    }

    if (kitInfo.qtVersion && !kitInfo.qtVersion->frameworkPath().isEmpty())
      headerPaths += HeaderPath::makeFramework(kitInfo.qtVersion->frameworkPath());
    rpp.setHeaderPaths(headerPaths);

    // Files and generators
    const auto cumulativeSourceFiles = pro->variableValue(Variable::CumulativeSource);
    auto fileList = pro->variableValue(Variable::ExactSource) + cumulativeSourceFiles;
    auto proGenerators = pro->extraCompilers();
    foreach(ProjectExplorer::ExtraCompiler *ec, proGenerators) {
      ec->forEachTarget([&](const Utils::FilePath &generatedFile) {
        fileList += generatedFile.toString();
      });
    }
    generators.append(proGenerators);
    fileList.prepend(CppEditor::CppModelManager::configurationFileName());
    rpp.setFiles(fileList, [cumulativeSourceFiles](const QString &filePath) {
      // Keep this lambda thread-safe!
      return !cumulativeSourceFiles.contains(filePath);
    });

    rpps.append(rpp);
  }

  m_cppCodeModelUpdater->update({project(), kitInfo, activeParseEnvironment(), rpps}, generators);
}

auto QmakeBuildSystem::updateQmlJSCodeModel() -> void
{
  QmlJS::ModelManagerInterface *modelManager = QmlJS::ModelManagerInterface::instance();
  if (!modelManager)
    return;

  QmlJS::ModelManagerInterface::ProjectInfo projectInfo = modelManager->defaultProjectInfoForProject(project());

  const auto proFiles = rootProFile()->allProFiles();

  projectInfo.importPaths.clear();

  auto hasQmlLib = false;
  for (auto file : proFiles) {
    for (const auto &path : file->variableValue(Variable::QmlImportPath)) {
      projectInfo.importPaths.maybeInsert(FilePath::fromString(path), QmlJS::Dialect::Qml);
    }
    const auto &exactResources = file->variableValue(Variable::ExactResource);
    const auto &cumulativeResources = file->variableValue(Variable::CumulativeResource);
    projectInfo.activeResourceFiles.append(exactResources);
    projectInfo.allResourceFiles.append(exactResources);
    projectInfo.allResourceFiles.append(cumulativeResources);
    QString errorMessage;
    foreach(const QString &rc, exactResources) {
      QString contents;
      auto id = m_qmakeVfs->idForFileName(rc, QMakeVfs::VfsExact);
      if (m_qmakeVfs->readFile(id, &contents, &errorMessage) == QMakeVfs::ReadOk)
        projectInfo.resourceFileContents[rc] = contents;
    }
    foreach(const QString &rc, cumulativeResources) {
      QString contents;
      auto id = m_qmakeVfs->idForFileName(rc, QMakeVfs::VfsCumulative);
      if (m_qmakeVfs->readFile(id, &contents, &errorMessage) == QMakeVfs::ReadOk)
        projectInfo.resourceFileContents[rc] = contents;
    }
    if (!hasQmlLib) {
      auto qtLibs = file->variableValue(Variable::Qt);
      hasQmlLib = qtLibs.contains(QLatin1String("declarative")) || qtLibs.contains(QLatin1String("qml")) || qtLibs.contains(QLatin1String("quick"));
    }
  }

  // If the project directory has a pro/pri file that includes a qml or quick or declarative
  // library then chances of the project being a QML project is quite high.
  // This assumption fails when there are no QDeclarativeEngine/QDeclarativeView (QtQuick 1)
  // or QQmlEngine/QQuickView (QtQuick 2) instances.
  project()->setProjectLanguage(ProjectExplorer::Constants::QMLJS_LANGUAGE_ID, hasQmlLib);

  projectInfo.activeResourceFiles.removeDuplicates();
  projectInfo.allResourceFiles.removeDuplicates();

  modelManager->updateProjectInfo(projectInfo, project());
}

auto QmakeBuildSystem::scheduleAsyncUpdateFile(QmakeProFile *file, QmakeProFile::AsyncUpdateDelay delay) -> void
{
  if (m_asyncUpdateState == ShuttingDown)
    return;

  if (m_cancelEvaluate) {
    // A cancel is in progress
    // That implies that a full update is going to happen afterwards
    // So we don't need to do anything
    return;
  }

  file->setParseInProgressRecursive(true);

  if (m_asyncUpdateState == AsyncFullUpdatePending) {
    // Just postpone
    startAsyncTimer(delay);
  } else if (m_asyncUpdateState == AsyncPartialUpdatePending || m_asyncUpdateState == Base) {
    // Add the node
    m_asyncUpdateState = AsyncPartialUpdatePending;

    auto add = true;
    auto it = m_partialEvaluate.begin();
    while (it != m_partialEvaluate.end()) {
      if (*it == file) {
        add = false;
        break;
      } else if (file->isParent(*it)) {
        // We already have the parent in the list, nothing to do
        it = m_partialEvaluate.erase(it);
      } else if ((*it)->isParent(file)) {
        // The node is the parent of a child already in the list
        add = false;
        break;
      } else {
        ++it;
      }
    }

    if (add)
      m_partialEvaluate.append(file);

    // Cancel running code model update
    m_cppCodeModelUpdater->cancel();

    startAsyncTimer(delay);
  } else if (m_asyncUpdateState == AsyncUpdateInProgress) {
    // A update is in progress
    // And this slot only gets called if a file changed on disc
    // So we'll play it safe and schedule a complete evaluate
    // This might trigger if due to version control a few files
    // change a partial update gets in progress and then another
    // batch of changes come in, which triggers a full update
    // even if that's not really needed
    scheduleUpdateAll(delay);
  }
}

auto QmakeBuildSystem::scheduleUpdateAllNowOrLater() -> void
{
  if (m_firstParseNeeded)
    scheduleUpdateAll(QmakeProFile::ParseNow);
  else
    scheduleUpdateAll(QmakeProFile::ParseLater);
}

auto QmakeBuildSystem::qmakeBuildConfiguration() const -> QmakeBuildConfiguration*
{
  return static_cast<QmakeBuildConfiguration*>(BuildSystem::buildConfiguration());
}

auto QmakeBuildSystem::scheduleUpdateAll(QmakeProFile::AsyncUpdateDelay delay) -> void
{
  if (m_asyncUpdateState == ShuttingDown) {
    TRACE("suppressed: we are shutting down");
    return;
  }

  if (m_cancelEvaluate) {
    // we are in progress of canceling
    // and will start the evaluation after that
    TRACE("suppressed: was previously canceled");
    return;
  }

  if (!buildConfiguration()->isActive()) {
    TRACE("firstParseNeeded: " << int(m_firstParseNeeded) << ", suppressed: buildconfig not active");
    return;
  }

  TRACE("firstParseNeeded: " << int(m_firstParseNeeded) << ", delay: " << delay);

  rootProFile()->setParseInProgressRecursive(true);

  if (m_asyncUpdateState == AsyncUpdateInProgress) {
    m_cancelEvaluate = true;
    m_asyncUpdateState = AsyncFullUpdatePending;
    return;
  }

  m_partialEvaluate.clear();
  m_asyncUpdateState = AsyncFullUpdatePending;

  // Cancel running code model update
  m_cppCodeModelUpdater->cancel();
  startAsyncTimer(delay);
}

auto QmakeBuildSystem::startAsyncTimer(QmakeProFile::AsyncUpdateDelay delay) -> void
{
  if (!buildConfiguration()->isActive()) {
    TRACE("skipped, not active")
    return;
  }

  const auto interval = qMin(parseDelay(), delay == QmakeProFile::ParseLater ? UPDATE_INTERVAL : 0);
  TRACE("interval: " << interval);
  requestParseWithCustomDelay(interval);
}

auto QmakeBuildSystem::incrementPendingEvaluateFutures() -> void
{
  if (m_pendingEvaluateFuturesCount == 0) {
    // The guard actually might already guard the project if this
    // here is the re-start of a previously aborted parse due to e.g.
    // changing build directories while parsing.
    if (!m_guard.guardsProject())
      m_guard = guardParsingRun();
  }
  ++m_pendingEvaluateFuturesCount;
  TRACE("pending inc to: " << m_pendingEvaluateFuturesCount);
  m_asyncUpdateFutureInterface->setProgressRange(m_asyncUpdateFutureInterface->progressMinimum(), m_asyncUpdateFutureInterface->progressMaximum() + 1);
}

auto QmakeBuildSystem::decrementPendingEvaluateFutures() -> void
{
  --m_pendingEvaluateFuturesCount;
  TRACE("pending dec to: " << m_pendingEvaluateFuturesCount);

  if (!rootProFile()) {
    TRACE("closing project");
    return; // We are closing the project!
  }

  m_asyncUpdateFutureInterface->setProgressValue(m_asyncUpdateFutureInterface->progressValue() + 1);
  if (m_pendingEvaluateFuturesCount == 0) {
    // We are done!
    setRootProjectNode(QmakeNodeTreeBuilder::buildTree(this));

    if (!m_rootProFile->validParse())
      m_asyncUpdateFutureInterface->reportCanceled();

    m_asyncUpdateFutureInterface->reportFinished();
    m_asyncUpdateFutureInterface.reset();
    m_cancelEvaluate = false;

    // TODO clear the profile cache ?
    if (m_asyncUpdateState == AsyncFullUpdatePending || m_asyncUpdateState == AsyncPartialUpdatePending) {
      // Already parsing!
      rootProFile()->setParseInProgressRecursive(true);
      startAsyncTimer(QmakeProFile::ParseLater);
    } else if (m_asyncUpdateState != ShuttingDown) {
      // After being done, we need to call:

      m_asyncUpdateState = Base;
      updateBuildSystemData();
      updateCodeModels();
      updateDocuments();
      target()->updateDefaultDeployConfigurations();
      m_guard.markAsSuccess(); // Qmake always returns (some) data, even when it failed:-)
      TRACE("success" << int(m_guard.isSuccess()));
      m_guard = {}; // This triggers emitParsingFinished by destroying the previous guard.

      m_firstParseNeeded = false;
      TRACE("first parse succeeded");

      emitBuildSystemUpdated();
    }
  }
}

auto QmakeBuildSystem::wasEvaluateCanceled() -> bool
{
  return m_cancelEvaluate;
}

auto QmakeBuildSystem::asyncUpdate() -> void
{
  TaskHub::clearTasks(ProjectExplorer::Constants::TASK_CATEGORY_BUILDSYSTEM);
  setParseDelay(UPDATE_INTERVAL);
  TRACE("");

  if (m_invalidateQmakeVfsContents) {
    m_invalidateQmakeVfsContents = false;
    m_qmakeVfs->invalidateContents();
  } else {
    m_qmakeVfs->invalidateCache();
  }

  m_asyncUpdateFutureInterface.reset(new QFutureInterface<void>);
  m_asyncUpdateFutureInterface->setProgressRange(0, 0);
  Core::ProgressManager::addTask(m_asyncUpdateFutureInterface->future(), tr("Reading Project \"%1\"").arg(project()->displayName()), Constants::PROFILE_EVALUATE);

  m_asyncUpdateFutureInterface->reportStarted();
  const auto watcher = new QFutureWatcher<void>(this);
  connect(watcher, &QFutureWatcher<void>::canceled, this, [this, watcher] {
    if (!m_qmakeGlobals)
      return;
    m_qmakeGlobals->killProcesses();
    watcher->disconnect();
    watcher->deleteLater();
  });
  connect(watcher, &QFutureWatcher<void>::finished, this, [watcher] {
    watcher->disconnect();
    watcher->deleteLater();
  });
  watcher->setFuture(m_asyncUpdateFutureInterface->future());

  const Kit *const k = kit();
  const auto qtVersion = QtSupport::QtKitAspect::qtVersion(k);
  if (!qtVersion || !qtVersion->isValid()) {
    const auto errorMessage = k ? tr("Cannot parse project \"%1\": The currently selected kit \"%2\" does not " "have a valid Qt.").arg(project()->displayName(), k->displayName()) : tr("Cannot parse project \"%1\": No kit selected.").arg(project()->displayName());
    proFileParseError(errorMessage, project()->projectFilePath());
    m_asyncUpdateFutureInterface->reportCanceled();
    m_asyncUpdateFutureInterface->reportFinished();
    m_asyncUpdateFutureInterface.reset();
    return;
  }

  // Make sure we ignore requests for re-evaluation for files whose QmakePriFile objects
  // will get deleted during the parse.
  const auto docUpdater = [](Core::IDocument *doc) {
    static_cast<QmakePriFileDocument*>(doc)->setPriFile(nullptr);
  };
  if (m_asyncUpdateState != AsyncFullUpdatePending) {
    QSet<FilePath> projectFilePaths;
    for (const auto file : qAsConst(m_partialEvaluate)) {
      auto priFiles = file->children();
      for (auto i = 0; i < priFiles.count(); ++i) {
        const QmakePriFile *const priFile = priFiles.at(i);
        projectFilePaths << priFile->filePath();
        priFiles << priFile->children();
      }
    }
    project()->updateExtraProjectFiles(projectFilePaths, docUpdater);
  }

  if (m_asyncUpdateState == AsyncFullUpdatePending) {
    project()->updateExtraProjectFiles(docUpdater);
    rootProFile()->asyncUpdate();
  } else {
    foreach(QmakeProFile *file, m_partialEvaluate)
      file->asyncUpdate();
  }

  m_partialEvaluate.clear();
  m_asyncUpdateState = AsyncUpdateInProgress;
}

auto QmakeBuildSystem::buildFinished(bool success) -> void
{
  if (success)
    m_invalidateQmakeVfsContents = true;
}

auto QmakeProject::projectIssues(const Kit *k) const -> Tasks
{
  auto result = Project::projectIssues(k);
  const QtSupport::QtVersion *const qtFromKit = QtSupport::QtKitAspect::qtVersion(k);
  if (!qtFromKit)
    result.append(createProjectTask(Task::TaskType::Error, tr("No Qt version set in kit.")));
  else if (!qtFromKit->isValid())
    result.append(createProjectTask(Task::TaskType::Error, tr("Qt version is invalid.")));
  if (!ToolChainKitAspect::cxxToolChain(k))
    result.append(createProjectTask(Task::TaskType::Error, tr("No C++ compiler set in kit.")));

  // A project can be considered part of more than one Qt version, for instance if it is an
  // example shipped via the installer.
  // Report a problem if and only if the project is considered to be part of *only* a Qt
  // that is not the one from the current kit.
  const auto qtsContainingThisProject = QtVersionManager::versions([filePath = projectFilePath()](const QtVersion *qt) {
    return qt->isValid() && qt->isQtSubProject(filePath);
  });
  if (!qtsContainingThisProject.isEmpty() && !qtsContainingThisProject.contains(const_cast<QtVersion*>(qtFromKit))) {
    result.append(CompileTask(Task::Warning, tr("Project is part of Qt sources that do not match " "the Qt defined in the kit.")));
  }

  return result;
}

// Find the folder that contains a file with a certain name (recurse down)
static auto folderOf(FolderNode *in, const FilePath &fileName) -> FolderNode*
{
  foreach(FileNode *fn, in->fileNodes()) if (fn->filePath() == fileName)
    return in;
  foreach(FolderNode *folder, in->folderNodes()) if (auto pn = folderOf(folder, fileName))
    return pn;
  return nullptr;
}

// Find the QmakeProFileNode that contains a certain file.
// First recurse down to folder, then find the pro-file.
static auto fileNodeOf(FolderNode *in, const FilePath &fileName) -> FileNode*
{
  for (auto folder = folderOf(in, fileName); folder; folder = folder->parentFolderNode()) {
    if (auto *proFile = dynamic_cast<QmakeProFileNode*>(folder)) {
      foreach(FileNode *fileNode, proFile->fileNodes()) {
        if (fileNode->filePath() == fileName)
          return fileNode;
      }
    }
  }
  return nullptr;
}

auto QmakeBuildSystem::buildDir(const FilePath &proFilePath) const -> FilePath
{
  const auto srcDirRoot = QDir(projectDirectory().toString());
  const auto relativeDir = srcDirRoot.relativeFilePath(proFilePath.parentDir().toString());
  const auto buildConfigBuildDir = buildConfiguration()->buildDirectory();
  auto buildDir = buildConfigBuildDir.isEmpty() ? projectDirectory() : buildConfigBuildDir;
  // FIXME: Convoluted.
  buildDir.setPath(QDir::cleanPath(QDir(buildDir.path()).absoluteFilePath(relativeDir)));
  return buildDir;
}

auto QmakeBuildSystem::proFileParseError(const QString &errorMessage, const FilePath &filePath) -> void
{
  TaskHub::addTask(BuildSystemTask(Task::Error, errorMessage, filePath));
}

auto QmakeBuildSystem::createProFileReader(const QmakeProFile *qmakeProFile) -> QtSupport::ProFileReader*
{
  if (!m_qmakeGlobals) {
    m_qmakeGlobals = std::make_unique<QMakeGlobals>();
    m_qmakeGlobalsRefCnt = 0;

    QStringList qmakeArgs;

    auto k = kit();
    auto bc = qmakeBuildConfiguration();

    auto env = bc->environment();
    if (auto qs = bc->qmakeStep())
      qmakeArgs = qs->parserArguments();
    else
      qmakeArgs = bc->configCommandLineArguments();

    auto qtVersion = QtSupport::QtKitAspect::qtVersion(k);
    m_qmakeSysroot = SysRootKitAspect::sysRoot(k).toString();

    if (qtVersion && qtVersion->isValid()) {
      m_qmakeGlobals->qmake_abslocation = QDir::cleanPath(qtVersion->qmakeFilePath().toString());
      qtVersion->applyProperties(m_qmakeGlobals.get());
    }
    m_qmakeGlobals->setDirectories(rootProFile()->sourceDir().toString(), buildDir(rootProFile()->filePath()).toString());

    auto eit = env.constBegin(), eend = env.constEnd();
    for (; eit != eend; ++eit)
      m_qmakeGlobals->environment.insert(env.key(eit), env.expandedValueForKey(env.key(eit)));

    m_qmakeGlobals->setCommandLineArguments(buildDir(rootProFile()->filePath()).toString(), qmakeArgs);
    m_qmakeGlobals->runSystemFunction = bc->runSystemFunction();

    QtSupport::ProFileCacheManager::instance()->incRefCount();

    // On ios, qmake is called recursively, and the second call with a different
    // spec.
    // macx-ios-clang just creates supporting makefiles, and to avoid being
    // slow does not evaluate everything, and contains misleading information
    // (that is never used).
    // macx-xcode correctly evaluates the variables and generates the xcodeproject
    // that is actually used to build the application.
    //
    // It is important to override the spec file only for the creator evaluator,
    // and not the qmake buildstep used to build the app (as we use the makefiles).
    const char IOSQT[] = "Qt4ProjectManager.QtVersion.Ios"; // from Ios::Constants
    if (qtVersion && qtVersion->type() == QLatin1String(IOSQT))
      m_qmakeGlobals->xqmakespec = QLatin1String("macx-xcode");
  }
  ++m_qmakeGlobalsRefCnt;

  auto reader = new QtSupport::ProFileReader(m_qmakeGlobals.get(), m_qmakeVfs);

  // FIXME: Currently intentional.
  // Core parts of the ProParser hard-assert on non-local items
  reader->setOutputDir(buildDir(qmakeProFile->filePath()).path());

  return reader;
}

auto QmakeBuildSystem::qmakeGlobals() -> QMakeGlobals*
{
  return m_qmakeGlobals.get();
}

auto QmakeBuildSystem::qmakeVfs() -> QMakeVfs*
{
  return m_qmakeVfs;
}

auto QmakeBuildSystem::qmakeSysroot() -> QString
{
  return m_qmakeSysroot;
}

auto QmakeBuildSystem::destroyProFileReader(QtSupport::ProFileReader *reader) -> void
{
  // The ProFileReader destructor is super expensive (but thread-safe).
  const auto deleteFuture = runAsync(ProjectExplorerPlugin::sharedThreadPool(), QThread::LowestPriority, [reader] { delete reader; });
  onFinished(deleteFuture, this, [this](const QFuture<void> &) {
    if (!--m_qmakeGlobalsRefCnt) {
      deregisterFromCacheManager();
      m_qmakeGlobals.reset();
    }
  });
}

auto QmakeBuildSystem::deregisterFromCacheManager() -> void
{
  auto dir = projectFilePath().toString();
  if (!dir.endsWith(QLatin1Char('/')))
    dir += QLatin1Char('/');
  QtSupport::ProFileCacheManager::instance()->discardFiles(dir, qmakeVfs());
  QtSupport::ProFileCacheManager::instance()->decRefCount();
}

auto QmakeBuildSystem::activeTargetWasChanged(Target *t) -> void
{
  // We are only interested in our own target.
  if (t != target())
    return;

  m_invalidateQmakeVfsContents = true;
  scheduleUpdateAll(QmakeProFile::ParseLater);
}

static auto notifyChangedHelper(const FilePath &fileName, QmakeProFile *file) -> void
{
  if (file->filePath() == fileName) {
    QtSupport::ProFileCacheManager::instance()->discardFile(fileName.toString(), file->buildSystem()->qmakeVfs());
    file->scheduleUpdate(QmakeProFile::ParseNow);
  }

  for (auto fn : file->children()) {
    if (auto pro = dynamic_cast<QmakeProFile*>(fn))
      notifyChangedHelper(fileName, pro);
  }
}

auto QmakeBuildSystem::notifyChanged(const FilePath &name) -> void
{
  auto files = project()->files([&name](const Node *n) {
    return Project::SourceFiles(n) && n->filePath() == name;
  });

  if (files.isEmpty())
    return;

  notifyChangedHelper(name, m_rootProFile.get());
}

auto QmakeBuildSystem::watchFolders(const QStringList &l, QmakePriFile *file) -> void
{
  if (l.isEmpty())
    return;
  if (!m_centralizedFolderWatcher)
    m_centralizedFolderWatcher = new Internal::CentralizedFolderWatcher(this);
  m_centralizedFolderWatcher->watchFolders(l, file);
}

auto QmakeBuildSystem::unwatchFolders(const QStringList &l, QmakePriFile *file) -> void
{
  if (m_centralizedFolderWatcher && !l.isEmpty())
    m_centralizedFolderWatcher->unwatchFolders(l, file);
}

/////////////
/// Centralized Folder Watcher
////////////

// All the folder have a trailing slash!
CentralizedFolderWatcher::CentralizedFolderWatcher(QmakeBuildSystem *parent) : QObject(parent), m_buildSystem(parent)
{
  m_compressTimer.setSingleShot(true);
  m_compressTimer.setInterval(200);
  connect(&m_compressTimer, &QTimer::timeout, this, &CentralizedFolderWatcher::onTimer);
  connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &CentralizedFolderWatcher::folderChanged);
}

auto CentralizedFolderWatcher::recursiveDirs(const QString &folder) -> QSet<QString>
{
  QSet<QString> result;
  QDir dir(folder);
  auto list = dir.entryList(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot);
  foreach(const QString &f, list) {
    const QString a = folder + f + QLatin1Char('/');
    result.insert(a);
    result += recursiveDirs(a);
  }
  return result;
}

auto CentralizedFolderWatcher::watchFolders(const QList<QString> &folders, QmakePriFile *file) -> void
{
  m_watcher.addPaths(folders);

  const QChar slash = QLatin1Char('/');
  foreach(const QString &f, folders) {
    auto folder = f;
    if (!folder.endsWith(slash))
      folder.append(slash);
    m_map.insert(folder, file);

    // Support for recursive watching
    // we add the recursive directories we find
    auto tmp = recursiveDirs(folder);
    if (!tmp.isEmpty())
      m_watcher.addPaths(Utils::toList(tmp));
    m_recursiveWatchedFolders += tmp;
  }
}

auto CentralizedFolderWatcher::unwatchFolders(const QList<QString> &folders, QmakePriFile *file) -> void
{
  const QChar slash = QLatin1Char('/');
  foreach(const QString &f, folders) {
    auto folder = f;
    if (!folder.endsWith(slash))
      folder.append(slash);
    m_map.remove(folder, file);
    if (!m_map.contains(folder))
      m_watcher.removePath(folder);

    // Figure out which recursive directories we can remove
    // this might not scale. I'm pretty sure it doesn't
    // A scaling implementation would need to save more information
    // where a given directory watcher actual comes from...

    QStringList toRemove;
    foreach(const QString &rwf, m_recursiveWatchedFolders) {
      if (rwf.startsWith(folder)) {
        // So the rwf is a subdirectory of a folder we aren't watching
        // but maybe someone else wants us to watch
        auto needToWatch = false;
        auto end = m_map.constEnd();
        for (auto it = m_map.constBegin(); it != end; ++it) {
          if (rwf.startsWith(it.key())) {
            needToWatch = true;
            break;
          }
        }
        if (!needToWatch) {
          m_watcher.removePath(rwf);
          toRemove << rwf;
        }
      }
    }

    foreach(const QString &tr, toRemove)
      m_recursiveWatchedFolders.remove(tr);
  }
}

auto CentralizedFolderWatcher::folderChanged(const QString &folder) -> void
{
  m_changedFolders.insert(folder);
  m_compressTimer.start();
}

auto CentralizedFolderWatcher::onTimer() -> void
{
  foreach(const QString &folder, m_changedFolders)
    delayedFolderChanged(folder);
  m_changedFolders.clear();
}

auto CentralizedFolderWatcher::delayedFolderChanged(const QString &folder) -> void
{
  // Figure out whom to inform
  auto dir = folder;
  const QChar slash = QLatin1Char('/');
  auto newOrRemovedFiles = false;
  while (true) {
    if (!dir.endsWith(slash))
      dir.append(slash);
    auto files = m_map.values(dir);
    if (!files.isEmpty()) {
      // Collect all the files
      QSet<FilePath> newFiles;
      newFiles += QmakePriFile::recursiveEnumerate(folder);
      foreach(QmakePriFile *file, files)
        newOrRemovedFiles = newOrRemovedFiles || file->folderChanged(folder, newFiles);
    }

    // Chop off last part, and break if there's nothing to chop off
    //
    if (dir.length() < 2)
      break;

    // We start before the last slash
    const int index = dir.lastIndexOf(slash, dir.length() - 2);
    if (index == -1)
      break;
    dir.truncate(index + 1);
  }

  auto folderWithSlash = folder;
  if (!folder.endsWith(slash))
    folderWithSlash.append(slash);

  // If a subdirectory was added, watch it too
  auto tmp = recursiveDirs(folderWithSlash);
  if (!tmp.isEmpty()) {
    auto alreadyAdded = Utils::toSet(m_watcher.directories());
    tmp.subtract(alreadyAdded);
    if (!tmp.isEmpty())
      m_watcher.addPaths(Utils::toList(tmp));
    m_recursiveWatchedFolders += tmp;
  }

  if (newOrRemovedFiles)
    m_buildSystem->updateCodeModels();
}

auto QmakeProject::configureAsExampleProject(Kit *kit) -> void
{
  QList<BuildInfo> infoList;
  const auto kits(kit != nullptr ? QList<Kit*>({kit}) : KitManager::kits());
  for (auto k : kits) {
    if (QtSupport::QtKitAspect::qtVersion(k) != nullptr) {
      if (auto factory = BuildConfigurationFactory::find(k, projectFilePath()))
        infoList << factory->allAvailableSetups(k, projectFilePath());
    }
  }
  setup(infoList);
}

auto QmakeBuildSystem::updateBuildSystemData() -> void
{
  const QmakeProFile *const file = rootProFile();
  if (!file || file->parseInProgress())
    return;

  DeploymentData deploymentData;
  collectData(file, deploymentData);
  setDeploymentData(deploymentData);

  QList<BuildTargetInfo> appTargetList;

  project()->rootProjectNode()->forEachProjectNode([this, &appTargetList](const ProjectNode *pn) {
    auto node = dynamic_cast<const QmakeProFileNode*>(pn);
    if (!node || !node->includedInExactParse())
      return;

    if (node->projectType() != ProjectType::ApplicationTemplate && node->projectType() != ProjectType::ScriptTemplate)
      return;

    auto ti = node->targetInformation();
    if (!ti.valid)
      return;

    const auto &config = node->variableValue(Variable::Config);

    auto destDir = ti.destDir;
    FilePath workingDir;
    if (!destDir.isEmpty()) {
      auto workingDirIsBaseDir = false;
      if (destDir.path() == ti.buildTarget)
        workingDirIsBaseDir = true;
      if (QDir::isRelativePath(destDir.path()))
        destDir = ti.buildDir / destDir.path();

      if (workingDirIsBaseDir)
        workingDir = ti.buildDir;
      else
        workingDir = destDir;
    } else {
      workingDir = ti.buildDir;
    }

    if (HostOsInfo::isMacHost() && config.contains("app_bundle"))
      workingDir = workingDir / (ti.target + ".app/Contents/MacOS");

    BuildTargetInfo bti;
    bti.targetFilePath = executableFor(node->proFile());
    bti.projectFilePath = node->filePath();
    bti.workingDirectory = workingDir;
    bti.displayName = bti.projectFilePath.completeBaseName();
    const auto relativePathInProject = bti.projectFilePath.relativeChildPath(projectDirectory());
    if (!relativePathInProject.isEmpty()) {
      bti.displayNameUniquifier = QString::fromLatin1(" (%1)").arg(relativePathInProject.toUserOutput());
    }
    bti.buildKey = bti.projectFilePath.toString();
    bti.isQtcRunnable = config.contains("qtc_runnable");

    if (config.contains("console") && !config.contains("testcase")) {
      const auto qt = node->variableValue(Variable::Qt);
      bti.usesTerminal = !qt.contains("testlib") && !qt.contains("qmltest");
    }

    FilePaths libraryPaths;

    // The user could be linking to a library found via a -L/some/dir switch
    // to find those libraries while actually running we explicitly prepend those
    // dirs to the library search path
    const auto libDirectories = node->variableValue(Variable::LibDirectories);
    if (!libDirectories.isEmpty()) {
      auto proFile = node->proFile();
      QTC_ASSERT(proFile, return);
      const auto proDirectory = buildDir(proFile->filePath()).toString();
      for (auto dir : libDirectories) {
        // Fix up relative entries like "LIBS+=-L.."
        const QFileInfo fi(dir);
        if (!fi.isAbsolute())
          dir = QDir::cleanPath(proDirectory + '/' + dir);
        libraryPaths.append(FilePath::fromUserInput(dir));
      }
    }
    auto qtVersion = QtSupport::QtKitAspect::qtVersion(kit());
    if (qtVersion)
      libraryPaths.append(qtVersion->librarySearchPath());

    bti.runEnvModifierHash = qHash(libraryPaths);
    bti.runEnvModifier = [libraryPaths](Environment &env, bool useLibrarySearchPath) {
      if (useLibrarySearchPath)
        env.prependOrSetLibrarySearchPaths(libraryPaths);
    };

    appTargetList.append(bti);
  });

  setApplicationTargets(appTargetList);
}

auto QmakeBuildSystem::collectData(const QmakeProFile *file, DeploymentData &deploymentData) -> void
{
  if (!file->isSubProjectDeployable(file->filePath()))
    return;

  const auto &installsList = file->installsList();
  for (const auto &item : installsList.items) {
    if (!item.active)
      continue;
    for (const auto &localFile : item.files) {
      deploymentData.addFile(FilePath::fromString(localFile.fileName), item.path, item.executable ? DeployableFile::TypeExecutable : DeployableFile::TypeNormal);
    }
  }

  switch (file->projectType()) {
  case ProjectType::ApplicationTemplate:
    if (!installsList.targetPath.isEmpty())
      collectApplicationData(file, deploymentData);
    break;
  case ProjectType::SharedLibraryTemplate:
  case ProjectType::StaticLibraryTemplate:
    collectLibraryData(file, deploymentData);
    break;
  case ProjectType::SubDirsTemplate:
    for (const QmakePriFile *const subPriFile : file->subPriFilesExact()) {
      auto subProFile = dynamic_cast<const QmakeProFile*>(subPriFile);
      if (subProFile)
        collectData(subProFile, deploymentData);
    }
    break;
  default:
    break;
  }
}

auto QmakeBuildSystem::collectApplicationData(const QmakeProFile *file, DeploymentData &deploymentData) -> void
{
  const auto executable = executableFor(file);
  if (!executable.isEmpty())
    deploymentData.addFile(executable, file->installsList().targetPath, DeployableFile::TypeExecutable);
}

static auto destDirFor(const TargetInformation &ti) -> FilePath
{
  if (ti.destDir.isEmpty())
    return ti.buildDir;
  if (QDir::isRelativePath(ti.destDir.path()))
    return ti.buildDir / ti.destDir.path();
  return ti.destDir;
}

auto QmakeBuildSystem::collectLibraryData(const QmakeProFile *file, DeploymentData &deploymentData) -> void
{
  const auto targetPath = file->installsList().targetPath;
  if (targetPath.isEmpty())
    return;
  const ToolChain *const toolchain = ToolChainKitAspect::cxxToolChain(kit());
  if (!toolchain)
    return;

  auto ti = file->targetInformation();
  auto targetFileName = ti.target;
  const auto config = file->variableValue(Variable::Config);
  const auto isStatic = config.contains(QLatin1String("static"));
  const auto isPlugin = config.contains(QLatin1String("plugin"));
  const auto nameIsVersioned = !isPlugin && !config.contains("unversioned_libname");
  switch (toolchain->targetAbi().os()) {
  case Abi::WindowsOS: {
    auto targetVersionExt = file->singleVariableValue(Variable::TargetVersionExt);
    if (targetVersionExt.isEmpty()) {
      const auto version = file->singleVariableValue(Variable::Version);
      if (!version.isEmpty()) {
        targetVersionExt = version.left(version.indexOf(QLatin1Char('.')));
        if (targetVersionExt == QLatin1String("0"))
          targetVersionExt.clear();
      }
    }
    targetFileName += targetVersionExt + QLatin1Char('.');
    targetFileName += QLatin1String(isStatic ? "lib" : "dll");
    deploymentData.addFile(destDirFor(ti) / targetFileName, targetPath);
    break;
  }
  case Abi::DarwinOS: {
    auto destDir = destDirFor(ti);
    if (config.contains(QLatin1String("lib_bundle"))) {
      destDir = destDir.pathAppended(ti.target + ".framework");
    } else {
      if (!(isPlugin && config.contains(QLatin1String("no_plugin_name_prefix"))))
        targetFileName.prepend(QLatin1String("lib"));

      if (nameIsVersioned) {
        targetFileName += QLatin1Char('.');
        const auto version = file->singleVariableValue(Variable::Version);
        auto majorVersion = version.left(version.indexOf(QLatin1Char('.')));
        if (majorVersion.isEmpty())
          majorVersion = QLatin1String("1");
        targetFileName += majorVersion;
      }
      targetFileName += QLatin1Char('.');
      targetFileName += file->singleVariableValue(isStatic ? Variable::StaticLibExtension : Variable::ShLibExtension);
    }
    deploymentData.addFile(destDir / targetFileName, targetPath);
    break;
  }
  case Abi::LinuxOS:
  case Abi::BsdOS:
  case Abi::QnxOS:
  case Abi::UnixOS:
    if (!(isPlugin && config.contains(QLatin1String("no_plugin_name_prefix"))))
      targetFileName.prepend(QLatin1String("lib"));

    targetFileName += QLatin1Char('.');
    if (isStatic) {
      targetFileName += QLatin1Char('a');
    } else {
      targetFileName += QLatin1String("so");
      deploymentData.addFile(destDirFor(ti) / targetFileName, targetPath);
      if (nameIsVersioned) {
        auto version = file->singleVariableValue(Variable::Version);
        if (version.isEmpty())
          version = QLatin1String("1.0.0");
        auto versionComponents = version.split('.');
        while (versionComponents.size() < 3)
          versionComponents << QLatin1String("0");
        targetFileName += QLatin1Char('.');
        while (!versionComponents.isEmpty()) {
          const auto versionString = versionComponents.join(QLatin1Char('.'));
          deploymentData.addFile(destDirFor(ti) / targetFileName + versionString, targetPath);
          versionComponents.removeLast();
        }
      }
    }
    break;
  default:
    break;
  }
}

static auto getFullPathOf(const QmakeProFile *pro, Variable variable, const BuildConfiguration *bc) -> Utils::FilePath
{
  // Take last non-flag value, to cover e.g. '@echo $< && $$QMAKE_CC' or 'ccache gcc'
  const auto values = Utils::filtered(pro->variableValue(variable), [](const QString &value) {
    return !value.startsWith('-');
  });
  if (values.isEmpty())
    return Utils::FilePath();
  const auto exe = values.last();
  QTC_ASSERT(bc, return Utils::FilePath::fromUserInput(exe));
  QFileInfo fi(exe);
  if (fi.isAbsolute())
    return Utils::FilePath::fromUserInput(exe);

  return bc->environment().searchInPath(exe);
}

auto QmakeBuildSystem::testToolChain(ToolChain *tc, const FilePath &path) const -> void
{
  if (!tc || path.isEmpty())
    return;

  const auto expected = tc->compilerCommand();
  auto env = buildConfiguration()->environment();

  if (env.isSameExecutable(path.toString(), expected.toString()))
    return;
  const auto pair = qMakePair(expected, path);
  if (m_toolChainWarnings.contains(pair))
    return;
  // Suppress warnings on Apple machines where compilers in /usr/bin point into Xcode.
  // This will suppress some valid warnings, but avoids annoying Apple users with
  // spurious warnings all the time!
  if (pair.first.path().startsWith("/usr/bin/") && pair.second.path().contains("/Contents/Developer/Toolchains/")) {
    return;
  }
  TaskHub::addTask(BuildSystemTask(Task::Warning, QCoreApplication::translate("QmakeProjectManager", "\"%1\" is used by qmake, but \"%2\" is configured in the kit.\n" "Please update your kit (%3) or choose a mkspec for qmake that matches " "your target environment better.").arg(path.toUserOutput()).arg(expected.toUserOutput()).arg(kit()->displayName())));
  m_toolChainWarnings.insert(pair);
}

auto QmakeBuildSystem::warnOnToolChainMismatch(const QmakeProFile *pro) const -> void
{
  const BuildConfiguration *bc = buildConfiguration();
  testToolChain(ToolChainKitAspect::cToolChain(kit()), getFullPathOf(pro, Variable::QmakeCc, bc));
  testToolChain(ToolChainKitAspect::cxxToolChain(kit()), getFullPathOf(pro, Variable::QmakeCxx, bc));
}

auto QmakeBuildSystem::executableFor(const QmakeProFile *file) -> FilePath
{
  const ToolChain *const tc = ToolChainKitAspect::cxxToolChain(kit());
  if (!tc)
    return {};

  auto ti = file->targetInformation();
  QString target;

  QTC_ASSERT(file, return {});

  if (tc->targetAbi().os() == Abi::DarwinOS && file->variableValue(Variable::Config).contains("app_bundle")) {
    target = ti.target + ".app/Contents/MacOS/" + ti.target;
  } else {
    const auto extension = file->singleVariableValue(Variable::TargetExt);
    if (extension.isEmpty())
      target = OsSpecificAspects::withExecutableSuffix(Abi::abiOsToOsType(tc->targetAbi().os()), ti.target);
    else
      target = ti.target + extension;
  }
  return (destDirFor(ti) / target).absoluteFilePath();
}

auto QmakeProject::projectImporter() const -> ProjectImporter*
{
  if (!m_projectImporter)
    m_projectImporter = new QmakeProjectImporter(projectFilePath());
  return m_projectImporter;
}

auto QmakeBuildSystem::asyncUpdateState() const -> QmakeBuildSystem::AsyncUpdateState
{
  return m_asyncUpdateState;
}

auto QmakeBuildSystem::rootProFile() const -> QmakeProFile*
{
  return m_rootProFile.get();
}

auto QmakeBuildSystem::triggerParsing() -> void
{
  asyncUpdate();
}

auto QmakeBuildSystem::filesGeneratedFrom(const FilePath &input) const -> FilePaths
{
  if (!project()->rootProjectNode())
    return {};

  if (const FileNode *file = fileNodeOf(project()->rootProjectNode(), input)) {
    const QmakeProFileNode *pro = dynamic_cast<QmakeProFileNode*>(file->parentFolderNode());
    QTC_ASSERT(pro, return {});
    if (const QmakeProFile *proFile = pro->proFile())
      return proFile->generatedFiles(buildDir(pro->filePath()), file->filePath(), file->fileType());
  }
  return {};
}

auto QmakeBuildSystem::additionalData(Utils::Id id) const -> QVariant
{
  if (id == "QmlDesignerImportPath")
    return m_rootProFile->variableValue(Variable::QmlDesignerImportPath);
  return BuildSystem::additionalData(id);
}

auto QmakeBuildSystem::buildHelper(Action action, bool isFileBuild, QmakeProFileNode *profile, FileNode *buildableFile) -> void
{
  auto bc = qmakeBuildConfiguration();

  if (!profile || !buildableFile)
    isFileBuild = false;

  if (profile) {
    if (profile != project()->rootProjectNode() || isFileBuild)
      bc->setSubNodeBuild(profile->proFileNode());
  }

  if (isFileBuild)
    bc->setFileNodeBuild(buildableFile);
  if (ProjectExplorerPlugin::saveModifiedFiles()) {
    if (action == BUILD)
      BuildManager::buildList(bc->buildSteps());
    else if (action == CLEAN)
      BuildManager::buildList(bc->cleanSteps());
    else if (action == REBUILD)
      BuildManager::buildLists({bc->cleanSteps(), bc->buildSteps()});
  }

  bc->setSubNodeBuild(nullptr);
  bc->setFileNodeBuild(nullptr);
}

} // QmakeProjectManager

#include "qmakeproject.moc"
