// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmaketoolmanager.hpp"

#include "cmaketoolsettingsaccessor.hpp"

#include <extensionsystem/pluginmanager.hpp>

#include <core/helpmanager.hpp>
#include <core/icore.hpp>

#include <utils/environment.hpp>
#include <utils/pointeralgorithm.hpp>
#include <utils/qtcassert.hpp>

using namespace Core;
using namespace Utils;

namespace CMakeProjectManager {

// --------------------------------------------------------------------
// CMakeToolManagerPrivate:
// --------------------------------------------------------------------

class CMakeToolManagerPrivate {
public:
  Id m_defaultCMake;
  std::vector<std::unique_ptr<CMakeTool>> m_cmakeTools;
  Internal::CMakeToolSettingsAccessor m_accessor;
};

static CMakeToolManagerPrivate *d = nullptr;

// --------------------------------------------------------------------
// CMakeToolManager:
// --------------------------------------------------------------------

CMakeToolManager *CMakeToolManager::m_instance = nullptr;

CMakeToolManager::CMakeToolManager()
{
  QTC_ASSERT(!m_instance, return);
  m_instance = this;

  qRegisterMetaType<QString*>();

  d = new CMakeToolManagerPrivate;
  connect(ICore::instance(), &ICore::saveSettingsRequested, this, &CMakeToolManager::saveCMakeTools);

  connect(this, &CMakeToolManager::cmakeAdded, this, &CMakeToolManager::cmakeToolsChanged);
  connect(this, &CMakeToolManager::cmakeRemoved, this, &CMakeToolManager::cmakeToolsChanged);
  connect(this, &CMakeToolManager::cmakeUpdated, this, &CMakeToolManager::cmakeToolsChanged);

  setObjectName("CMakeToolManager");
  ExtensionSystem::PluginManager::addObject(this);
}

CMakeToolManager::~CMakeToolManager()
{
  ExtensionSystem::PluginManager::removeObject(this);
  delete d;
}

auto CMakeToolManager::instance() -> CMakeToolManager*
{
  return m_instance;
}

auto CMakeToolManager::cmakeTools() -> QList<CMakeTool*>
{
  return Utils::toRawPointer<QList>(d->m_cmakeTools);
}

auto CMakeToolManager::registerCMakeTool(std::unique_ptr<CMakeTool> &&tool) -> bool
{
  if (!tool || Utils::contains(d->m_cmakeTools, tool.get()))
    return true;

  const auto toolId = tool->id();
  QTC_ASSERT(toolId.isValid(), return false);

  //make sure the same id was not used before
  QTC_ASSERT(!Utils::contains(d->m_cmakeTools, [toolId](const std::unique_ptr<CMakeTool> &known) { return toolId == known->id(); }), return false);

  d->m_cmakeTools.emplace_back(std::move(tool));

  emit CMakeToolManager::m_instance->cmakeAdded(toolId);

  ensureDefaultCMakeToolIsValid();

  updateDocumentation();

  return true;
}

auto CMakeToolManager::deregisterCMakeTool(const Id &id) -> void
{
  auto toRemove = Utils::take(d->m_cmakeTools, Utils::equal(&CMakeTool::id, id));
  if (toRemove.has_value()) {
    ensureDefaultCMakeToolIsValid();

    updateDocumentation();

    emit m_instance->cmakeRemoved(id);
  }
}

auto CMakeToolManager::defaultCMakeTool() -> CMakeTool*
{
  return findById(d->m_defaultCMake);
}

auto CMakeToolManager::setDefaultCMakeTool(const Id &id) -> void
{
  if (d->m_defaultCMake != id && findById(id)) {
    d->m_defaultCMake = id;
    emit m_instance->defaultCMakeChanged();
    return;
  }

  ensureDefaultCMakeToolIsValid();
}

auto CMakeToolManager::findByCommand(const Utils::FilePath &command) -> CMakeTool*
{
  return Utils::findOrDefault(d->m_cmakeTools, Utils::equal(&CMakeTool::cmakeExecutable, command));
}

auto CMakeToolManager::findById(const Id &id) -> CMakeTool*
{
  return Utils::findOrDefault(d->m_cmakeTools, Utils::equal(&CMakeTool::id, id));
}

auto CMakeToolManager::restoreCMakeTools() -> void
{
  auto tools = d->m_accessor.restoreCMakeTools(ICore::dialogParent());
  d->m_cmakeTools = std::move(tools.cmakeTools);
  setDefaultCMakeTool(tools.defaultToolId);

  updateDocumentation();

  emit m_instance->cmakeToolsLoaded();
}

auto CMakeToolManager::updateDocumentation() -> void
{
  const auto tools = cmakeTools();
  QStringList docs;
  for (const auto tool : tools) {
    if (!tool->qchFilePath().isEmpty())
      docs.append(tool->qchFilePath().toString());
  }
  Core::HelpManager::registerDocumentation(docs);
}

auto CMakeToolManager::autoDetectCMakeForDevice(const FilePaths &searchPaths, const QString &detectionSource, QString *logMessage) -> void
{
  QStringList messages{tr("Searching CMake binaries...")};
  for (const auto &path : searchPaths) {
    const auto cmake = path.pathAppended("cmake").withExecutableSuffix();
    if (cmake.isExecutableFile()) {
      registerCMakeByPath(cmake, detectionSource);
      messages.append(tr("Found \"%1\"").arg(cmake.toUserOutput()));
    }
  }
  if (logMessage)
    *logMessage = messages.join('\n');
}

auto CMakeToolManager::registerCMakeByPath(const FilePath &cmakePath, const QString &detectionSource) -> void
{
  const auto id = Id::fromString(cmakePath.toUserOutput());

  auto cmakeTool = findById(id);
  if (cmakeTool)
    return;

  auto newTool = std::make_unique<CMakeTool>(CMakeTool::ManualDetection, id);
  newTool->setFilePath(cmakePath);
  newTool->setDetectionSource(detectionSource);
  newTool->setDisplayName(cmakePath.toUserOutput());
  registerCMakeTool(std::move(newTool));
}

auto CMakeToolManager::removeDetectedCMake(const QString &detectionSource, QString *logMessage) -> void
{
  QStringList logMessages{tr("Removing CMake entries...")};
  while (true) {
    auto toRemove = Utils::take(d->m_cmakeTools, Utils::equal(&CMakeTool::detectionSource, detectionSource));
    if (!toRemove.has_value())
      break;
    logMessages.append(tr("Removed \"%1\"").arg((*toRemove)->displayName()));
    emit m_instance->cmakeRemoved((*toRemove)->id());
  }

  ensureDefaultCMakeToolIsValid();
  updateDocumentation();
  if (logMessage)
    *logMessage = logMessages.join('\n');
}

auto CMakeToolManager::listDetectedCMake(const QString &detectionSource, QString *logMessage) -> void
{
  QTC_ASSERT(logMessage, return);
  QStringList logMessages{tr("CMake:")};
  for (const auto &tool : qAsConst(d->m_cmakeTools)) {
    if (tool->detectionSource() == detectionSource)
      logMessages.append(tool->displayName());
  }
  *logMessage = logMessages.join('\n');
}

auto CMakeToolManager::notifyAboutUpdate(CMakeTool *tool) -> void
{
  if (!tool || !Utils::contains(d->m_cmakeTools, tool))
    return;
  emit m_instance->cmakeUpdated(tool->id());
}

auto CMakeToolManager::saveCMakeTools() -> void
{
  d->m_accessor.saveCMakeTools(cmakeTools(), d->m_defaultCMake, ICore::dialogParent());
}

auto CMakeToolManager::ensureDefaultCMakeToolIsValid() -> void
{
  const auto oldId = d->m_defaultCMake;
  if (d->m_cmakeTools.size() == 0) {
    d->m_defaultCMake = Utils::Id();
  } else {
    if (findById(d->m_defaultCMake))
      return;
    auto cmakeTool = Utils::findOrDefault(cmakeTools(), [](CMakeTool *tool) { return tool->detectionSource().isEmpty(); });
    if (cmakeTool)
      d->m_defaultCMake = cmakeTool->id();
  }

  // signaling:
  if (oldId != d->m_defaultCMake) emit m_instance->defaultCMakeChanged();
}

} // namespace CMakeProjectManager
