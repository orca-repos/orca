// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "detailswidget.h"
#include "detailsbutton.h"
#include "hostosinfo.h"
#include "theme/theme.h"

#include <QGridLayout>
#include <QLabel>
#include <QCheckBox>
#include <QPainter>
#include <QScrollArea>
#include <QApplication>
#include <QStyle>

/*!
    \class Utils::DetailsWidget

    \brief The DetailsWidget class implements a button to expand a \e Details
    area.

    This widget is using a grid layout and places the items
    in the following way:

    \code
+------------+-------------------------+---------------+
+summaryLabel|              toolwidget | detailsButton |
+------------+-------------------------+---------------+
+                additional summary                    |
+------------+-------------------------+---------------+
|                  widget                              |
+------------+-------------------------+---------------+
    \endcode
*/

namespace Utils {

static const int MARGIN = 8;

class DetailsWidgetPrivate {
public:
  DetailsWidgetPrivate(QWidget *parent);

  auto updateControls() -> void;
  auto changeHoverState(bool hovered) -> void;

  QWidget *q;
  DetailsButton *m_detailsButton;
  QGridLayout *m_grid;
  QLabel *m_summaryLabelIcon;
  QLabel *m_summaryLabel;
  QCheckBox *m_summaryCheckBox;
  QLabel *m_additionalSummaryLabel;
  FadingPanel *m_toolWidget;
  QWidget *m_widget;

  QPixmap m_collapsedPixmap;
  QPixmap m_expandedPixmap;

  DetailsWidget::State m_state;
  bool m_hovered;
  bool m_useCheckBox;
};

DetailsWidgetPrivate::DetailsWidgetPrivate(QWidget *parent) : q(parent), m_detailsButton(new DetailsButton), m_grid(new QGridLayout), m_summaryLabelIcon(new QLabel(parent)), m_summaryLabel(new QLabel(parent)), m_summaryCheckBox(new QCheckBox(parent)), m_additionalSummaryLabel(new QLabel(parent)), m_toolWidget(nullptr), m_widget(nullptr), m_state(DetailsWidget::Collapsed), m_hovered(false), m_useCheckBox(false)
{
  auto summaryLayout = new QHBoxLayout;
  summaryLayout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
  summaryLayout->setSpacing(0);

  m_summaryLabelIcon->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  m_summaryLabelIcon->setContentsMargins(0, 0, 0, 0);
  m_summaryLabelIcon->setFixedWidth(0);
  summaryLayout->addWidget(m_summaryLabelIcon);

  m_summaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
  m_summaryLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Minimum);
  m_summaryLabel->setContentsMargins(0, 0, 0, 0);
  summaryLayout->addWidget(m_summaryLabel, 1);

  m_summaryCheckBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_summaryCheckBox->setAttribute(Qt::WA_LayoutUsesWidgetRect); /* broken layout on mac otherwise */
  m_summaryCheckBox->setVisible(false);
  m_summaryCheckBox->setContentsMargins(0, 0, 0, 0);
  summaryLayout->addWidget(m_summaryCheckBox);

  m_additionalSummaryLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_additionalSummaryLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
  m_additionalSummaryLabel->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
  m_additionalSummaryLabel->setWordWrap(true);
  m_additionalSummaryLabel->setVisible(false);

  m_grid->setContentsMargins(0, 0, 0, 0);
  m_grid->setSpacing(0);
  m_grid->addLayout(summaryLayout, 0, 0);
  m_grid->addWidget(m_detailsButton, 0, 2);
  m_grid->addWidget(m_additionalSummaryLabel, 1, 0, 1, 3);
}

auto DetailsWidget::createBackground(const QSize &size, int topHeight, QWidget *widget) -> QPixmap
{
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);
  QPainter p(&pixmap);

  QRect topRect(0, 0, size.width(), topHeight);
  QRect fullRect(0, 0, size.width(), size.height());
  if (HostOsInfo::isMacHost())
    p.fillRect(fullRect, QApplication::palette().window().color());
  else
    p.fillRect(fullRect, orcaTheme()->color(Theme::DetailsWidgetBackgroundColor));

  if (!orcaTheme()->flag(Theme::FlatProjectsMode)) {
    QLinearGradient lg(topRect.topLeft(), topRect.bottomLeft());
    lg.setStops(orcaTheme()->gradient(Theme::DetailsWidgetHeaderGradient));
    p.fillRect(topRect, lg);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.translate(0.5, 0.5);
    p.setPen(QColor(0, 0, 0, 40));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(fullRect.adjusted(0, 0, -1, -1), 2, 2);
    p.setBrush(Qt::NoBrush);
    p.setPen(QColor(255, 255, 255, 140));
    p.drawRoundedRect(fullRect.adjusted(1, 1, -2, -2), 2, 2);
    p.setPen(QPen(widget->palette().color(QPalette::Mid)));
  }

  return pixmap;
}

