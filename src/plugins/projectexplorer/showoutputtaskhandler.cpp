// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "showoutputtaskhandler.hpp"

#include "task.hpp"

#include <core/ioutputpane.hpp>
#include <core/outputwindow.hpp>
#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

#include <QAction>

namespace ProjectExplorer {
namespace Internal {

ShowOutputTaskHandler::ShowOutputTaskHandler(Core::IOutputPane *window, const QString &text, const QString &tooltip, const QString &shortcut) : m_window(window), m_text(text), m_tooltip(tooltip), m_shortcut(shortcut)
{
  QTC_CHECK(m_window);
  QTC_CHECK(!m_text.isEmpty());
}

auto ShowOutputTaskHandler::canHandle(const Task &task) const -> bool
{
  return Utils::anyOf(m_window->outputWindows(), [task](const Core::OutputWindow *ow) {
    return ow->knowsPositionOf(task.taskId);
  });
}

auto ShowOutputTaskHandler::handle(const Task &task) -> void
{
  Q_ASSERT(canHandle(task));
  // popup first as this does move the visible area!
  m_window->popup(Core::IOutputPane::Flags(Core::IOutputPane::ModeSwitch | Core::IOutputPane::WithFocus));
  for (const auto ow : m_window->outputWindows()) {
    if (ow->knowsPositionOf(task.taskId)) {
      m_window->ensureWindowVisible(ow);
      ow->showPositionOf(task.taskId);
      break;
    }
  }
}

auto ShowOutputTaskHandler::createAction(QObject *parent) const -> QAction*
{
  const auto outputAction = new QAction(m_text, parent);
  if (!m_tooltip.isEmpty())
    outputAction->setToolTip(m_tooltip);
  if (!m_shortcut.isEmpty())
    outputAction->setShortcut(QKeySequence(m_shortcut));
  outputAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  return outputAction;
}

} // namespace Internal
} // namespace ProjectExplorer
