// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "project.hpp"

#include "buildconfiguration.hpp"
#include "buildinfo.hpp"
#include "buildsystem.hpp"
#include "deployconfiguration.hpp"
#include "editorconfiguration.hpp"
#include "kit.hpp"
#include "kitinformation.hpp"
#include "makestep.hpp"
#include "projectexplorer.hpp"
#include "projectnodes.hpp"
#include "runconfiguration.hpp"
#include "runcontrol.hpp"
#include "session.hpp"
#include "target.hpp"
#include "taskhub.hpp"
#include "userfileaccessor.hpp"

#include <core/core-document-interface.hpp>
#include <core/core-document-manager.hpp>
#include <core/core-context-interface.hpp>
#include <core/core-interface.hpp>
#include <core/core-version-control-interface.hpp>
#include <core/core-vcs-manager.hpp>
#include <core/core-document-model.hpp>

#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/kitmanager.hpp>
#include <projectexplorer/projecttree.hpp>

#include <utils/algorithm.hpp>
#include <utils/environment.hpp>
#include <utils/macroexpander.hpp>
#include <utils/pointeralgorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QFileDialog>

#include <limits>

#ifdef WITH_TESTS
#include <core/core-editor-manager.hpp>
#include <utils/temporarydirectory.hpp>

#include <QEventLoop>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#endif

using namespace Utils;
using namespace Orca::Plugin::Core;

namespace ProjectExplorer {

/*!
    \class ProjectExplorer::Project

    \brief The Project class implements a project node in the project explorer.
*/

/*!
   \fn void ProjectExplorer::Project::environmentChanged()

   A convenience signal emitted if activeBuildConfiguration emits
   environmentChanged or if the active build configuration changes
   (including due to the active target changing).
*/

/*!
   \fn void ProjectExplorer::Project::buildConfigurationEnabledChanged()

   A convenience signal emitted if activeBuildConfiguration emits
   isEnabledChanged() or if the active build configuration changes
   (including due to the active target changing).
*/

constexpr char ACTIVE_TARGET_KEY[] = "ProjectExplorer.Project.ActiveTarget";
constexpr char TARGET_KEY_PREFIX[] = "ProjectExplorer.Project.Target.";
constexpr char TARGET_COUNT_KEY[] = "ProjectExplorer.Project.TargetCount";
constexpr char EDITOR_SETTINGS_KEY[] = "ProjectExplorer.Project.EditorSettings";
constexpr char PLUGIN_SETTINGS_KEY[] = "ProjectExplorer.Project.PluginSettings";
constexpr char PROJECT_ENV_KEY[] = "ProjectExplorer.Project.Environment";

static auto isListedFileNode(const Node *node) -> bool
{
  return node->asContainerNode() || node->listInProject();
}

static auto nodeLessThan(const Node *n1, const Node *n2) -> bool
{
  return n1->filePath() < n2->filePath();
}

const Project::NodeMatcher Project::AllFiles = [](const Node *node) {
  return isListedFileNode(node);
};

const Project::NodeMatcher Project::SourceFiles = [](const Node *node) {
  return isListedFileNode(node) && !node->isGenerated();
};

const Project::NodeMatcher Project::GeneratedFiles = [](const Node *node) {
  return isListedFileNode(node) && node->isGenerated();
};

// --------------------------------------------------------------------
// ProjectDocument:
// --------------------------------------------------------------------

class ProjectDocument : public IDocument {
public:
  ProjectDocument(const QString &mimeType, const FilePath &fileName, Project *project) : m_project(project)
  {
    QTC_CHECK(project);

    setFilePath(fileName);
    setMimeType(mimeType);
  }

  auto reloadBehavior(ChangeTrigger state, ChangeType type) const -> ReloadBehavior final
  {
    Q_UNUSED(state)
    Q_UNUSED(type)
    return BehaviorSilent;
  }

  auto reload(QString *errorString, ReloadFlag flag, ChangeType type) -> bool final
  {
    Q_UNUSED(errorString)
    Q_UNUSED(flag)
    Q_UNUSED(type)

    emit m_project->projectFileIsDirty(filePath());
    return true;
  }

private:
  Project *m_project;
};

// -------------------------------------------------------------------------
// Project
// -------------------------------------------------------------------------
class ProjectPrivate {
public:
  ~ProjectPrivate();

  Id m_id;
  bool m_needsInitialExpansion = false;
  bool m_canBuildProducts = false;
  bool m_hasMakeInstallEquivalent = false;
  bool m_needsBuildConfigurations = true;
  bool m_needsDeployConfigurations = true;
  bool m_shuttingDown = false;

  std::function<BuildSystem *(Target *)> m_buildSystemCreator;

  std::unique_ptr<IDocument> m_document;
  std::vector<std::unique_ptr<IDocument>> m_extraProjectDocuments;
  std::unique_ptr<ProjectNode> m_rootProjectNode;
  std::unique_ptr<ContainerNode> m_containerNode;
  std::vector<std::unique_ptr<Target>> m_targets;
  Target *m_activeTarget = nullptr;
  EditorConfiguration m_editorConfiguration;
  Context m_projectLanguages;
  QVariantMap m_pluginSettings;
  std::unique_ptr<Internal::UserFileAccessor> m_accessor;

  QString m_displayName;

  MacroExpander m_macroExpander;
  FilePath m_rootProjectDirectory;
  mutable QVector<const Node*> m_sortedNodeList;