auto DetailsWidgetPrivate::updateControls() -> void
{
  if (m_widget)
    m_widget->setVisible(m_state == DetailsWidget::Expanded || m_state == DetailsWidget::NoSummary);
  m_detailsButton->setChecked(m_state == DetailsWidget::Expanded && m_widget);
  m_detailsButton->setVisible(m_state == DetailsWidget::Expanded || m_state == DetailsWidget::Collapsed);
  m_summaryLabelIcon->setVisible(m_state != DetailsWidget::NoSummary && !m_useCheckBox);
  m_summaryLabel->setVisible(m_state != DetailsWidget::NoSummary && !m_useCheckBox);
  m_summaryCheckBox->setVisible(m_state != DetailsWidget::NoSummary && m_useCheckBox);

  for (QWidget *w = q; w; w = w->parentWidget()) {
    if (w->layout())
      w->layout()->activate();
    if (auto area = qobject_cast<QScrollArea*>(w)) {
      QEvent e(QEvent::LayoutRequest);
      QCoreApplication::sendEvent(area, &e);
    }
  }
}

auto DetailsWidgetPrivate::changeHoverState(bool hovered) -> void
{
  if (!m_toolWidget)
    return;
  if (HostOsInfo::isMacHost())
    m_toolWidget->setOpacity(hovered ? .999 : 0);
  else
    m_toolWidget->fadeTo(hovered ? .999 : 0);
  m_hovered = hovered;
}

DetailsWidget::DetailsWidget(QWidget *parent) : QWidget(parent), d(new DetailsWidgetPrivate(this))
{
  setLayout(d->m_grid);

  setUseCheckBox(false);

  connect(d->m_detailsButton, &QAbstractButton::toggled, this, &DetailsWidget::setExpanded);
  connect(d->m_summaryCheckBox, &QAbstractButton::toggled, this, &DetailsWidget::checked);
  connect(d->m_summaryLabel, &QLabel::linkActivated, this, &DetailsWidget::linkActivated);
  d->updateControls();
}

DetailsWidget::~DetailsWidget()
{
  delete d;
}

auto DetailsWidget::useCheckBox() -> bool
{
  return d->m_useCheckBox;
}

auto DetailsWidget::setUseCheckBox(bool b) -> void
{
  d->m_useCheckBox = b;
  d->updateControls();
}

auto DetailsWidget::setCheckable(bool b) -> void
{
  d->m_summaryCheckBox->setEnabled(b);
}

auto DetailsWidget::setExpandable(bool b) -> void
{
  d->m_detailsButton->setEnabled(b);
}

auto DetailsWidget::setChecked(bool b) -> void
{
  d->m_summaryCheckBox->setChecked(b);
}

auto DetailsWidget::isChecked() const -> bool
{
  return d->m_useCheckBox && d->m_summaryCheckBox->isChecked();
}

auto DetailsWidget::setSummaryFontBold(bool b) -> void
{
  QFont f;
  f.setBold(b);
  d->m_summaryCheckBox->setFont(f);
  d->m_summaryLabel->setFont(f);
}

auto DetailsWidget::setIcon(const QIcon &icon) -> void
{
  int iconSize = style()->pixelMetric(QStyle::PM_ButtonIconSize, nullptr, this);
  d->m_summaryLabelIcon->setFixedWidth(icon.isNull() ? 0 : iconSize);
  d->m_summaryLabelIcon->setPixmap(icon.pixmap(iconSize, iconSize));
  d->m_summaryCheckBox->setIcon(icon);
}

