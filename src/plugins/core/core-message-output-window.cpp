// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-message-output-window.hpp"

#include "core-constants.hpp"
#include "core-context-interface.hpp"
#include "core-interface.hpp"
#include "core-output-window.hpp"

namespace Orca::Plugin::Core {

constexpr char g_zoom_settings_key[] = "Core/MessageOutput/Zoom";

MessageOutputWindow::MessageOutputWindow()
{
  m_widget = new OutputWindow(Context(C_GENERAL_OUTPUT_PANE), g_zoom_settings_key);
  m_widget->setReadOnly(true);

  connect(this, &IOutputPane::zoomInRequested, m_widget, &Orca::Plugin::Core::OutputWindow::zoomIn);
  connect(this, &IOutputPane::zoomOutRequested, m_widget, &Orca::Plugin::Core::OutputWindow::zoomOut);
  connect(this, &IOutputPane::resetZoomRequested, m_widget, &Orca::Plugin::Core::OutputWindow::resetZoom);
  connect(this, &IOutputPane::fontChanged, m_widget, &OutputWindow::setBaseFont);
  connect(this, &IOutputPane::wheelZoomEnabledChanged, m_widget, &OutputWindow::setWheelZoomEnabled);

  setupFilterUi("MessageOutputPane.Filter");
  setFilteringEnabled(true);
  setupContext(C_GENERAL_OUTPUT_PANE, m_widget);
}

MessageOutputWindow::~MessageOutputWindow()
{
  delete m_widget;
}

auto MessageOutputWindow::hasFocus() const -> bool
{
  return m_widget->window()->focusWidget() == m_widget;
}

auto MessageOutputWindow::canFocus() const -> bool
{
  return true;
}

auto MessageOutputWindow::setFocus() -> void
{
  m_widget->setFocus();
}

auto MessageOutputWindow::clearContents() -> void
{
  m_widget->clear();
}

auto MessageOutputWindow::outputWidget(QWidget *parent) -> QWidget*
{
  m_widget->setParent(parent);
  return m_widget;
}

auto MessageOutputWindow::displayName() const -> QString
{
  return tr("General Messages");
}

auto MessageOutputWindow::append(const QString &text) const -> void
{
  m_widget->appendMessage(text, Utils::GeneralMessageFormat);
}

auto MessageOutputWindow::priorityInStatusBar() const -> int
{
  return -1;
}

auto MessageOutputWindow::canNext() const -> bool
{
  return false;
}

auto MessageOutputWindow::canPrevious() const -> bool
{
  return false;
}

auto MessageOutputWindow::goToNext() -> void {}
auto MessageOutputWindow::goToPrev() -> void {}

auto MessageOutputWindow::canNavigate() const -> bool
{
  return false;
}

auto MessageOutputWindow::updateFilter() -> void
{
  m_widget->updateFilterProperties(filterText(), filterCaseSensitivity(), filterUsesRegexp(), filterIsInverted());
}

} // namespace Orca::Plugin::Core
