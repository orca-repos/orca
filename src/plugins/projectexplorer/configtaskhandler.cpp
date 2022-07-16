// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "configtaskhandler.hpp"

#include "task.hpp"
#include "taskhub.hpp"

#include <core/core-interface.hpp>

#include <QAction>

#include <utils/qtcassert.hpp>

using namespace ProjectExplorer;
using namespace Internal;

ConfigTaskHandler::ConfigTaskHandler(const Task &pattern, Utils::Id page) : m_pattern(pattern), m_targetPage(page) { }

auto ConfigTaskHandler::canHandle(const Task &task) const -> bool
{
  return task.description() == m_pattern.description() && task.category == m_pattern.category;
}

auto ConfigTaskHandler::handle(const Task &task) -> void
{
  Q_UNUSED(task)
  Orca::Plugin::Core::ICore::showOptionsDialog(m_targetPage);
}

auto ConfigTaskHandler::createAction(QObject *parent) const -> QAction*
{
  const auto action = new QAction(Orca::Plugin::Core::ICore::msgShowOptionsDialog(), parent);
  action->setToolTip(Orca::Plugin::Core::ICore::msgShowOptionsDialogToolTip());
  return action;
}