auto DetailsWidget::paintEvent(QPaintEvent *paintEvent) -> void
{
  QWidget::paintEvent(paintEvent);

  QPainter p(this);

  QWidget *topLeftWidget = d->m_useCheckBox ? static_cast<QWidget*>(d->m_summaryCheckBox) : static_cast<QWidget*>(d->m_summaryLabelIcon);
  QPoint topLeft(topLeftWidget->geometry().left() - MARGIN, contentsRect().top());
  const QRect paintArea(topLeft, contentsRect().bottomRight());

  int topHeight = d->m_useCheckBox ? d->m_summaryCheckBox->height() : d->m_summaryLabel->height();
  if (d->m_state == DetailsWidget::Expanded || d->m_state == DetailsWidget::Collapsed) // Details Button is shown
    topHeight = qMax(d->m_detailsButton->height(), topHeight);

  if (d->m_state == Collapsed) {
    if (d->m_collapsedPixmap.isNull() || d->m_collapsedPixmap.size() != size())
      d->m_collapsedPixmap = createBackground(paintArea.size(), topHeight, this);
    p.drawPixmap(paintArea, d->m_collapsedPixmap);
  } else {
    if (d->m_expandedPixmap.isNull() || d->m_expandedPixmap.size() != size())
      d->m_expandedPixmap = createBackground(paintArea.size(), topHeight, this);
    p.drawPixmap(paintArea, d->m_expandedPixmap);
  }
}

auto DetailsWidget::enterEvent(EnterEvent *event) -> void
{
  QWidget::enterEvent(event);
  d->changeHoverState(true);
}

auto DetailsWidget::leaveEvent(QEvent *event) -> void
{
  QWidget::leaveEvent(event);
  d->changeHoverState(false);
}

auto DetailsWidget::setSummaryText(const QString &text) -> void
{
  if (d->m_useCheckBox)
    d->m_summaryCheckBox->setText(text);
  else
    d->m_summaryLabel->setText(text);
}

auto DetailsWidget::summaryText() const -> QString
{
  if (d->m_useCheckBox)
    return d->m_summaryCheckBox->text();
  return d->m_summaryLabel->text();
}

auto DetailsWidget::additionalSummaryText() const -> QString
{
  return d->m_additionalSummaryLabel->text();
}

auto DetailsWidget::setAdditionalSummaryText(const QString &text) -> void
{
  d->m_additionalSummaryLabel->setText(text);
  d->m_additionalSummaryLabel->setVisible(!text.isEmpty());
}

auto DetailsWidget::state() const -> DetailsWidget::State
{
  return d->m_state;
}

auto DetailsWidget::setState(State state) -> void
{
  if (state == d->m_state)
    return;
  d->m_state = state;
  d->updateControls();
  emit expanded(d->m_state == Expanded);
}

auto DetailsWidget::setExpanded(bool expanded) -> void
{
  setState(expanded ? Expanded : Collapsed);
}

auto DetailsWidget::widget() const -> QWidget*
{
  return d->m_widget;
}

auto DetailsWidget::takeWidget() -> QWidget*
{
  QWidget *widget = d->m_widget;
  d->m_widget = nullptr;
  d->m_grid->removeWidget(widget);
  if (widget)
    widget->setParent(nullptr);
  return widget;
}

auto DetailsWidget::setWidget(QWidget *widget) -> void
{
  if (d->m_widget == widget)
    return;

  if (d->m_widget) {
    d->m_grid->removeWidget(d->m_widget);
    delete d->m_widget;
  }

  d->m_widget = widget;

  if (d->m_widget) {
    d->m_widget->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
    d->m_grid->addWidget(d->m_widget, 2, 0, 1, 3);
  }
  d->updateControls();
}

auto DetailsWidget::setToolWidget(FadingPanel *widget) -> void
{
  if (d->m_toolWidget == widget)
    return;

  d->m_toolWidget = widget;

  if (!d->m_toolWidget)
    return;

  d->m_toolWidget->adjustSize();
  d->m_grid->addWidget(d->m_toolWidget, 0, 1, 1, 1, Qt::AlignRight);

  if (HostOsInfo::isMacHost())
    d->m_toolWidget->setOpacity(.999);
  d->changeHoverState(d->m_hovered);
}

auto DetailsWidget::toolWidget() const -> QWidget*
{
  return d->m_toolWidget;
}

} // namespace Utils
