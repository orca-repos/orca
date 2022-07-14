// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-progress-view.hpp"

#include <QEvent>
#include <QVBoxLayout>

namespace Orca::Plugin::Core {

ProgressView::ProgressView(QWidget *parent) : QWidget(parent)
{
  m_layout = new QVBoxLayout;
  setLayout(m_layout);
  m_layout->setContentsMargins(0, 0, 0, 1);
  m_layout->setSpacing(0);
  m_layout->setSizeConstraint(QLayout::SetFixedSize);
  setWindowTitle(tr("Processes"));
}

ProgressView::~ProgressView() = default;

auto ProgressView::addProgressWidget(QWidget *widget) const -> void
{
  m_layout->insertWidget(0, widget);
}

auto ProgressView::removeProgressWidget(QWidget *widget) const -> void
{
  m_layout->removeWidget(widget);
}

auto ProgressView::isHovered() const -> bool
{
  return m_hovered;
}

auto ProgressView::setReferenceWidget(QWidget *widget) -> void
{
  if (m_reference_widget)
    removeEventFilter(this);

  m_reference_widget = widget;

  if (m_reference_widget)
    installEventFilter(this);

  reposition();
}

auto ProgressView::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::ParentAboutToChange && parentWidget()) {
    parentWidget()->removeEventFilter(this);
  } else if (event->type() == QEvent::ParentChange && parentWidget()) {
    parentWidget()->installEventFilter(this);
  } else if (event->type() == QEvent::Resize) {
    reposition();
  } else if (event->type() == QEvent::Enter) {
    m_hovered = true;
    emit hoveredChanged(m_hovered);
  } else if (event->type() == QEvent::Leave) {
    m_hovered = false;
    emit hoveredChanged(m_hovered);
  }
  return QWidget::event(event);
}

auto ProgressView::eventFilter(QObject *obj, QEvent *event) -> bool
{
  if ((obj == parentWidget() || obj == m_reference_widget) && event->type() == QEvent::Resize)
    reposition();

  return false;
}

auto ProgressView::reposition() -> void
{
  if (!parentWidget() || !m_reference_widget)
    return;

  const auto top_right_reference_in_parent = m_reference_widget->mapTo(parentWidget(), m_reference_widget->rect().topRight());
  move(top_right_reference_in_parent - rect().bottomRight());
}

} // namespace Orca::Plugin::Core
