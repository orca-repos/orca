// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "disassembly.hpp"

namespace Orca::Plugin::Disassembly {

Disassembly::Disassembly(DisassemblyWidget *widget)
{
  IContext::setWidget(widget);

  m_file = new DisassemblyDocument(widget);

  const auto layout = new QHBoxLayout;
  const auto central_widget = new QWidget;

  layout->setContentsMargins(0, 0, 5, 0);
  layout->addStretch(1);

  central_widget->setLayout(layout);

  m_tool_bar = new QToolBar;
  m_tool_bar->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
  m_tool_bar->addWidget(central_widget);

  widget->setEditor(this);
}

Disassembly::~Disassembly()
{
  delete m_widget;
}

auto Disassembly::document() const -> DisassemblyDocument*
{
  return m_file;
}

auto Disassembly::toolBar() -> QWidget*
{
  return m_tool_bar;
}

} // namespace Orca::Plugin::Disassembly
