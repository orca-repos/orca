// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildsystem.hpp"

#include "buildconfiguration.hpp"
#include "projectexplorer.hpp"
#include "runconfiguration.hpp"
#include "runcontrol.hpp"
#include "session.hpp"
#include "target.hpp"

#include <core/core-message-manager.hpp>
#include <core/core-output-window.hpp>
#include <projectexplorer/buildaspects.hpp>

#include <utils/qtcassert.hpp>

#include <QTimer>

using namespace Utils;

namespace ProjectExplorer {

// --------------------------------------------------------------------
// BuildSystem:
// --------------------------------------------------------------------

class BuildSystemPrivate {
public:
  Target *m_target = nullptr;
  BuildConfiguration *m_buildConfiguration = nullptr;
  QTimer m_delayedParsingTimer;
  bool m_isParsing = false;
  bool m_hasParsingData = false;
  DeploymentData m_deploymentData;
  QList<BuildTargetInfo> m_appTargets;
};

BuildSystem::BuildSystem(BuildConfiguration *bc) : BuildSystem(bc->target())
{
  d->m_buildConfiguration = bc;
}

BuildSystem::BuildSystem(Target *target) : d(new BuildSystemPrivate)
{
  QTC_CHECK(target);
  d->m_target = target;

  // Timer:
  d->m_delayedParsingTimer.setSingleShot(true);

  connect(&d->m_delayedParsingTimer, &QTimer::timeout, this, [this] {
    if (SessionManager::hasProject(project()))
      triggerParsing();
    else
      requestDelayedParse();
  });
}

BuildSystem::~BuildSystem()
{
  delete d;
}

auto BuildSystem::project() const -> Project*
{
  return d->m_target->project();
}

auto BuildSystem::target() const -> Target*
{
  return d->m_target;
}

auto BuildSystem::kit() const -> Kit*
{
  return d->m_target->kit();
}

auto BuildSystem::buildConfiguration() const -> BuildConfiguration*
{
  return d->m_buildConfiguration;
}

auto BuildSystem::emitParsingStarted() -> void
{
  QTC_ASSERT(!d->m_isParsing, return);

  d->m_isParsing = true;
  emit parsingStarted();
  emit d->m_target->parsingStarted();
}

auto BuildSystem::emitParsingFinished(bool success) -> void
{
  // Intentionally no return, as we currently get start - start - end - end
  // sequences when switching qmake targets quickly.
  QTC_CHECK(d->m_isParsing);

  d->m_isParsing = false;
  d->m_hasParsingData = success;
  emit parsingFinished(success);
  emit d->m_target->parsingFinished(success);
}

auto BuildSystem::projectFilePath() const -> FilePath
{
  return d->m_target->project()->projectFilePath();
}

auto BuildSystem::projectDirectory() const -> FilePath
{
  return d->m_target->project()->projectDirectory();
}

auto BuildSystem::isWaitingForParse() const -> bool
{
  return d->m_delayedParsingTimer.isActive();
}

auto BuildSystem::requestParse() -> void
{
  requestParseHelper(0);
}

auto BuildSystem::requestDelayedParse() -> void
{
  requestParseHelper(1000);
}

auto BuildSystem::requestParseWithCustomDelay(int delayInMs) -> void
{
  requestParseHelper(delayInMs);
}

auto BuildSystem::cancelDelayedParseRequest() -> void
{
  d->m_delayedParsingTimer.stop();
}

auto BuildSystem::setParseDelay(int delayInMs) -> void
{
  d->m_delayedParsingTimer.setInterval(delayInMs);
}

auto BuildSystem::parseDelay() const -> int
{
  return d->m_delayedParsingTimer.interval();
}

auto BuildSystem::isParsing() const -> bool
{
  return d->m_isParsing;
}

auto BuildSystem::hasParsingData() const -> bool
{
  return d->m_hasParsingData;
}

auto BuildSystem::activeParseEnvironment() const -> Environment
{
  const BuildConfiguration *const bc = d->m_target->activeBuildConfiguration();
  if (bc)
    return bc->environment();

  const RunConfiguration *const rc = d->m_target->activeRunConfiguration();
  if (rc)
    return rc->runnable().environment;

  return d->m_target->kit()->buildEnvironment();
}

auto BuildSystem::requestParseHelper(int delay) -> void
{
  d->m_delayedParsingTimer.setInterval(delay);
  d->m_delayedParsingTimer.start();
}

auto BuildSystem::addFiles(Node *, const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  Q_UNUSED(filePaths)
  Q_UNUSED(notAdded)
  return false;
}

auto BuildSystem::removeFiles(Node *, const FilePaths &filePaths, FilePaths *notRemoved) -> RemovedFilesFromProject
{
  Q_UNUSED(filePaths)
  Q_UNUSED(notRemoved)
  return RemovedFilesFromProject::Error;
}

auto BuildSystem::deleteFiles(Node *, const FilePaths &filePaths) -> bool
{
  Q_UNUSED(filePaths)
  return false;
}

auto BuildSystem::canRenameFile(Node *, const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  Q_UNUSED(oldFilePath)
  Q_UNUSED(newFilePath)
  return true;
}

auto BuildSystem::renameFile(Node *, const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  Q_UNUSED(oldFilePath)
  Q_UNUSED(newFilePath)
  return false;
}

auto BuildSystem::addDependencies(Node *, const QStringList &dependencies) -> bool
{
  Q_UNUSED(dependencies)
  return false;
}

auto BuildSystem::supportsAction(Node *, ProjectAction, const Node *) const -> bool
{
  return false;
}

auto BuildSystem::filesGeneratedFrom(const FilePath &sourceFile) const -> FilePaths
{
  Q_UNUSED(sourceFile)
  return {};
}

auto BuildSystem::additionalData(Id id) const -> QVariant
{
  Q_UNUSED(id)
  return {};
}

// ParseGuard

BuildSystem::ParseGuard::ParseGuard(ParseGuard &&other) : m_buildSystem{std::move(other.m_buildSystem)}, m_success{std::move(other.m_success)}
{
  // No need to release this as this is invalid anyway:-)
  other.m_buildSystem = nullptr;
}

BuildSystem::ParseGuard::ParseGuard(BuildSystem *p) : m_buildSystem(p)
{
  if (m_buildSystem && !m_buildSystem->isParsing())
    m_buildSystem->emitParsingStarted();
  else
    m_buildSystem = nullptr;
}

auto BuildSystem::ParseGuard::release() -> void
{
  if (m_buildSystem)
    m_buildSystem->emitParsingFinished(m_success);
  m_buildSystem = nullptr;
}

auto BuildSystem::ParseGuard::operator=(ParseGuard &&other) -> ParseGuard&
{
  release();

  m_buildSystem = std::move(other.m_buildSystem);
  m_success = std::move(other.m_success);

  other.m_buildSystem = nullptr;
  return *this;
}

auto BuildSystem::setDeploymentData(const DeploymentData &deploymentData) -> void
{
  if (d->m_deploymentData != deploymentData) {
    d->m_deploymentData = deploymentData;
    emit deploymentDataChanged();
    emit applicationTargetsChanged();
    emit target()->deploymentDataChanged();
  }
}

auto BuildSystem::deploymentData() const -> DeploymentData
{
  return d->m_deploymentData;
}

auto BuildSystem::setApplicationTargets(const QList<BuildTargetInfo> &appTargets) -> void
{
  if (toSet(appTargets) != toSet(d->m_appTargets)) {
    d->m_appTargets = appTargets;
    emit applicationTargetsChanged();
  }
}

auto BuildSystem::applicationTargets() const -> const QList<BuildTargetInfo>
{
  return d->m_appTargets;
}

auto BuildSystem::buildTarget(const QString &buildKey) const -> BuildTargetInfo
{
  return findOrDefault(d->m_appTargets, [&buildKey](const BuildTargetInfo &ti) {
    return ti.buildKey == buildKey;
  });
}

auto BuildSystem::setRootProjectNode(std::unique_ptr<ProjectNode> &&root) -> void
{
  d->m_target->project()->setRootProjectNode(std::move(root));
}

auto BuildSystem::emitBuildSystemUpdated() -> void
{
  emit target()->buildSystemUpdated(this);
}

auto BuildSystem::setExtraData(const QString &buildKey, Id dataKey, const QVariant &data) -> void
{
  const ProjectNode *node = d->m_target->project()->findNodeForBuildKey(buildKey);
  QTC_ASSERT(node, return);
  node->setData(dataKey, data);
}

auto BuildSystem::extraData(const QString &buildKey, Id dataKey) const -> QVariant
{
  const ProjectNode *node = d->m_target->project()->findNodeForBuildKey(buildKey);
  QTC_ASSERT(node, return {});
  return node->data(dataKey);
}

auto BuildSystem::startNewBuildSystemOutput(const QString &message) -> void
{
  const auto outputArea = ProjectExplorerPlugin::buildSystemOutput();
  outputArea->grayOutOldContent();
  outputArea->appendMessage(message + '\n', GeneralMessageFormat);
  Orca::Plugin::Core::MessageManager::writeFlashing(message);
}

auto BuildSystem::appendBuildSystemOutput(const QString &message) -> void
{
  const auto outputArea = ProjectExplorerPlugin::buildSystemOutput();
  outputArea->appendMessage(message + '\n', GeneralMessageFormat);
  Orca::Plugin::Core::MessageManager::writeSilently(message);
}

auto BuildSystem::disabledReason(const QString &buildKey) const -> QString
{
  if (!hasParsingData()) {
    auto msg = isParsing() ? tr("The project is currently being parsed.") : tr("The project could not be fully parsed.");
    const auto projectFilePath = buildTarget(buildKey).projectFilePath;
    if (!projectFilePath.isEmpty() && !projectFilePath.exists())
      msg += '\n' + tr("The project file \"%1\" does not exist.").arg(projectFilePath.toString());
    return msg;
  }
  return {};
}

auto BuildSystem::commandLineForTests(const QList<QString> & /*tests*/, const QStringList & /*options*/) const -> CommandLine
{
  return {};
}

} // namespace ProjectExplorer
