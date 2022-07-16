// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakelocatorfilter.hpp"

#include "cmakebuildstep.hpp"
#include "cmakebuildsystem.hpp"
#include "cmakeproject.hpp"

#include <core/core-editor-manager.hpp>
#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/buildsteplist.hpp>
#include <projectexplorer/session.hpp>
#include <projectexplorer/target.hpp>

#include <utils/algorithm.hpp>

using namespace CMakeProjectManager;
using namespace CMakeProjectManager::Internal;
using namespace ProjectExplorer;
using namespace Utils;

CMakeTargetLocatorFilter::CMakeTargetLocatorFilter()
{
  connect(SessionManager::instance(), &SessionManager::projectAdded, this, &CMakeTargetLocatorFilter::projectListUpdated);
  connect(SessionManager::instance(), &SessionManager::projectRemoved, this, &CMakeTargetLocatorFilter::projectListUpdated);

  // Initialize the filter
  projectListUpdated();
}

auto CMakeTargetLocatorFilter::prepareSearch(const QString &entry) -> void
{
  m_result.clear();
  const auto projects = SessionManager::projects();
  for (auto p : projects) {
    auto cmakeProject = qobject_cast<const CMakeProject*>(p);
    if (!cmakeProject || !cmakeProject->activeTarget())
      continue;
    auto bs = qobject_cast<CMakeBuildSystem*>(cmakeProject->activeTarget()->buildSystem());
    if (!bs)
      continue;

    const auto buildTargets = bs->buildTargets();
    for (const auto &target : buildTargets) {
      if (CMakeBuildSystem::filteredOutTarget(target))
        continue;
      const int index = target.title.indexOf(entry);
      if (index >= 0) {
        const auto path = target.backtrace.isEmpty() ? cmakeProject->projectFilePath() : target.backtrace.last().path;
        const auto line = target.backtrace.isEmpty() ? -1 : target.backtrace.last().line;

        QVariantMap extraData;
        extraData.insert("project", cmakeProject->projectFilePath().toString());
        extraData.insert("line", line);
        extraData.insert("file", path.toString());

        Orca::Plugin::Core::LocatorFilterEntry filterEntry(this, target.title, extraData);
        filterEntry.extra_info = path.shortNativePath();
        filterEntry.highlight_info = {index, int(entry.length())};
        filterEntry.file_path = path;

        m_result.append(filterEntry);
      }
    }
  }
}

auto CMakeTargetLocatorFilter::matchesFor(QFutureInterface<Orca::Plugin::Core::LocatorFilterEntry> &future, const QString &entry) -> QList<Orca::Plugin::Core::LocatorFilterEntry>
{
  Q_UNUSED(future)
  Q_UNUSED(entry)
  return m_result;
}

auto CMakeTargetLocatorFilter::projectListUpdated() -> void
{
  // Enable the filter if there's at least one CMake project
  setEnabled(Utils::contains(SessionManager::projects(), [](Project *p) { return qobject_cast<CMakeProject*>(p); }));
}

BuildCMakeTargetLocatorFilter::BuildCMakeTargetLocatorFilter()
{
  setId("Build CMake target");
  setDisplayName(tr("Build CMake target"));
  setDescription(tr("Builds a target of any open CMake project."));
  setDefaultShortcutString("cm");
  setPriority(High);
}

auto BuildCMakeTargetLocatorFilter::accept(const Orca::Plugin::Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void
{
  Q_UNUSED(newText)
  Q_UNUSED(selectionStart)
  Q_UNUSED(selectionLength)

  const auto extraData = selection.internal_data.toMap();
  const auto projectPath = FilePath::fromString(extraData.value("project").toString());

  // Get the project containing the target selected
  const auto cmakeProject = qobject_cast<CMakeProject*>(Utils::findOrDefault(SessionManager::projects(), [projectPath](Project *p) {
    return p->projectFilePath() == projectPath;
  }));
  if (!cmakeProject || !cmakeProject->activeTarget() || !cmakeProject->activeTarget()->activeBuildConfiguration())
    return;

  // Find the make step
  auto buildStepList = cmakeProject->activeTarget()->activeBuildConfiguration()->buildSteps();
  auto buildStep = buildStepList->firstOfType<CMakeBuildStep>();
  if (!buildStep)
    return;

  // Change the make step to build only the given target
  auto oldTargets = buildStep->buildTargets();
  buildStep->setBuildTargets({selection.display_name});

  // Build
  BuildManager::buildProjectWithDependencies(cmakeProject);
  buildStep->setBuildTargets(oldTargets);
}

OpenCMakeTargetLocatorFilter::OpenCMakeTargetLocatorFilter()
{
  setId("Open CMake target definition");
  setDisplayName(tr("Open CMake target"));
  setDescription(tr("Jumps to the definition of a target of any open CMake project."));
  setDefaultShortcutString("cmo");
  setPriority(Medium);
}

auto OpenCMakeTargetLocatorFilter::accept(const Orca::Plugin::Core::LocatorFilterEntry &selection, QString *newText, int *selectionStart, int *selectionLength) const -> void
{
  Q_UNUSED(newText)
  Q_UNUSED(selectionStart)
  Q_UNUSED(selectionLength)

  const auto extraData = selection.internal_data.toMap();
  const auto line = extraData.value("line").toInt();
  const auto file = FilePath::fromVariant(extraData.value("file"));

  if (line >= 0)
    Orca::Plugin::Core::EditorManager::openEditorAt({file, line}, {}, Orca::Plugin::Core::EditorManager::AllowExternalEditor);
  else
    Orca::Plugin::Core::EditorManager::openEditor(file, {}, Orca::Plugin::Core::EditorManager::AllowExternalEditor);
}