  QVariantMap m_extraData;
};

ProjectPrivate::~ProjectPrivate()
{
  // Make sure our root node is null when deleting the actual node
  auto oldNode = std::move(m_rootProjectNode);
}

Project::Project(const QString &mimeType, const FilePath &fileName) : d(new ProjectPrivate)
{
  d->m_document = std::make_unique<ProjectDocument>(mimeType, fileName, this);
  DocumentManager::addDocument(d->m_document.get());

  d->m_macroExpander.setDisplayName(tr("Project"));
  d->m_macroExpander.registerVariable("Project:Name", tr("Project Name"), [this] { return displayName(); });

  // Only set up containernode after d is set so that it will find the project directory!
  d->m_containerNode = std::make_unique<ContainerNode>(this);
}

Project::~Project()
{
  delete d;
}

auto Project::displayName() const -> QString
{
  return d->m_displayName;
}

auto Project::id() const -> Id
{
  QTC_CHECK(d->m_id.isValid());
  return d->m_id;
}

auto Project::markAsShuttingDown() -> void
{
  d->m_shuttingDown = true;
}

auto Project::isShuttingDown() const -> bool
{
  return d->m_shuttingDown;
}

auto Project::mimeType() const -> QString
{
  return d->m_document->mimeType();
}

auto Project::canBuildProducts() const -> bool
{
  return d->m_canBuildProducts;
}

auto Project::createBuildSystem(Target *target) const -> BuildSystem*
{
  return d->m_buildSystemCreator ? d->m_buildSystemCreator(target) : nullptr;
}

auto Project::projectFilePath() const -> FilePath
{
  QTC_ASSERT(d->m_document, return {});
  return d->m_document->filePath();
}

auto Project::addTarget(std::unique_ptr<Target> &&t) -> void
{
  const auto pointer = t.get();
  QTC_ASSERT(t && !contains(d->m_targets, pointer), return);
  QTC_ASSERT(!target(t->kit()), return);
  Q_ASSERT(t->project() == this);

  // add it
  d->m_targets.emplace_back(std::move(t));
  emit addedTarget(pointer);

  // check activeTarget:
  if (!activeTarget())
    SessionManager::setActiveTarget(this, pointer, SetActive::Cascade);
}

auto Project::addTargetForDefaultKit() -> Target*
{
  return addTargetForKit(KitManager::defaultKit());
}

auto Project::addTargetForKit(Kit *kit) -> Target*
{
  if (!kit || target(kit))
    return nullptr;

  auto t = std::make_unique<Target>(this, kit, Target::_constructor_tag{});
  const auto pointer = t.get();

  if (!setupTarget(pointer))
    return {};

  addTarget(std::move(t));

  return pointer;
}

auto Project::removeTarget(Target *target) -> bool
{
  QTC_ASSERT(target && contains(d->m_targets, target), return false);

  if (BuildManager::isBuilding(target))
    return false;

  target->markAsShuttingDown();
  emit aboutToRemoveTarget(target);
  auto keep = take(d->m_targets, target);
  if (target == d->m_activeTarget) {
    const auto newActiveTarget = (d->m_targets.size() == 0 ? nullptr : d->m_targets.at(0).get());
    SessionManager::setActiveTarget(this, newActiveTarget, SetActive::Cascade);
  }
  emit removedTarget(target);

  return true;
}

auto Project::targets() const -> const QList<Target*>
{
  return toRawPointer<QList>(d->m_targets);
}

auto Project::activeTarget() const -> Target*
{
  return d->m_activeTarget;
}

auto Project::setActiveTarget(Target *target) -> void
{
  if (d->m_activeTarget == target)
    return;

  // Allow to set nullptr just before the last target is removed or when no target exists.
  if ((!target && d->m_targets.size() == 0) || (target && contains(d->m_targets, target))) {
    d->m_activeTarget = target;
    emit activeTargetChanged(d->m_activeTarget);
    ProjectExplorerPlugin::updateActions();
  }
}

auto Project::needsInitialExpansion() const -> bool
{
  return d->m_needsInitialExpansion;
}

auto Project::setNeedsInitialExpansion(bool needsExpansion) -> void
{
  d->m_needsInitialExpansion = needsExpansion;
}

auto Project::setExtraProjectFiles(const QSet<FilePath> &projectDocumentPaths, const DocGenerator &docGenerator, const DocUpdater &docUpdater) -> void
{
  auto uniqueNewFiles = projectDocumentPaths;
  uniqueNewFiles.remove(projectFilePath()); // Make sure to never add the main project file!

  const auto existingWatches = transform<QSet>(d->m_extraProjectDocuments, &IDocument::filePath);

  const auto toAdd = uniqueNewFiles - existingWatches;
  const auto toRemove = existingWatches - uniqueNewFiles;

  Utils::erase(d->m_extraProjectDocuments, [&toRemove](const std::unique_ptr<IDocument> &d) {
    return toRemove.contains(d->filePath());
  });
  if (docUpdater) {
    for (const auto &doc : qAsConst(d->m_extraProjectDocuments))
      docUpdater(doc.get());
  }
  QList<IDocument*> toRegister;
  for (const auto &p : toAdd) {
    if (docGenerator) {
      auto doc = docGenerator(p);
      QTC_ASSERT(doc, continue);
      d->m_extraProjectDocuments.push_back(std::move(doc));
    } else {
      auto document = std::make_unique<ProjectDocument>(d->m_document->mimeType(), p, this);
      toRegister.append(document.get());
      d->m_extraProjectDocuments.emplace_back(std::move(document));
    }
  }
  DocumentManager::addDocuments(toRegister);
}

auto Project::updateExtraProjectFiles(const QSet<FilePath> &projectDocumentPaths, const DocUpdater &docUpdater) -> void
{
  for (const auto &fp : projectDocumentPaths) {
    for (const auto &doc : d->m_extraProjectDocuments) {
      if (doc->filePath() == fp) {
        docUpdater(doc.get());
        break;
      }
    }
  }
}

auto Project::updateExtraProjectFiles(const DocUpdater &docUpdater) -> void
{
  for (const auto &doc : qAsConst(d->m_extraProjectDocuments))
    docUpdater(doc.get());
}

auto Project::target(Id id) const -> Target*
{
  return findOrDefault(d->m_targets, equal(&Target::id, id));
}

auto Project::target(Kit *k) const -> Target*
{
  return findOrDefault(d->m_targets, equal(&Target::kit, k));
}

auto Project::projectIssues(const Kit *k) const -> Tasks
{
  Tasks result;
  if (!k->isValid())
    result.append(createProjectTask(Task::TaskType::Error, tr("Kit is not valid.")));
  return {};
}

auto Project::copySteps(Target *sourceTarget, Target *newTarget) -> bool
{
  QTC_ASSERT(newTarget, return false);
  auto fatalError = false;
  QStringList buildconfigurationError;
  QStringList deployconfigurationError;
  QStringList runconfigurationError;

  const Project *const project = newTarget->project();
  for (const auto sourceBc : sourceTarget->buildConfigurations()) {
    const auto newBc = BuildConfigurationFactory::clone(newTarget, sourceBc);
    if (!newBc) {
      buildconfigurationError << sourceBc->displayName();
      continue;
    }
    newBc->setDisplayName(sourceBc->displayName());
    newBc->setBuildDirectory(BuildConfiguration::buildDirectoryFromTemplate(project->projectDirectory(), project->projectFilePath(), project->displayName(), newTarget->kit(), sourceBc->displayName(), sourceBc->buildType()));
    newTarget->addBuildConfiguration(newBc);
    if (sourceTarget->activeBuildConfiguration() == sourceBc)
      SessionManager::setActiveBuildConfiguration(newTarget, newBc, SetActive::NoCascade);
  }
  if (!newTarget->activeBuildConfiguration()) {
    auto bcs = newTarget->buildConfigurations();
    if (!bcs.isEmpty())
      SessionManager::setActiveBuildConfiguration(newTarget, bcs.first(), SetActive::NoCascade);
  }

  for (const auto sourceDc : sourceTarget->deployConfigurations()) {
    const auto newDc = DeployConfigurationFactory::clone(newTarget, sourceDc);
    if (!newDc) {
      deployconfigurationError << sourceDc->displayName();
      continue;
    }
    newDc->setDisplayName(sourceDc->displayName());
    newTarget->addDeployConfiguration(newDc);
    if (sourceTarget->activeDeployConfiguration() == sourceDc)
      SessionManager::setActiveDeployConfiguration(newTarget, newDc, SetActive::NoCascade);
  }
  if (!newTarget->activeBuildConfiguration()) {
    auto dcs = newTarget->deployConfigurations();
    if (!dcs.isEmpty())
      SessionManager::setActiveDeployConfiguration(newTarget, dcs.first(), SetActive::NoCascade);
  }

  for (const auto sourceRc : sourceTarget->runConfigurations()) {
    const auto newRc = RunConfigurationFactory::clone(newTarget, sourceRc);
    if (!newRc) {
      runconfigurationError << sourceRc->displayName();
      continue;
    }
    newRc->setDisplayName(sourceRc->displayName());
    newTarget->addRunConfiguration(newRc);
    if (sourceTarget->activeRunConfiguration() == sourceRc)
      newTarget->setActiveRunConfiguration(newRc);
  }
  if (!newTarget->activeRunConfiguration()) {
    auto rcs = newTarget->runConfigurations();
    if (!rcs.isEmpty())
      newTarget->setActiveRunConfiguration(rcs.first());
  }

  if (buildconfigurationError.count() == sourceTarget->buildConfigurations().count())
    fatalError = true;

  if (deployconfigurationError.count() == sourceTarget->deployConfigurations().count())
    fatalError = true;

  if (runconfigurationError.count() == sourceTarget->runConfigurations().count())
    fatalError = true;

  if (fatalError) {
    // That could be a more granular error message
    QMessageBox::critical(ICore::dialogParent(), tr("Incompatible Kit"), tr("Kit %1 is incompatible with kit %2.").arg(sourceTarget->kit()->displayName()).arg(newTarget->kit()->displayName()));
  } else if (!buildconfigurationError.isEmpty() || !deployconfigurationError.isEmpty() || ! runconfigurationError.isEmpty()) {

    QString error;
    if (!buildconfigurationError.isEmpty())
      error += tr("Build configurations:") + QLatin1Char('\n') + buildconfigurationError.join(QLatin1Char('\n'));

    if (!deployconfigurationError.isEmpty()) {
      if (!error.isEmpty())
        error.append(QLatin1Char('\n'));
      error += tr("Deploy configurations:") + QLatin1Char('\n') + deployconfigurationError.join(QLatin1Char('\n'));
    }

    if (!runconfigurationError.isEmpty()) {
      if (!error.isEmpty())
        error.append(QLatin1Char('\n'));
      error += tr("Run configurations:") + QLatin1Char('\n') + runconfigurationError.join(QLatin1Char('\n'));
    }

    QMessageBox msgBox(ICore::dialogParent());
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(tr("Partially Incompatible Kit"));
    msgBox.setText(tr("Some configurations could not be copied."));
    msgBox.setDetailedText(error);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    fatalError = msgBox.exec() != QDialog::Accepted;
  }

  return !fatalError;
}

auto Project::setupTarget(Target *t) -> bool
{
  if (d->m_needsBuildConfigurations)
    t->updateDefaultBuildConfigurations();
  if (d->m_needsDeployConfigurations)
    t->updateDefaultDeployConfigurations();
  t->updateDefaultRunConfigurations();
  return true;
}

auto Project::setDisplayName(const QString &name) -> void
{
  if (name == d->m_displayName)
    return;
  d->m_displayName = name;
  emit displayNameChanged();
}

auto Project::setId(Id id) -> void
{
  QTC_ASSERT(!d->m_id.isValid(), return); // Id may not change ever!
  d->m_id = id;
}

auto Project::setRootProjectNode(std::unique_ptr<ProjectNode> &&root) -> void
{
  QTC_ASSERT(d->m_rootProjectNode.get() != root.get() || !root, return);

  if (root && root->isEmpty()) {
    // Something went wrong with parsing: At least the project file needs to be
    // shown so that the user can fix the breakage.
    // Do not leak root and use default project tree in this case.
    root.reset();
  }

  if (root) {
    ProjectTree::applyTreeManager(root.get(), ProjectTree::AsyncPhase);
    ProjectTree::applyTreeManager(root.get(), ProjectTree::FinalPhase);
    root->setParentFolderNode(d->m_containerNode.get());
  }

  const auto oldNode = std::move(d->m_rootProjectNode);

  d->m_rootProjectNode = std::move(root);
  if (oldNode || d->m_rootProjectNode)
    handleSubTreeChanged(d->m_containerNode.get());
}

auto Project::handleSubTreeChanged(FolderNode *node) -> void
{
  QVector<const Node*> nodeList;
  if (d->m_rootProjectNode) {
    d->m_rootProjectNode->forEachGenericNode([&nodeList](const Node *n) {
      nodeList.append(n);
    });
    sort(nodeList, &nodeLessThan);
  }
  d->m_sortedNodeList = nodeList;

  ProjectTree::emitSubtreeChanged(node);
  emit fileListChanged();
}

auto Project::saveSettings() -> void
{
  emit aboutToSaveSettings();
  if (!d->m_accessor)
    d->m_accessor = std::make_unique<Internal::UserFileAccessor>(this);
  if (!targets().isEmpty())
    d->m_accessor->saveSettings(toMap(), ICore::dialogParent());
}

auto Project::restoreSettings(QString *errorMessage) -> RestoreResult
{
  if (!d->m_accessor)
    d->m_accessor = std::make_unique<Internal::UserFileAccessor>(this);
  const auto map(d->m_accessor->restoreSettings(ICore::dialogParent()));
  const auto result = fromMap(map, errorMessage);
  if (result == RestoreResult::Ok) emit settingsLoaded();

  return result;
}

/*!
 * Returns a sorted list of all files matching the predicate \a filter.
 */
auto Project::files(const NodeMatcher &filter) const -> FilePaths
{
  QTC_ASSERT(filter, return {});

  FilePaths result;
  if (d->m_sortedNodeList.empty() && filter(containerNode()))
    result.append(projectFilePath());

  FilePath lastAdded;
  for (const auto n : qAsConst(d->m_sortedNodeList)) {
    if (!filter(n))
      continue;

    // Remove duplicates:
    const auto path = n->filePath();
    if (path == lastAdded)
      continue; // skip duplicates
    lastAdded = path;

    result.append(path);
  }
  return result;
}

/*!
    Serializes all data into a QVariantMap.

    This map is then saved in the .user file of the project.
    Just put all your data into the map.

    \note Do not forget to call your base class' toMap function.
    \note Do not forget to call setActiveBuildConfiguration when
    creating new build configurations.
*/

auto Project::toMap() const -> QVariantMap
{
  const auto ts = targets();

  QVariantMap map;
  map.insert(QLatin1String(ACTIVE_TARGET_KEY), ts.indexOf(d->m_activeTarget));
  map.insert(QLatin1String(TARGET_COUNT_KEY), ts.size());
  for (auto i = 0; i < ts.size(); ++i)
    map.insert(QString::fromLatin1(TARGET_KEY_PREFIX) + QString::number(i), ts.at(i)->toMap());

  map.insert(QLatin1String(EDITOR_SETTINGS_KEY), d->m_editorConfiguration.toMap());
  if (!d->m_pluginSettings.isEmpty())
    map.insert(QLatin1String(PLUGIN_SETTINGS_KEY), d->m_pluginSettings);

  return map;
}

/*!
    Returns the directory that contains the project.

    This includes the absolute path.
*/

auto Project::projectDirectory() const -> FilePath
{
  return projectDirectory(projectFilePath());
}

/*!
    Returns the directory that contains the file \a top.

    This includes the absolute path.
*/

auto Project::projectDirectory(const FilePath &top) -> FilePath
{
  if (top.isEmpty())
    return FilePath();
  return top.absolutePath();
}

auto Project::changeRootProjectDirectory() -> void
{
  const auto rootPath = FileUtils::getExistingDirectory(nullptr, tr("Select the Root Directory"), rootProjectDirectory(), QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
  if (rootPath != d->m_rootProjectDirectory) {
    d->m_rootProjectDirectory = rootPath;
    setNamedSettings(Constants::PROJECT_ROOT_PATH_KEY, d->m_rootProjectDirectory.toString());
    emit rootProjectDirectoryChanged();
  }
}

/*!
    Returns the common root directory that contains all files which belong to a project.
*/
auto Project::rootProjectDirectory() const -> FilePath
{
  if (!d->m_rootProjectDirectory.isEmpty())
    return d->m_rootProjectDirectory;

  return projectDirectory();
}

auto Project::rootProjectNode() const -> ProjectNode*
{
  return d->m_rootProjectNode.get();
}

auto Project::containerNode() const -> ContainerNode*
{
  return d->m_containerNode.get();
}

auto Project::fromMap(const QVariantMap &map, QString *errorMessage) -> RestoreResult
{
  Q_UNUSED(errorMessage)
  if (map.contains(QLatin1String(EDITOR_SETTINGS_KEY))) {
    const auto values(map.value(QLatin1String(EDITOR_SETTINGS_KEY)).toMap());
    d->m_editorConfiguration.fromMap(values);
  }

  if (map.contains(QLatin1String(PLUGIN_SETTINGS_KEY)))
    d->m_pluginSettings = map.value(QLatin1String(PLUGIN_SETTINGS_KEY)).toMap();

  bool ok;
  auto maxI(map.value(QLatin1String(TARGET_COUNT_KEY), 0).toInt(&ok));
  if (!ok || maxI < 0)
    maxI = 0;
  auto active(map.value(QLatin1String(ACTIVE_TARGET_KEY), 0).toInt(&ok));
  if (!ok || active < 0 || active >= maxI)
    active = 0;

  if (active >= 0 && active < maxI)
    createTargetFromMap(map, active); // sets activeTarget since it is the first target created!

  for (auto i = 0; i < maxI; ++i) {
    if (i == active) // already covered!
      continue;

    createTargetFromMap(map, i);
  }

  d->m_rootProjectDirectory = FilePath::fromString(namedSettings(Constants::PROJECT_ROOT_PATH_KEY).toString());

  return RestoreResult::Ok;
}

auto Project::createTargetFromMap(const QVariantMap &map, int index) -> void
{
  const QString key = QString::fromLatin1(TARGET_KEY_PREFIX) + QString::number(index);
  if (!map.contains(key))
    return;

  const auto targetMap = map.value(key).toMap();

  const auto id = idFromMap(targetMap);
  if (target(id)) {
    qWarning("Warning: Duplicated target id found, not restoring second target with id '%s'. Continuing.", qPrintable(id.toString()));
    return;
  }

  auto k = KitManager::kit(id);
  if (!k) {
    auto deviceTypeId = Id::fromSetting(targetMap.value(Target::deviceTypeKey()));
    if (!deviceTypeId.isValid())
      deviceTypeId = Constants::DESKTOP_DEVICE_TYPE;
    const auto formerKitName = targetMap.value(Target::displayNameKey()).toString();
    k = KitManager::registerKit([deviceTypeId, &formerKitName](Kit *kit) {
      const auto kitNameSuggestion = formerKitName.contains(tr("Replacement for")) ? formerKitName : tr("Replacement for \"%1\"").arg(formerKitName);
      const auto tempKitName = makeUniquelyNumbered(kitNameSuggestion, transform(KitManager::kits(), &Kit::unexpandedDisplayName));
      kit->setUnexpandedDisplayName(tempKitName);
      DeviceTypeKitAspect::setDeviceTypeId(kit, deviceTypeId);
      kit->makeReplacementKit();
      kit->setup();
    }, id);
    QTC_ASSERT(k, return);
    TaskHub::addTask(BuildSystemTask(Task::Warning, tr("Project \"%1\" was configured for " "kit \"%2\" with id %3, which does not exist anymore. The new kit \"%4\" was " "created in its place, in an attempt not to lose custom project settings.").arg(displayName(), formerKitName, id.toString(), k->displayName())));
  }

  auto t = std::make_unique<Target>(this, k, Target::_constructor_tag{});
  if (!t->fromMap(targetMap))
    return;

  if (t->runConfigurations().isEmpty() && t->buildConfigurations().isEmpty())
    return;

  addTarget(std::move(t));
}

auto Project::editorConfiguration() const -> EditorConfiguration*
{
  return &d->m_editorConfiguration;
}

auto Project::isKnownFile(const FilePath &filename) const -> bool
{
  if (d->m_sortedNodeList.empty())
    return filename == projectFilePath();
  const FileNode element(filename, FileType::Unknown);
  return std::binary_search(std::begin(d->m_sortedNodeList), std::end(d->m_sortedNodeList), &element, nodeLessThan);
}

auto Project::nodeForFilePath(const FilePath &filePath, const NodeMatcher &extraMatcher) const -> const Node*
{
  const FileNode dummy(filePath, FileType::Unknown);
  const auto range = std::equal_range(d->m_sortedNodeList.cbegin(), d->m_sortedNodeList.cend(), &dummy, &nodeLessThan);
  for (auto it = range.first; it != range.second; ++it) {
    if ((*it)->filePath() == filePath && (!extraMatcher || extraMatcher(*it)))
      return *it;
  }
  return nullptr;
}

auto Project::setProjectLanguages(Context language) -> void
{
  if (d->m_projectLanguages == language)
    return;
  d->m_projectLanguages = language;
  emit projectLanguagesUpdated();
}

auto Project::addProjectLanguage(Id id) -> void
{
  auto lang = projectLanguages();
  const auto pos = lang.indexOf(id);
  if (pos < 0)
    lang.add(id);
  setProjectLanguages(lang);
}

auto Project::removeProjectLanguage(Id id) -> void
{
  auto lang = projectLanguages();
  const auto pos = lang.indexOf(id);
  if (pos >= 0)
    lang.removeAt(pos);
  setProjectLanguages(lang);
}

auto Project::setProjectLanguage(Id id, bool enabled) -> void
{
  if (enabled)
    addProjectLanguage(id);
  else
    removeProjectLanguage(id);
}

auto Project::setHasMakeInstallEquivalent(bool enabled) -> void
{
  d->m_hasMakeInstallEquivalent = enabled;
}

auto Project::setNeedsBuildConfigurations(bool value) -> void
{
  d->m_needsBuildConfigurations = value;
}

auto Project::setNeedsDeployConfigurations(bool value) -> void
{
  d->m_needsDeployConfigurations = value;
}

auto Project::createProjectTask(Task::TaskType type, const QString &description) -> Task
{
  return Task(type, description, FilePath(), -1, Id());
}

auto Project::setBuildSystemCreator(const std::function<BuildSystem *(Target *)> &creator) -> void
{
  d->m_buildSystemCreator = creator;
}

auto Project::projectContext() const -> Context
{
  return Context(d->m_id);
}

auto Project::projectLanguages() const -> Context
{
  return d->m_projectLanguages;
}

auto Project::namedSettings(const QString &name) const -> QVariant
{
  return d->m_pluginSettings.value(name);
}

auto Project::setNamedSettings(const QString &name, const QVariant &value) -> void
{
  if (value.isNull())
    d->m_pluginSettings.remove(name);
  else
    d->m_pluginSettings.insert(name, value);
}

auto Project::setAdditionalEnvironment(const EnvironmentItems &envItems) -> void
{
  setNamedSettings(PROJECT_ENV_KEY, NameValueItem::toStringList(envItems));
  emit environmentChanged();
}

auto Project::additionalEnvironment() const -> EnvironmentItems
{
  return NameValueItem::fromStringList(namedSettings(PROJECT_ENV_KEY).toStringList());
}

auto Project::needsConfiguration() const -> bool
{
  return d->m_targets.size() == 0;
}

auto Project::needsBuildConfigurations() const -> bool
{
  return d->m_needsBuildConfigurations;
}

auto Project::configureAsExampleProject(Kit * /*kit*/) -> void {}

auto Project::hasMakeInstallEquivalent() const -> bool
{
  return d->m_hasMakeInstallEquivalent;
}

auto Project::makeInstallCommand(const Target *target, const QString &installRoot) -> MakeInstallCommand
{
  QTC_ASSERT(hasMakeInstallEquivalent(), return MakeInstallCommand());
  MakeInstallCommand cmd;
  if (const BuildConfiguration *const bc = target->activeBuildConfiguration()) {
    if (const auto makeStep = bc->buildSteps()->firstOfType<MakeStep>())
      cmd.command = makeStep->makeExecutable();
  }
  cmd.arguments << "install" << ("INSTALL_ROOT=" + QDir::toNativeSeparators(installRoot));
  return cmd;
}

auto Project::setup(const QList<BuildInfo> &infoList) -> void
{
  std::vector<std::unique_ptr<Target>> toRegister;
  for (const auto &info : infoList) {
    auto k = KitManager::kit(info.kitId);
    if (!k)
      continue;
    auto t = target(k);
    if (!t)
      t = findOrDefault(toRegister, equal(&Target::kit, k));
    if (!t) {
      auto newTarget = std::make_unique<Target>(this, k, Target::_constructor_tag{});
      t = newTarget.get();
      toRegister.emplace_back(std::move(newTarget));
    }

    if (!info.factory)
      continue;

    if (const auto bc = info.factory->create(t, info))
      t->addBuildConfiguration(bc);
  }
  for (auto &t : toRegister) {
    t->updateDefaultDeployConfigurations();
    t->updateDefaultRunConfigurations();
    addTarget(std::move(t));
  }
}

auto Project::macroExpander() const -> MacroExpander*
{
  return &d->m_macroExpander;
}

auto Project::findNodeForBuildKey(const QString &buildKey) const -> ProjectNode*
{
  if (!d->m_rootProjectNode)
    return nullptr;

  return d->m_rootProjectNode->findProjectNode([buildKey](const ProjectNode *node) {
    return node->buildKey() == buildKey;
  });
}

auto Project::projectImporter() const -> ProjectImporter*
{
  return nullptr;
}

auto Project::setCanBuildProducts() -> void
{
  d->m_canBuildProducts = true;
}

auto Project::setExtraData(const QString &key, const QVariant &data) -> void
{
  d->m_extraData.insert(key, data);
}

auto Project::extraData(const QString &key) const -> QVariant
{
  return d->m_extraData.value(key);
}

auto Project::availableQmlPreviewTranslations(QString *errorMessage) -> QStringList
{
  const auto projectDirectory = rootProjectDirectory().toFileInfo().absoluteFilePath();
  const QDir languageDirectory(projectDirectory + "/i18n");
  const auto qmFiles = languageDirectory.entryList({"qml_*.qm"});
  if (qmFiles.isEmpty() && errorMessage)
    errorMessage->append(tr("Could not find any qml_*.qm file at \"%1\"").arg(languageDirectory.absolutePath()));
  return transform(qmFiles, [](const QString &qmFile) {
    const int localeStartPosition = qmFile.lastIndexOf("_") + 1;
    const int localeEndPosition = qmFile.size() - QString(".qm").size();
    const auto locale = qmFile.left(localeEndPosition).mid(localeStartPosition);
    return locale;
  });
}

auto Project::modifiedDocuments() const -> QList<IDocument*>
{
  QList<IDocument*> modifiedProjectDocuments;

  for (const auto doc : DocumentModel::openedDocuments()) {
    if (doc->isModified() && isKnownFile(doc->filePath()))
      modifiedProjectDocuments.append(doc);
  }

  return modifiedProjectDocuments;
}

auto Project::isModified() const -> bool
{
  return !modifiedDocuments().isEmpty();
}

auto Project::isEditModePreferred() const -> bool
{
  return true;
}

#if defined(WITH_TESTS)

static FilePath constructTestPath(const char *basePath)
{
  FilePath drive;
  if (HostOsInfo::isWindowsHost())
    drive = "C:";
  return drive + QLatin1String(basePath);
}

const FilePath TEST_PROJECT_PATH = constructTestPath("/tmp/foobar/baz.project");
const FilePath TEST_PROJECT_NONEXISTING_FILE = constructTestPath("/tmp/foobar/nothing.cpp");
const FilePath TEST_PROJECT_CPP_FILE = constructTestPath("/tmp/foobar/main.cpp");
const FilePath TEST_PROJECT_GENERATED_FILE = constructTestPath("/tmp/foobar/generated.foo");
const QString TEST_PROJECT_MIMETYPE = "application/vnd.test.qmakeprofile";
const QString TEST_PROJECT_DISPLAYNAME = "testProjectFoo";
const char TEST_PROJECT_ID[] = "Test.Project.Id";

class TestBuildSystem : public BuildSystem {
public:
  using BuildSystem::BuildSystem;

  void triggerParsing() final {}
  QString name() const final { return QLatin1String("test"); }
};

class TestProject : public Project {
public:
  TestProject() : Project(TEST_PROJECT_MIMETYPE, TEST_PROJECT_PATH)
  {
    setId(TEST_PROJECT_ID);
    setDisplayName(TEST_PROJECT_DISPLAYNAME);
    setBuildSystemCreator([](Target *t) { return new TestBuildSystem(t); });
    setNeedsBuildConfigurations(false);
    setNeedsDeployConfigurations(false);

    target = addTargetForKit(&testKit);
  }

  bool needsConfiguration() const final { return false; }

  Kit testKit;
  Target *target = nullptr;
};

void ProjectExplorerPlugin::testProject_setup()
{
  TestProject project;

  QCOMPARE(project.displayName(), TEST_PROJECT_DISPLAYNAME);

  QVERIFY(!project.rootProjectNode());
  QVERIFY(project.containerNode());

  QVERIFY(project.macroExpander());

  QCOMPARE(project.mimeType(), TEST_PROJECT_MIMETYPE);
  QCOMPARE(project.projectFilePath(), TEST_PROJECT_PATH);
  QCOMPARE(project.projectDirectory(), TEST_PROJECT_PATH.parentDir());

  QCOMPARE(project.isKnownFile(TEST_PROJECT_PATH), true);
  QCOMPARE(project.isKnownFile(TEST_PROJECT_NONEXISTING_FILE), false);
  QCOMPARE(project.isKnownFile(TEST_PROJECT_CPP_FILE), false);

  QCOMPARE(project.files(Project::AllFiles), {TEST_PROJECT_PATH});
  QCOMPARE(project.files(Project::GeneratedFiles), {});

  QCOMPARE(project.id(), Id(TEST_PROJECT_ID));

  QVERIFY(!project.target->buildSystem()->isParsing());
  QVERIFY(!project.target->buildSystem()->hasParsingData());
}

void ProjectExplorerPlugin::testProject_changeDisplayName()
{
  TestProject project;

  QSignalSpy spy(&project, &Project::displayNameChanged);

  const QString newName = "other name";
  project.setDisplayName(newName);
  QCOMPARE(spy.count(), 1);
  QVariantList args = spy.takeFirst();
  QCOMPARE(args, {});

  project.setDisplayName(newName);
  QCOMPARE(spy.count(), 0);
}

void ProjectExplorerPlugin::testProject_parsingSuccess()
{
  TestProject project;

  QSignalSpy startSpy(project.target->buildSystem(), &BuildSystem::parsingStarted);
  QSignalSpy stopSpy(project.target->buildSystem(), &BuildSystem::parsingFinished);

  {
    BuildSystem::ParseGuard guard = project.target->buildSystem()->guardParsingRun();
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(stopSpy.count(), 0);

    QVERIFY(project.target->buildSystem()->isParsing());
    QVERIFY(!project.target->buildSystem()->hasParsingData());

    guard.markAsSuccess();
  }

  QCOMPARE(startSpy.count(), 1);
  QCOMPARE(stopSpy.count(), 1);
  QCOMPARE(stopSpy.at(0), {QVariant(true)});

  QVERIFY(!project.target->buildSystem()->isParsing());
  QVERIFY(project.target->buildSystem()->hasParsingData());
}

void ProjectExplorerPlugin::testProject_parsingFail()
{
  TestProject project;

  QSignalSpy startSpy(project.target->buildSystem(), &BuildSystem::parsingStarted);
  QSignalSpy stopSpy(project.target->buildSystem(), &BuildSystem::parsingFinished);

  {
    BuildSystem::ParseGuard guard = project.target->buildSystem()->guardParsingRun();
    QCOMPARE(startSpy.count(), 1);
    QCOMPARE(stopSpy.count(), 0);

    QVERIFY(project.target->buildSystem()->isParsing());
    QVERIFY(!project.target->buildSystem()->hasParsingData());
  }

  QCOMPARE(startSpy.count(), 1);
  QCOMPARE(stopSpy.count(), 1);
  QCOMPARE(stopSpy.at(0), {QVariant(false)});

  QVERIFY(!project.target->buildSystem()->isParsing());
  QVERIFY(!project.target->buildSystem()->hasParsingData());
}

std::unique_ptr<ProjectNode> createFileTree(Project *project)
{
  std::unique_ptr<ProjectNode> root = std::make_unique<ProjectNode>(project->projectDirectory());
  std::vector<std::unique_ptr<FileNode>> nodes;
  nodes.emplace_back(std::make_unique<FileNode>(TEST_PROJECT_PATH, FileType::Project));
  nodes.emplace_back(std::make_unique<FileNode>(TEST_PROJECT_CPP_FILE, FileType::Source));
  nodes.emplace_back(std::make_unique<FileNode>(TEST_PROJECT_GENERATED_FILE, FileType::Source));
  nodes.back()->setIsGenerated(true);
  root->addNestedNodes(std::move(nodes));

  return root;
}

void ProjectExplorerPlugin::testProject_projectTree()
{
  TestProject project;
  QSignalSpy fileSpy(&project, &Project::fileListChanged);

  project.setRootProjectNode(nullptr);
  QCOMPARE(fileSpy.count(), 0);
  QVERIFY(!project.rootProjectNode());

  project.setRootProjectNode(std::make_unique<ProjectNode>(project.projectDirectory()));
  QCOMPARE(fileSpy.count(), 0);
  QVERIFY(!project.rootProjectNode());

  std::unique_ptr<ProjectNode> root = createFileTree(&project);
  ProjectNode *rootNode = root.get();
  project.setRootProjectNode(std::move(root));
  QCOMPARE(fileSpy.count(), 1);
  QCOMPARE(project.rootProjectNode(), rootNode);

  // Test known files:
  QCOMPARE(project.isKnownFile(TEST_PROJECT_PATH), true);
  QCOMPARE(project.isKnownFile(TEST_PROJECT_NONEXISTING_FILE), false);
  QCOMPARE(project.isKnownFile(TEST_PROJECT_CPP_FILE), true);
  QCOMPARE(project.isKnownFile(TEST_PROJECT_GENERATED_FILE), true);

  FilePaths allFiles = project.files(Project::AllFiles);
  QCOMPARE(allFiles.count(), 3);
  QVERIFY(allFiles.contains(TEST_PROJECT_PATH));
  QVERIFY(allFiles.contains(TEST_PROJECT_CPP_FILE));
  QVERIFY(allFiles.contains(TEST_PROJECT_GENERATED_FILE));

  QCOMPARE(project.files(Project::GeneratedFiles), {TEST_PROJECT_GENERATED_FILE});
  FilePaths sourceFiles = project.files(Project::SourceFiles);
  QCOMPARE(sourceFiles.count(), 2);
  QVERIFY(sourceFiles.contains(TEST_PROJECT_PATH));
  QVERIFY(sourceFiles.contains(TEST_PROJECT_CPP_FILE));

  project.setRootProjectNode(nullptr);
  QCOMPARE(fileSpy.count(), 2);
  QVERIFY(!project.rootProjectNode());
}

void ProjectExplorerPlugin::testProject_multipleBuildConfigs()
{
  // Find suitable kit.
  Kit *const kit = findOr(KitManager::kits(), nullptr, [](const Kit *k) {
    return k->isValid();
  });
  if (!kit)
    QSKIP("The test requires at least one valid kit.");

  // Copy project from qrc file and set it up.
  QTemporaryDir *const tempDir = TemporaryDirectory::masterTemporaryDirectory();
  QVERIFY(tempDir->isValid());
  QString error;
  const FilePath projectDir = FilePath::fromString(tempDir->path() + "/generic-project");
  FileUtils::copyRecursively(":/projectexplorer/testdata/generic-project", projectDir, &error);
  QVERIFY2(error.isEmpty(), qPrintable(error));
  const QFileInfoList files = QDir(projectDir.toString()).entryInfoList(QDir::Files | QDir::Dirs);
  for (const QFileInfo &f : files)
    QFile(f.absoluteFilePath()).setPermissions(f.permissions() | QFile::WriteUser);
  const auto theProject = openProject(projectDir.pathAppended("generic-project.creator"));
  QVERIFY2(theProject, qPrintable(theProject.errorMessage()));
  theProject.project()->configureAsExampleProject(kit);
  QCOMPARE(theProject.project()->targets().size(), 1);
  Target *const target = theProject.project()->activeTarget();
  QVERIFY(target);
  QCOMPARE(target->buildConfigurations().size(), 6);
  SessionManager::setActiveBuildConfiguration(target, target->buildConfigurations().at(1), SetActive::Cascade);
  BuildSystem *const bs = theProject.project()->activeTarget()->buildSystem();
  QVERIFY(bs);
  QCOMPARE(bs, target->activeBuildConfiguration()->buildSystem());
  if (bs->isWaitingForParse() || bs->isParsing()) {
    QEventLoop loop;
    QTimer t;
    t.setSingleShot(true);
    connect(&t, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(bs, &BuildSystem::parsingFinished, &loop, &QEventLoop::quit);
    t.start(10000);
    QVERIFY(loop.exec());
    QVERIFY(t.isActive());
  }
  QVERIFY(!bs->isWaitingForParse() && !bs->isParsing());

  QCOMPARE(SessionManager::startupProject(), theProject.project());
  QCOMPARE(ProjectTree::currentProject(), theProject.project());
  QVERIFY(EditorManager::openEditor(projectDir.pathAppended("main.cpp")));
  QVERIFY(ProjectTree::currentNode());
  ProjectTree::instance()->expandAll();
  SessionManager::closeAllProjects(); // QTCREATORBUG-25655
}

#endif // WITH_TESTS

} // namespace ProjectExplorer
