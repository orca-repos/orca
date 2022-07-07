// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "styledbar.hpp"

#include <QPainter>
#include <QStyleOption>

using namespace Utils;

StyledBar::StyledBar(QWidget *parent) : QWidget(parent)
{
  setProperty("panelwidget", true);
  setProperty("panelwidget_singlerow", true);
  setProperty("lightColored", false);
}

auto StyledBar::setSingleRow(bool singleRow) -> void
{
  setProperty("panelwidget_singlerow", singleRow);
}

auto StyledBar::isSingleRow() const -> bool
{
  return property("panelwidget_singlerow").toBool();
}

auto StyledBar::setLightColored(bool lightColored) -> void
{
  if (isLightColored() == lightColored)
    return;
  setProperty("lightColored", lightColored);
  const QList<QWidget*> children = findChildren<QWidget*>();
  for (QWidget *childWidget : children)
    childWidget->style()->polish(childWidget);
}

auto StyledBar::isLightColored() const -> bool
{
  return property("lightColored").toBool();
}

auto StyledBar::paintEvent(QPaintEvent *event) -> void
{
  Q_UNUSED(event)
  QPainter painter(this);
  QStyleOptionToolBar option;
  option.rect = rect();
  option.state = QStyle::State_Horizontal;
  style()->drawControl(QStyle::CE_ToolBar, &option, &painter, this);
}

StyledSeparator::StyledSeparator(QWidget *parent) : QWidget(parent)
{
  setFixedWidth(10);
}

auto StyledSeparator::paintEvent(QPaintEvent *event) -> void
{
  Q_UNUSED(event)
  QPainter painter(this);
  QStyleOption option;
  option.rect = rect();
  option.state = QStyle::State_Horizontal;
  option.palette = palette();
  style()->drawPrimitive(QStyle::PE_IndicatorToolBarSeparator, &option, &painter, this);
}
