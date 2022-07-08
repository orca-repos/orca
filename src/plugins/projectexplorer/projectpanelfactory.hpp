// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "panelswidget.hpp"
#include "projectwindow.hpp"

#include <utils/id.hpp>
#include <utils/treemodel.hpp>

#include <functional>

namespace ProjectExplorer {

class Project;
class ProjectExplorerPlugin;

class PROJECTEXPLORER_EXPORT ProjectPanelFactory {
public:
  ProjectPanelFactory();

  auto id() const -> Utils::Id;
  auto setId(Utils::Id id) -> void;
  // simple properties
  auto displayName() const -> QString;
  auto setDisplayName(const QString &name) -> void;
  auto priority() const -> int;
  auto setPriority(int priority) -> void;
  // interface for users of ProjectPanelFactory
  auto supports(Project *project) -> bool;
  using WidgetCreator = std::function<QWidget *(Project *)>;
  // interface for "implementations" of ProjectPanelFactory
  // by default all projects are supported, only set a custom supports function
  // if you need something different
  using SupportsFunction = std::function<bool (Project *)>;
  auto setSupportsFunction(std::function<bool (Project *)> function) -> void;
  // This takes ownership.
  static auto registerFactory(ProjectPanelFactory *factory) -> void;
  static auto factories() -> QList<ProjectPanelFactory*>;
  auto createPanelItem(Project *project) -> Utils::TreeItem*;
  auto setCreateWidgetFunction(const WidgetCreator &createWidgetFunction) -> void;
  auto createWidget(Project *project) const -> QWidget*;

private:
  friend class ProjectExplorerPlugin;
  static auto destroyFactories() -> void;

  Utils::Id m_id;
  int m_priority = 0;
  QString m_displayName;
  SupportsFunction m_supportsFunction;
  WidgetCreator m_widgetCreator;
};

} // namespace ProjectExplorer
