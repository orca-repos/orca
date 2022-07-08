// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "panelswidget.hpp"

#include <utils/qtcassert.hpp>
#include <utils/styledbar.hpp>
#include <utils/stylehelper.hpp>
#include <utils/theme/theme.hpp>

#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

using namespace Utils;

namespace ProjectExplorer {
namespace {

constexpr int ABOVE_HEADING_MARGIN = 10;
constexpr int ABOVE_CONTENTS_MARGIN = 4;
constexpr int BELOW_CONTENTS_MARGIN = 16;

}

///
// PanelsWidget
///

PanelsWidget::PanelsWidget(QWidget *parent) : QWidget(parent)
{
  m_root = new QWidget(nullptr);
  m_root->setFocusPolicy(Qt::NoFocus);
  m_root->setContentsMargins(0, 0, 0, 0);

  const auto scroller = new QScrollArea(this);
  scroller->setWidget(m_root);
  scroller->setFrameStyle(QFrame::NoFrame);
  scroller->setWidgetResizable(true);
  scroller->setFocusPolicy(Qt::NoFocus);

  // The layout holding the individual panels:
  const auto topLayout = new QVBoxLayout(m_root);
  topLayout->setContentsMargins(PanelVMargin, 0, PanelVMargin, 0);
  topLayout->setSpacing(0);

  m_layout = new QVBoxLayout;
  m_layout->setSpacing(0);

  topLayout->addLayout(m_layout);
  topLayout->addStretch(100);

  const auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(new StyledBar(this));
  layout->addWidget(scroller);

  //layout->addWidget(new FindToolBarPlaceHolder(this));
}

PanelsWidget::PanelsWidget(const QString &displayName, QWidget *widget) : PanelsWidget(nullptr)
{
  addPropertiesPanel(displayName, widget);
}

PanelsWidget::~PanelsWidget() = default;

/*
 * Add a widget with heading information into the layout of the PanelsWidget.
 *
 *     ...
 * +------------+ ABOVE_HEADING_MARGIN
 * | name       |
 * +------------+
 * | line       |
 * +------------+ ABOVE_CONTENTS_MARGIN
 * | widget     |
 * +------------+ BELOW_CONTENTS_MARGIN
 */
auto PanelsWidget::addPropertiesPanel(const QString &displayName, QWidget *widget) -> void
{
  // name:
  const auto nameLabel = new QLabel(m_root);
  nameLabel->setText(displayName);
  nameLabel->setContentsMargins(0, ABOVE_HEADING_MARGIN, 0, 0);
  auto f = nameLabel->font();
  f.setBold(true);
  f.setPointSizeF(f.pointSizeF() * 1.6);
  nameLabel->setFont(f);
  m_layout->addWidget(nameLabel);

  // line:
  const auto line = new QFrame(m_root);
  line->setFrameShape(QFrame::HLine);
  line->setForegroundRole(QPalette::Midlight);
  line->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  m_layout->addWidget(line);

  // add the widget:
  widget->setContentsMargins(0, ABOVE_CONTENTS_MARGIN, 0, BELOW_CONTENTS_MARGIN);
  widget->setParent(m_root);
  m_layout->addWidget(widget);
}

} // ProjectExplorer
