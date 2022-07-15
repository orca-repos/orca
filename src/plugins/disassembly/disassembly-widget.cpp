// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "disassembly-widget.hpp"

namespace Orca::Plugin::Disassembly {

class DisassemblyWidgetPrivate : public DisassemblyService {
public:
  DisassemblyWidgetPrivate(DisassemblyWidget *widget);
  ~DisassemblyWidgetPrivate() override;

  auto widget() -> QWidget* override;
  auto editor() -> Core::IEditor* override;

private:
  DisassemblyWidget *q;
};

DisassemblyWidgetPrivate::DisassemblyWidgetPrivate(DisassemblyWidget *widget): q(widget) {}
DisassemblyWidgetPrivate::~DisassemblyWidgetPrivate() = default;

auto DisassemblyWidgetPrivate::widget() -> QWidget*
{
  return q;
}

auto DisassemblyWidgetPrivate::editor() -> Core::IEditor*
{
  return q->editor();
}

DisassemblyWidget::DisassemblyWidget(QWidget *parent) : QAbstractScrollArea(parent), d(new DisassemblyWidgetPrivate(this))
{
#ifdef _DEBUG
  setStyleSheet("QWidget {  background-color: red; }");
#endif
  setFocusPolicy(Qt::WheelFocus);
  setFrameStyle(Plain);
}

DisassemblyWidget::~DisassemblyWidget()
{
  delete d;
}

auto DisassemblyWidget::disassemblyService() const -> DisassemblyService*
{
  return d;
}

auto DisassemblyWidget::editor() const -> Core::IEditor*
{
  assert(m_editor_interface);
  return m_editor_interface;
}

auto DisassemblyWidget::setEditor(Core::IEditor *editor_interface) -> void
{
  m_editor_interface = editor_interface;
}

} // namespace Orca::Plugin::Disassembly
