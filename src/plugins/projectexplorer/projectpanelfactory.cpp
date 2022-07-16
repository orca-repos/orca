// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectpanelfactory.hpp"

#include "project.hpp"
#include "projectwindow.hpp"

using namespace ProjectExplorer::Internal;
using namespace Utils;

namespace ProjectExplorer {

static QList<ProjectPanelFactory*> s_factories;

ProjectPanelFactory::ProjectPanelFactory() : m_supportsFunction([](Project *) { return true; }) { }

auto ProjectPanelFactory::priority() const -> int
{
  return m_priority;
}

auto ProjectPanelFactory::setPriority(int priority) -> void
{
  m_priority = priority;
}

auto ProjectPanelFactory::displayName() const -> QString
{
  return m_displayName;
}

auto ProjectPanelFactory::setDisplayName(const QString &name) -> void
{
  m_displayName = name;
}

auto ProjectPanelFactory::registerFactory(ProjectPanelFactory *factory) -> void
{
  const auto it = std::lower_bound(s_factories.begin(), s_factories.end(), factory, [](ProjectPanelFactory *a, ProjectPanelFactory *b) {
    return (a->priority() == b->priority() && a < b) || a->priority() < b->priority();
  });

  s_factories.insert(it, factory);
}

auto ProjectPanelFactory::factories() -> QList<ProjectPanelFactory*>
{
  return s_factories;
}

auto ProjectPanelFactory::destroyFactories() -> void
{
  qDeleteAll(s_factories);
  s_factories.clear();
}

auto ProjectPanelFactory::id() const -> Id
{
  return m_id;
}

auto ProjectPanelFactory::setId(Id id) -> void
{
  m_id = id;
}

auto ProjectPanelFactory::createWidget(Project *project) const -> QWidget*
{
  return m_widgetCreator(project);
}

auto ProjectPanelFactory::setCreateWidgetFunction(const WidgetCreator &createWidgetFunction) -> void
{
  m_widgetCreator = createWidgetFunction;
}

auto ProjectPanelFactory::supports(Project *project) -> bool
{
  return m_supportsFunction(project);
}

auto ProjectPanelFactory::setSupportsFunction(std::function<bool (Project *)> function) -> void
{
  m_supportsFunction = function;
}

} // namespace ProjectExplorer
