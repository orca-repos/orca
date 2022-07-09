// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/project.hpp>

namespace Utils { class ParameterAction; }

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

namespace CMakeProjectManager {
namespace Internal {

class CMakeManager : public QObject {
  Q_OBJECT

public:
  CMakeManager();

private:
  auto updateCmakeActions(ProjectExplorer::Node *node) -> void;
  auto clearCMakeCache(ProjectExplorer::BuildSystem *buildSystem) -> void;
  auto runCMake(ProjectExplorer::BuildSystem *buildSystem) -> void;
  auto rescanProject(ProjectExplorer::BuildSystem *buildSystem) -> void;
  auto buildFileContextMenu() -> void;
  auto buildFile(ProjectExplorer::Node *node = nullptr) -> void;
  auto updateBuildFileAction() -> void;
  auto enableBuildFileMenus(ProjectExplorer::Node *node) -> void;

  QAction *m_runCMakeAction;
  QAction *m_clearCMakeCacheAction;
  QAction *m_runCMakeActionContextMenu;
  QAction *m_rescanProjectAction;
  QAction *m_buildFileContextMenu;
  Utils::ParameterAction *m_buildFileAction;
};

} // namespace Internal
} // namespace CMakeProjectManager
