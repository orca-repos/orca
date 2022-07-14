// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-manhattan-style.hpp"

#include "core-style-animator.hpp"

#include <utils/algorithm.hpp>
#include <utils/fancymainwindow.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stylehelper.hpp>
#include <utils/utilsicons.hpp>
#include <utils/theme/theme.hpp>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSpinBox>
#include <QStatusBar>
#include <QStyleFactory>
#include <QStyleOption>
#include <QToolBar>
#include <QToolButton>

using namespace Utils;

namespace Orca::Plugin::Core {

// We define a currently unused state for indicating animations
const QStyle::State State_Animating = QStyle::State(0x00000040);

// Because designer needs to disable this for widget previews
// we have a custom property that is inherited
auto styleEnabled(const QWidget *widget) -> bool
{
  auto p = widget;
  while (p) {
    if (p->property("_q_custom_style_disabled").toBool())
      return false;
    p = p->parentWidget();
  }
  return true;
}

static auto isInUnstyledDialogOrPopup(const QWidget *widget) -> bool
{
  // Do not style contents of dialogs or popups without "panelwidget" property
  const QWidget *window = widget->window();
  if (window->property("panelwidget").toBool())
    return false;
  const auto windowType = window->windowType();
  return (windowType == Qt::Dialog || windowType == Qt::Popup);
}

// Consider making this a QStyle state
auto panelWidget(const QWidget *widget) -> bool
{
  if (!widget)
    return false;

  if (isInUnstyledDialogOrPopup(widget))
    return false;

  if (qobject_cast<const FancyMainWindow*>(widget))
    return true;

  if (qobject_cast<const QTabBar*>(widget))
    return styleEnabled(widget);

  auto p = widget;
  while (p) {
    if (qobject_cast<const QToolBar*>(p) || qobject_cast<const QStatusBar*>(p) || qobject_cast<const QMenuBar*>(p) || p->property("panelwidget").toBool())
      return styleEnabled(widget);
    p = p->parentWidget();
  }
  return false;
}

// Consider making this a QStyle state
auto lightColored(const QWidget *widget) -> bool
{
  if (!widget)
    return false;

  if (isInUnstyledDialogOrPopup(widget))
    return false;

  auto p = widget;
  while (p) {
    if (p->property("lightColored").toBool())
      return true;
    p = p->parentWidget();
  }
  return false;
}

static auto isDarkFusionStyle(const QStyle *style) -> bool
{
  return orcaTheme()->flag(Theme::DarkUserInterface) && strcmp(style->metaObject()->className(), "QFusionStyle") == 0;
}

class ManhattanStylePrivate {
public:
  explicit ManhattanStylePrivate();
  auto init() -> void;

public:
  const QIcon extButtonIcon;
  const QPixmap closeButtonPixmap;
  StyleAnimator animator;
};

ManhattanStylePrivate::ManhattanStylePrivate() : extButtonIcon(Utils::Icons::TOOLBAR_EXTENSION.icon()), closeButtonPixmap(Utils::Icons::CLOSE_FOREGROUND.pixmap()) {}

ManhattanStyle::ManhattanStyle(const QString &baseStyleName) : QProxyStyle(QStyleFactory::create(baseStyleName)), d(new ManhattanStylePrivate()) {}

ManhattanStyle::~ManhattanStyle()
{
  delete d;
  d = nullptr;
}

auto ManhattanStyle::generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap, const QStyleOption *opt) const -> QPixmap
{
  return QProxyStyle::generatedIconPixmap(iconMode, pixmap, opt);
}

auto ManhattanStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const -> QSize
{
  auto newSize = QProxyStyle::sizeFromContents(type, option, size, widget);

  if (type == CT_Splitter && widget && widget->property("minisplitter").toBool())
    return {1, 1};
  else if (type == CT_ComboBox && panelWidget(widget))
    newSize += QSize(14, 0);
  return newSize;
}

auto ManhattanStyle::subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const -> QRect
{
  return QProxyStyle::subElementRect(element, option, widget);
}

auto ManhattanStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const -> QRect
{
  #if QT_VERSION < QT_VERSION_CHECK(6, 2, 5)
    // Workaround for QTBUG-101581, can be removed when building with Qt 6.2.5 or higher
    if (control == CC_ScrollBar) {
        const auto scrollbar = qstyleoption_cast<const QStyleOptionSlider *>(option);
        if (scrollbar && qint64(scrollbar->maximum) - scrollbar->minimum > INT_MAX)
            return QRect(); // breaks the scrollbar, but avoids the crash
    }
  #endif
  return QProxyStyle::subControlRect(control, option, subControl, widget);
}

auto ManhattanStyle::hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option, const QPoint &pos, const QWidget *widget) const -> QStyle::SubControl
{
  return QProxyStyle::hitTestComplexControl(control, option, pos, widget);
}

auto ManhattanStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const -> int
{
  auto retval = 0;
  retval = QProxyStyle::pixelMetric(metric, option, widget);
  switch (metric) {
  case PM_SplitterWidth:
    if (widget && widget->property("minisplitter").toBool())
      retval = 1;
    break;
  case PM_ToolBarIconSize:
  case PM_ButtonIconSize:
    if (panelWidget(widget))
      retval = 16;
    break;
  case PM_SmallIconSize:
    retval = 16;
    break;
  case PM_DockWidgetHandleExtent:
  case PM_DockWidgetSeparatorExtent:
    return 1;
  case PM_MenuPanelWidth:
  case PM_MenuBarHMargin:
  case PM_MenuBarVMargin:
  case PM_ToolBarFrameWidth:
    if (panelWidget(widget))
      retval = 1;
    break;
  case PM_ButtonShiftVertical:
  case PM_ButtonShiftHorizontal:
  case PM_MenuBarPanelWidth:
  case PM_ToolBarItemMargin:
  case PM_ToolBarItemSpacing:
    if (panelWidget(widget))
      retval = 0;
    break;
  case PM_DefaultFrameWidth:
    if (qobject_cast<const QLineEdit*>(widget) && panelWidget(widget))
      return 1;
    break;
  default:
    break;
  }
  return retval;
}

auto ManhattanStyle::standardPalette() const -> QPalette
{
  return QProxyStyle::standardPalette();
}

auto ManhattanStyle::polish(QApplication *app) -> void
{
  QProxyStyle::polish(app);
}

auto ManhattanStyle::unpolish(QApplication *app) -> void
{
  QProxyStyle::unpolish(app);
}

auto panelPalette(const QPalette &oldPalette, bool lightColored = false) -> QPalette
{
  auto color = orcaTheme()->color(lightColored ? Theme::PanelTextColorDark : Theme::PanelTextColorLight);
  auto pal = oldPalette;
  pal.setBrush(QPalette::All, QPalette::WindowText, color);
  pal.setBrush(QPalette::All, QPalette::ButtonText, color);
  if (lightColored)
    color.setAlpha(100);
  else
    color = orcaTheme()->color(Theme::IconsDisabledColor);
  pal.setBrush(QPalette::Disabled, QPalette::WindowText, color);
  pal.setBrush(QPalette::Disabled, QPalette::ButtonText, color);
  return pal;
}

auto ManhattanStyle::polish(QWidget *widget) -> void
{
  QProxyStyle::polish(widget);

  // OxygenStyle forces a rounded widget mask on toolbars and dock widgets
  if (baseStyle()->inherits("OxygenStyle") || baseStyle()->inherits("Oxygen::Style")) {
    if (qobject_cast<QToolBar*>(widget) || qobject_cast<QDockWidget*>(widget)) {
      widget->removeEventFilter(baseStyle());
      widget->setContentsMargins(0, 0, 0, 0);
    }
  }
  if (panelWidget(widget)) {

    // Oxygen and possibly other styles override this
    if (qobject_cast<QDockWidget*>(widget))
      widget->setContentsMargins(0, 0, 0, 0);

    widget->setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
    // So that text isn't cutoff in line-edits, comboboxes... etc.
    const auto height = qMax(StyleHelper::navigationWidgetHeight(), QApplication::fontMetrics().height());
    if (qobject_cast<QToolButton*>(widget) || qobject_cast<QLineEdit*>(widget)) {
      widget->setAttribute(Qt::WA_Hover);
      widget->setMaximumHeight(height - 2);
    } else if (qobject_cast<QLabel*>(widget) || qobject_cast<QSpinBox*>(widget) || qobject_cast<QCheckBox*>(widget)) {
      widget->setPalette(panelPalette(widget->palette(), lightColored(widget)));
    } else if (widget->property("panelwidget_singlerow").toBool()) {
      widget->setFixedHeight(height);
    } else if (qobject_cast<QStatusBar*>(widget)) {
      widget->setFixedHeight(height + 2);
    } else if (qobject_cast<QComboBox*>(widget)) {
      const auto isLightColored = lightColored(widget);
      auto palette = panelPalette(widget->palette(), isLightColored);
      if (!isLightColored)
        palette.setBrush(QPalette::All, QPalette::WindowText, orcaTheme()->color(Theme::ComboBoxTextColor));
      widget->setPalette(palette);
      widget->setMaximumHeight(height - 2);
      widget->setAttribute(Qt::WA_Hover);
    }
  }
}

auto ManhattanStyle::unpolish(QWidget *widget) -> void
{
  QProxyStyle::unpolish(widget);
  if (panelWidget(widget)) {
    widget->setAttribute(Qt::WA_LayoutUsesWidgetRect, false);
    if (qobject_cast<QTabBar*>(widget) || qobject_cast<QToolBar*>(widget) || qobject_cast<QComboBox*>(widget)) {
      widget->setAttribute(Qt::WA_Hover, false);
    }
  }
}

auto ManhattanStyle::polish(QPalette &pal) -> void
{
  QProxyStyle::polish(pal);
}

auto ManhattanStyle::standardPixmap(StandardPixmap standardPixmap, const QStyleOption *opt, const QWidget *widget) const -> QPixmap
{
  if (widget && !panelWidget(widget))
    return QProxyStyle::standardPixmap(standardPixmap, opt, widget);

  QPixmap pixmap;
  switch (standardPixmap) {
  case QStyle::SP_TitleBarCloseButton:
    pixmap = d->closeButtonPixmap;
    break;
  default:
    pixmap = QProxyStyle::standardPixmap(standardPixmap, opt, widget);
    break;
  }
  return pixmap;
}

auto ManhattanStyle::standardIcon(StandardPixmap standardIcon, const QStyleOption *option, const QWidget *widget) const -> QIcon
{
  QIcon icon;
  switch (standardIcon) {
  case QStyle::SP_ToolBarHorizontalExtensionButton:
    icon = d->extButtonIcon;
    break;
  default:
    icon = QProxyStyle::standardIcon(standardIcon, option, widget);
    break;
  }

  if (standardIcon == QStyle::SP_ComputerIcon) {
    // Ubuntu has in some versions a 16x16 icon, see ORCABUG-12832
    const auto &sizes = icon.availableSizes();
    if (Utils::allOf(sizes, [](const QSize &size) { return size.width() < 32; }))
      icon = QIcon(":/utils/images/Desktop.png");
  }
  return icon;
}

auto ManhattanStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const -> int
{
  auto ret = QProxyStyle::styleHint(hint, option, widget, returnData);
  switch (hint) {
  case QStyle::SH_EtchDisabledText:
    if (panelWidget(widget) || qobject_cast<const QMenu*>(widget))
      ret = false;
    break;
  case QStyle::SH_ItemView_ArrowKeysNavigateIntoChildren:
    ret = true;
    break;
  case QStyle::SH_ItemView_ActivateItemOnSingleClick:
    // default depends on the style
    if (widget) {
      auto activationMode = widget->property("ActivationMode");
      if (activationMode.isValid())
        ret = activationMode.toBool();
    }
    break;
  case QStyle::SH_FormLayoutFieldGrowthPolicy:
    // The default in QMacStyle, FieldsStayAtSizeHint, is just always the wrong thing
    // Use the same as on all other shipped styles
    if (Utils::HostOsInfo::isMacHost())
      ret = QFormLayout::AllNonFixedFieldsGrow;
    break;
  case QStyle::SH_Widget_Animation_Duration:
    if (widget && widget->inherits("QTreeView"))
      ret = 0;
    break;
  default:
    break;
  }
  return ret;
}

static auto drawPrimitiveTweakedForDarkTheme(QStyle::PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) -> void
{
  const bool hasFocus = option->state & QStyle::State_HasFocus;
  const bool isChecked = option->state & QStyle::State_On;
  const bool isPartiallyChecked = option->state & QStyle::State_NoChange;
  const bool isEnabled = option->state & QStyle::State_Enabled;
  const bool isSunken = option->state & QStyle::State_Sunken;

  const auto frameColor = isEnabled ? option->palette.color(QPalette::Mid).darker(132) : orcaTheme()->color(Theme::BackgroundColorDisabled);
  const auto indicatorColor = isEnabled ? option->palette.color(QPalette::Mid).darker(90) : orcaTheme()->color(Theme::BackgroundColorDisabled);
  const auto bgColor = isSunken ? option->palette.color(QPalette::Mid).darker() : option->palette.color(QPalette::Window);
  const auto hlColor = option->palette.color(QPalette::Highlight);

  QPen framePen(hasFocus ? hlColor : frameColor, 1);
  framePen.setJoinStyle(Qt::MiterJoin);
  QPen indicatorPen(indicatorColor, 1);
  indicatorPen.setJoinStyle(Qt::MiterJoin);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing);

  switch (element) {
  case QStyle::PE_Frame: {
    const auto frameRectF = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    painter->setPen(framePen);
    painter->drawRect(frameRectF);
    break;
  }
  case QStyle::PE_FrameLineEdit: {
    const auto isComboBox = widget && widget->inherits("QComboBox");
    const auto frameRectF = QRectF(option->rect).adjusted(0.5, 0.5, isComboBox ? -8.5 : -0.5, -0.5);
    painter->setPen(framePen);
    painter->drawRect(frameRectF);
    break;
  }
  case QStyle::PE_FrameGroupBox: {
    // Snippet from QFusionStyle::drawPrimitive - BEGIN
    static const auto groupBoxTopMargin = 3;
    auto topMargin = 0;
    auto control = dynamic_cast<const QGroupBox*>(widget);
    if (control && !control->isCheckable() && control->title().isEmpty()) {
      // Shrinking the topMargin if Not checkable AND title is empty
      topMargin = groupBoxTopMargin;
    } else {
      const auto exclusiveIndicatorHeight = widget ? widget->style()->pixelMetric(QStyle::PM_ExclusiveIndicatorHeight) : 0;
      topMargin = qMax(exclusiveIndicatorHeight, option->fontMetrics.height()) + groupBoxTopMargin;
    }
    // Snippet from QFusionStyle::drawPrimitive - END

    const auto frameRectF = QRectF(option->rect).adjusted(0.5, topMargin + 0.5, -0.5, -0.5);
    painter->setPen(framePen);
    if (isEnabled)
      painter->setOpacity(0.5);
    painter->drawRect(frameRectF);
    break;
  }
  case QStyle::PE_IndicatorRadioButton: {
    const auto lineWidth = 1.666;
    const auto o = lineWidth / 2;
    indicatorPen.setWidth(lineWidth);
    painter->setPen(framePen);
    if (isEnabled)
      painter->setBrush(bgColor);
    painter->drawRoundedRect(QRectF(option->rect).adjusted(o, o, -o, -o), 100, 100, Qt::RelativeSize);

    if (isChecked) {
      painter->setPen(Qt::NoPen);
      painter->setBrush(indicatorColor);
      const auto o = 4.25;
      painter->drawRoundedRect(QRectF(option->rect).adjusted(o, o, -o, -o), 100, 100, Qt::RelativeSize);
    }
    break;
  }
  case QStyle::PE_IndicatorCheckBox: {
    const auto frameRectF = QRectF(option->rect).adjusted(0.5, 0.5, -0.5, -0.5);
    painter->setPen(framePen);
    if (isEnabled)
      painter->setBrush(bgColor);
    painter->drawRect(frameRectF);

    if (isPartiallyChecked) {
      QPen outline(indicatorColor, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
      painter->setPen(outline);
      auto fill(frameColor);
      fill.setAlphaF(0.8f);
      painter->setBrush(fill);
      const auto o = 3.5;
      painter->drawRect(QRectF(option->rect).adjusted(o, o, -o, -o));
    } else if (isChecked) {
      const double o = 3;
      const auto r = QRectF(option->rect).adjusted(o, o, -o, -o);
      QPen checkMarkPen(indicatorColor, 1.75, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin);
      painter->setPen(checkMarkPen);
      painter->drawPolyline(QPolygonF({QPointF(r.left(), r.top() + r.height() / 2), QPointF(r.left() + r.width() / 2.3, r.bottom()), r.topRight()}));
    }
    break;
  }
  case QStyle::PE_IndicatorTabClose: {
    auto window = widget ? widget->window()->windowHandle() : nullptr;
    auto iconRect = QRect(0, 0, 16, 16);
    iconRect.moveCenter(option->rect.center());
    const auto mode = !isEnabled ? QIcon::Disabled : QIcon::Normal;
    const static auto closeIcon = Utils::Icons::CLOSE_FOREGROUND.icon();
    if (option->state & QStyle::State_MouseOver && widget)
      widget->style()->drawPrimitive(QStyle::PE_PanelButtonCommand, option, painter, widget);
    const int devicePixelRatio = widget ? widget->devicePixelRatio() : 1;
    const auto iconPx = closeIcon.pixmap(window, iconRect.size() * devicePixelRatio, mode);
    painter->drawPixmap(iconRect, iconPx);
    break;
  }
  default: QTC_ASSERT_STRING("Unhandled QStyle::PrimitiveElement case");
    break;
  }
  painter->restore();
}

auto ManhattanStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const -> void
{
  const auto isPanelWidget = panelWidget(widget);
  if (!isPanelWidget) {
    const auto tweakDarkTheme = (element == PE_Frame || element == PE_FrameLineEdit || element == PE_FrameGroupBox || element == PE_IndicatorRadioButton || element == PE_IndicatorCheckBox || element == PE_IndicatorTabClose) && isDarkFusionStyle(baseStyle());
    if (tweakDarkTheme)
      drawPrimitiveTweakedForDarkTheme(element, option, painter, widget);
    else
      QProxyStyle::drawPrimitive(element, option, painter, widget);
    return;
  }

  bool animating = (option->state & State_Animating);
  int state = option->state;
  auto rect = option->rect;
  QRect oldRect;
  QRect newRect;
  if (widget && (element == PE_PanelButtonTool) && !animating) {
    auto w = const_cast<QWidget*>(widget);
    auto oldState = w->property("_q_stylestate").toInt();
    oldRect = w->property("_q_stylerect").toRect();
    newRect = w->rect();
    w->setProperty("_q_stylestate", (int)option->state);
    w->setProperty("_q_stylerect", w->rect());

    // Determine the animated transition
    auto doTransition = ((state & State_On) != (oldState & State_On) || (state & State_MouseOver) != (oldState & State_MouseOver));
    if (oldRect != newRect) {
      doTransition = false;
      d->animator.stopAnimation(widget);
    }

    if (doTransition) {
      QImage startImage(option->rect.size(), QImage::Format_ARGB32_Premultiplied);
      QImage endImage(option->rect.size(), QImage::Format_ARGB32_Premultiplied);
      auto anim = d->animator.widgetAnimation(widget);
      auto opt = *option;
      opt.state = (QStyle::State)oldState;
      opt.state |= State_Animating;
      startImage.fill(0);
      auto t = new Transition;
      t->setWidget(w);
      QPainter startPainter(&startImage);
      if (!anim) {
        drawPrimitive(element, &opt, &startPainter, widget);
      } else {
        anim->paint(&startPainter, &opt);
        d->animator.stopAnimation(widget);
      }
      auto endOpt = *option;
      endOpt.state |= State_Animating;
      t->setStartImage(startImage);
      d->animator.startAnimation(t);
      endImage.fill(0);
      QPainter endPainter(&endImage);
      drawPrimitive(element, &endOpt, &endPainter, widget);
      t->setEndImage(endImage);
      if (oldState & State_MouseOver)
        t->setDuration(150);
      else
        t->setDuration(75);
      t->setStartTime(QTime::currentTime());
    }
  }

  switch (element) {
  case PE_IndicatorDockWidgetResizeHandle:
    painter->fillRect(option->rect, orcaTheme()->color(Theme::DockWidgetResizeHandleColor));
    break;
  case PE_FrameDockWidget:
    QCommonStyle::drawPrimitive(element, option, painter, widget);
    break;
  case PE_PanelLineEdit: {
    painter->save();

    // Fill the line edit background
    QRectF backgroundRect = option->rect;
    const bool enabled = option->state & State_Enabled;
    if (Utils::orcaTheme()->flag(Theme::FlatToolBars)) {
      painter->save();
      if (!enabled)
        painter->setOpacity(0.75);
      painter->fillRect(backgroundRect, option->palette.base());
      painter->restore();
    } else {
      backgroundRect.adjust(1, 1, -1, -1);
      painter->setBrushOrigin(backgroundRect.topLeft());
      painter->fillRect(backgroundRect, option->palette.base());

      static const QImage bg(StyleHelper::dpiSpecificImageFile(QLatin1String(":/utils/images/inputfield.png")));
      static const QImage bg_disabled(StyleHelper::dpiSpecificImageFile(QLatin1String(":/utils/images/inputfield_disabled.png")));

      StyleHelper::drawCornerImage(enabled ? bg : bg_disabled, painter, option->rect, 5, 5, 5, 5);
    }

    const bool hasFocus = state & State_HasFocus;
    if (enabled && (hasFocus || state & State_MouseOver)) {
      auto hover = StyleHelper::baseColor();
      hover.setAlpha(hasFocus ? 100 : 50);
      painter->setPen(QPen(hover, 1, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
      painter->drawRect(backgroundRect.adjusted(0.5, 0.5, -0.5, -0.5));
    }
    painter->restore();
  }
  break;

  case PE_FrameStatusBarItem:
    break;

  case PE_PanelButtonTool: {
    auto anim = d->animator.widgetAnimation(widget);
    if (!animating && anim) {
      anim->paint(painter, option);
    } else {
      auto pressed = option->state & State_Sunken || option->state & State_On;
      painter->setPen(StyleHelper::sidebarShadow());
      if (pressed) {
        const auto shade = orcaTheme()->color(Theme::FancyToolButtonSelectedColor);
        painter->fillRect(rect, shade);
        if (!orcaTheme()->flag(Theme::FlatToolBars)) {
          const auto borderRect = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
          painter->drawLine(borderRect.topLeft() + QPointF(1, 0), borderRect.topRight() - QPointF(1, 0));
          painter->drawLine(borderRect.topLeft(), borderRect.bottomLeft());
          painter->drawLine(borderRect.topRight(), borderRect.bottomRight());
        }
      } else if (option->state & State_Enabled && option->state & State_MouseOver) {
        painter->fillRect(rect, orcaTheme()->color(Theme::FancyToolButtonHoverColor));
      } else if (widget && widget->property("highlightWidget").toBool()) {
        QColor shade(0, 0, 0, 128);
        painter->fillRect(rect, shade);
      }
      if (option->state & State_HasFocus && (option->state & State_KeyboardFocusChange)) {
        auto highlight = option->palette.highlight().color();
        highlight.setAlphaF(0.4f);
        painter->setPen(QPen(highlight.lighter(), 1));
        highlight.setAlphaF(0.3f);
        painter->setBrush(highlight);
        painter->setRenderHint(QPainter::Antialiasing);
        const QRectF rect = option->rect;
        painter->drawRoundedRect(rect.adjusted(2.5, 2.5, -2.5, -2.5), 2, 2);
      }
    }
  }
  break;

  case PE_PanelStatusBar: {
    const auto borderRect = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
    painter->save();
    if (orcaTheme()->flag(Theme::FlatToolBars)) {
      painter->fillRect(rect, StyleHelper::baseColor());
    } else {
      auto grad = StyleHelper::statusBarGradient(rect);
      painter->fillRect(rect, grad);
      painter->setPen(QColor(255, 255, 255, 60));
      painter->drawLine(borderRect.topLeft() + QPointF(0, 1), borderRect.topRight() + QPointF(0, 1));
      painter->setPen(StyleHelper::borderColor().darker(110)); //TODO: make themable
      painter->drawLine(borderRect.topLeft(), borderRect.topRight());
    }
    if (orcaTheme()->flag(Theme::DrawToolBarBorders)) {
      painter->setPen(StyleHelper::toolBarBorderColor());
      painter->drawLine(borderRect.topLeft(), borderRect.topRight());
    }
    painter->restore();
  }
  break;

  case PE_IndicatorToolBarSeparator: {
    auto separatorRect = rect;
    separatorRect.setLeft(rect.width() / 2);
    separatorRect.setWidth(1);
    drawButtonSeparator(painter, separatorRect, false);
  }
  break;

  case PE_IndicatorToolBarHandle: {
    bool horizontal = option->state & State_Horizontal;
    painter->save();
    QPainterPath path;
    auto x = option->rect.x() + (horizontal ? 2 : 6);
    auto y = option->rect.y() + (horizontal ? 6 : 2);
    static const auto RectHeight = 2;
    if (horizontal) {
      while (y < option->rect.height() - RectHeight - 6) {
        path.moveTo(x, y);
        path.addRect(x, y, RectHeight, RectHeight);
        y += 6;
      }
    } else {
      while (x < option->rect.width() - RectHeight - 6) {
        path.moveTo(x, y);
        path.addRect(x, y, RectHeight, RectHeight);
        x += 6;
      }
    }

    painter->setPen(Qt::NoPen);
    auto dark = StyleHelper::borderColor();
    dark.setAlphaF(0.4f);

    auto light = StyleHelper::baseColor();
    light.setAlphaF(0.4f);

    painter->fillPath(path, light);
    painter->save();
    painter->translate(1, 1);
    painter->fillPath(path, dark);
    painter->restore();
    painter->translate(3, 3);
    painter->fillPath(path, light);
    painter->translate(1, 1);
    painter->fillPath(path, dark);
    painter->restore();
  }
  break;
  case PE_IndicatorArrowUp:
  case PE_IndicatorArrowDown:
  case PE_IndicatorArrowRight:
  case PE_IndicatorArrowLeft:
    if (qobject_cast<const QMenu*>(widget)) // leave submenu arrow painting alone
      QProxyStyle::drawPrimitive(element, option, painter, widget);
    else
      StyleHelper::drawArrow(element, painter, option);
    break;

  default:
    QProxyStyle::drawPrimitive(element, option, painter, widget);
    break;
  }
}

auto ManhattanStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const -> void
{
  if (!panelWidget(widget) && !qobject_cast<const QMenu*>(widget)) {
    QProxyStyle::drawControl(element, option, painter, widget);
    return;
  }

  switch (element) {
  case CE_MenuItem:
    painter->save();
    if (const auto mbi = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
      const bool enabled = mbi->state & State_Enabled;
      auto item = *mbi;
      item.rect = mbi->rect;
      const auto color = orcaTheme()->color(enabled ? Theme::MenuItemTextColorNormal : Theme::MenuItemTextColorDisabled);
      if (color.isValid()) {
        auto pal = mbi->palette;
        pal.setBrush(QPalette::Text, color);
        item.palette = pal;
      }
      QProxyStyle::drawControl(element, &item, painter, widget);
    }
    painter->restore();
    break;

  case CE_MenuBarItem:
    painter->save();
    if (const auto mbi = qstyleoption_cast<const QStyleOptionMenuItem*>(option)) {
      const bool act = mbi->state & (State_Sunken | State_Selected);
      const auto dis = !(mbi->state & State_Enabled);

      if (orcaTheme()->flag(Theme::FlatMenuBar))
        painter->fillRect(option->rect, StyleHelper::baseColor());
      else
        StyleHelper::menuGradient(painter, option->rect, option->rect);

      auto item = *mbi;
      item.rect = mbi->rect;
      auto pal = mbi->palette;
      pal.setBrush(QPalette::ButtonText, dis ? orcaTheme()->color(Theme::MenuBarItemTextColorDisabled) : orcaTheme()->color(Theme::MenuBarItemTextColorNormal));
      item.palette = pal;
      QCommonStyle::drawControl(element, &item, painter, widget);

      if (act) {
        // Fill|
        const auto fillColor = StyleHelper::alphaBlendedColors(StyleHelper::baseColor(), orcaTheme()->color(Theme::FancyToolButtonHoverColor));
        painter->fillRect(option->rect, fillColor);

        auto pal = mbi->palette;
        uint alignment = Qt::AlignCenter | Qt::TextShowMnemonic | Qt::TextDontClip | Qt::TextSingleLine;
        if (!styleHint(SH_UnderlineShortcut, mbi, widget))
          alignment |= Qt::TextHideMnemonic;
        pal.setBrush(QPalette::Text, orcaTheme()->color(dis ? Theme::IconsDisabledColor : Theme::PanelTextColorLight));
        drawItemText(painter, item.rect, alignment, pal, !dis, mbi->text, QPalette::Text);
      }
    }
    painter->restore();
    break;

  case CE_ComboBoxLabel:
    if (const auto cb = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
      if (panelWidget(widget)) {
        painter->save();
        auto editRect = subControlRect(CC_ComboBox, cb, SC_ComboBoxEditField, widget);
        auto customPal = cb->palette;
        auto drawIcon = !(widget && widget->property("hideicon").toBool());

        if (!cb->currentIcon.isNull() && drawIcon) {
          auto mode = cb->state & State_Enabled ? QIcon::Normal : QIcon::Disabled;
          auto pixmap = cb->currentIcon.pixmap(cb->iconSize, mode);
          auto iconRect(editRect);
          iconRect.setWidth(cb->iconSize.width() + 4);
          iconRect = alignedRect(cb->direction, Qt::AlignLeft | Qt::AlignVCenter, iconRect.size(), editRect);
          if (cb->editable)
            painter->fillRect(iconRect, customPal.brush(QPalette::Base));
          drawItemPixmap(painter, iconRect, Qt::AlignCenter, pixmap);

          if (cb->direction == Qt::RightToLeft)
            editRect.translate(-4 - cb->iconSize.width(), 0);
          else
            editRect.translate(cb->iconSize.width() + 4, 0);

          // Reserve some space for the down-arrow
          editRect.adjust(0, 0, -13, 0);
        }

        QLatin1Char asterisk('*');
        auto elideWidth = editRect.width();

        auto notElideAsterisk = widget && widget->property("notelideasterisk").toBool() && cb->currentText.endsWith(asterisk) && option->fontMetrics.horizontalAdvance(cb->currentText) > elideWidth;

        QString text;
        if (notElideAsterisk) {
          elideWidth -= option->fontMetrics.horizontalAdvance(asterisk);
          text = asterisk;
        }
        text.prepend(option->fontMetrics.elidedText(cb->currentText, Qt::ElideRight, elideWidth));

        if (orcaTheme()->flag(Theme::ComboBoxDrawTextShadow) && (option->state & State_Enabled)) {
          painter->setPen(StyleHelper::toolBarDropShadowColor());
          painter->drawText(editRect.adjusted(1, 0, -1, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
        }
        painter->setPen((option->state & State_Enabled) ? option->palette.color(QPalette::WindowText) : orcaTheme()->color(Theme::IconsDisabledColor));
        painter->drawText(editRect.adjusted(1, 0, -1, 0), Qt::AlignLeft | Qt::AlignVCenter, text);

        painter->restore();
      } else {
        QProxyStyle::drawControl(element, option, painter, widget);
      }
    }
    break;

  case CE_SizeGrip: {
    painter->save();
    QColor dark = Qt::white;
    dark.setAlphaF(0.1f);
    int x, y, w, h;
    option->rect.getRect(&x, &y, &w, &h);
    auto sw = qMin(h, w);
    if (h > w)
      painter->translate(0, h - w);
    else
      painter->translate(w - h, 0);
    auto sx = x;
    auto sy = y;
    auto s = 4;
    painter->setPen(dark);
    if (option->direction == Qt::RightToLeft) {
      sx = x + sw;
      for (auto i = 0; i < 4; ++i) {
        painter->drawLine(x, sy, sx, sw);
        sx -= s;
        sy += s;
      }
    } else {
      for (auto i = 0; i < 4; ++i) {
        painter->drawLine(sx, sw, sw, sy);
        sx += s;
        sy += s;
      }
    }
    painter->restore();
  }
  break;

  case CE_MenuBarEmptyArea: {
    if (orcaTheme()->flag(Theme::FlatMenuBar))
      painter->fillRect(option->rect, StyleHelper::baseColor());
    else
      StyleHelper::menuGradient(painter, option->rect, option->rect);

    painter->save();
    painter->setPen(StyleHelper::toolBarBorderColor());
    painter->drawLine(option->rect.bottomLeft() + QPointF(0.5, 0.5), option->rect.bottomRight() + QPointF(0.5, 0.5));
    painter->restore();
  }
  break;

  case CE_ToolBar: {
    auto rect = option->rect;
    const auto borderRect = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
    bool horizontal = option->state & State_Horizontal;

    // Map offset for global window gradient
    QRect gradientSpan;
    if (widget) {
      auto offset = widget->window()->mapToGlobal(option->rect.topLeft()) - widget->mapToGlobal(option->rect.topLeft());
      gradientSpan = QRect(offset, widget->window()->size());
    }

    auto drawLightColored = lightColored(widget);
    // draws the background of the 'Type hierarchy', 'Projects' headers
    if (orcaTheme()->flag(Theme::FlatToolBars))
      painter->fillRect(rect, StyleHelper::baseColor(drawLightColored));
    else if (horizontal)
      StyleHelper::horizontalGradient(painter, gradientSpan, rect, drawLightColored);
    else
      StyleHelper::verticalGradient(painter, gradientSpan, rect, drawLightColored);

    if (orcaTheme()->flag(Theme::DrawToolBarHighlights)) {
      if (!drawLightColored)
        painter->setPen(StyleHelper::toolBarBorderColor());
      else
        painter->setPen(QColor(0x888888));

      if (horizontal) {
        // Note: This is a hack to determine if the
        // toolbar should draw the top or bottom outline
        // (needed for the find toolbar for instance)
        const auto hightLight = orcaTheme()->flag(Theme::FlatToolBars) ? orcaTheme()->color(Theme::FancyToolBarSeparatorColor) : StyleHelper::sidebarHighlight();
        const auto borderColor = drawLightColored ? QColor(255, 255, 255, 180) : hightLight;
        if (widget && widget->property("topBorder").toBool()) {
          painter->drawLine(borderRect.topLeft(), borderRect.topRight());
          painter->setPen(borderColor);
          painter->drawLine(borderRect.topLeft() + QPointF(0, 1), borderRect.topRight() + QPointF(0, 1));
        } else {
          painter->drawLine(borderRect.bottomLeft(), borderRect.bottomRight());
          painter->setPen(borderColor);
          painter->drawLine(borderRect.topLeft(), borderRect.topRight());
        }
      } else {
        painter->drawLine(borderRect.topLeft(), borderRect.bottomLeft());
        painter->drawLine(borderRect.topRight(), borderRect.bottomRight());
      }
    }
    if (orcaTheme()->flag(Theme::DrawToolBarBorders)) {
      painter->setPen(StyleHelper::toolBarBorderColor());
      if (widget && widget->property("topBorder").toBool())
        painter->drawLine(borderRect.topLeft(), borderRect.topRight());
      else
        painter->drawLine(borderRect.bottomLeft(), borderRect.bottomRight());
    }
  }
  break;
  case CE_ToolButtonLabel:
    // Directly use QCommonStyle to circumvent funny painting in QMacStyle
    // which ignores the palette and adds an alpha
    QCommonStyle::drawControl(element, option, painter, widget);
    break;
  default:
    QProxyStyle::drawControl(element, option, painter, widget);
    break;
  }
}

auto ManhattanStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const -> void
{
  if (!panelWidget(widget))
    return QProxyStyle::drawComplexControl(control, option, painter, widget);

  auto rect = option->rect;
  switch (control) {
  case CC_ToolButton:
    if (const auto toolbutton = qstyleoption_cast<const QStyleOptionToolButton*>(option)) {
      auto reverse = option->direction == Qt::RightToLeft;
      auto drawborder = (widget && widget->property("showborder").toBool());

      if (drawborder)
        drawButtonSeparator(painter, rect, reverse);

      QRect button, menuarea;
      button = subControlRect(control, toolbutton, SC_ToolButton, widget);
      menuarea = subControlRect(control, toolbutton, SC_ToolButtonMenu, widget);

      auto bflags = toolbutton->state;
      if (bflags & State_AutoRaise) {
        if (!(bflags & State_MouseOver))
          bflags &= ~State_Raised;
      }

      auto mflags = bflags;
      if (toolbutton->state & State_Sunken) {
        if (toolbutton->activeSubControls & SC_ToolButton)
          bflags |= State_Sunken;
        if (toolbutton->activeSubControls & SC_ToolButtonMenu)
          mflags |= State_Sunken;
      }

      QStyleOption tool(0);
      tool.palette = toolbutton->palette;
      if (toolbutton->subControls & SC_ToolButton) {
        tool.rect = button;
        tool.state = bflags;
        drawPrimitive(PE_PanelButtonTool, &tool, painter, widget);
      }

      auto label = *toolbutton;

      label.palette = panelPalette(option->palette, lightColored(widget));
      if (widget && widget->property("highlightWidget").toBool()) {
        label.palette.setColor(QPalette::ButtonText, orcaTheme()->color(Theme::IconsWarningToolBarColor));
      }
      auto fw = pixelMetric(PM_DefaultFrameWidth, option, widget);
      label.rect = button.adjusted(fw, fw, -fw, -fw);

      drawControl(CE_ToolButtonLabel, &label, painter, widget);

      if (toolbutton->subControls & SC_ToolButtonMenu) {
        tool.state = mflags;
        tool.rect = menuarea.adjusted(1, 1, -1, -1);
        if (mflags & (State_Sunken | State_On | State_Raised)) {
          painter->setPen(Qt::gray);
          const auto lineRect = QRectF(tool.rect).adjusted(-0.5, 2.5, 0, -2.5);
          painter->drawLine(lineRect.topLeft(), lineRect.bottomLeft());
          if (mflags & (State_Sunken)) {
            QColor shade(0, 0, 0, 50);
            painter->fillRect(tool.rect.adjusted(0, -1, 1, 1), shade);
          } else if (!HostOsInfo::isMacHost() && (mflags & State_MouseOver)) {
            QColor shade(255, 255, 255, 50);
            painter->fillRect(tool.rect.adjusted(0, -1, 1, 1), shade);
          }
        }
        tool.rect = tool.rect.adjusted(2, 2, -2, -2);
        drawPrimitive(PE_IndicatorArrowDown, &tool, painter, widget);
      } else if (toolbutton->features & QStyleOptionToolButton::HasMenu && widget && !widget->property("noArrow").toBool()) {
        auto arrowSize = 6;
        auto ir = toolbutton->rect.adjusted(1, 1, -1, -1);
        auto newBtn = *toolbutton;
        newBtn.palette = panelPalette(option->palette);
        newBtn.rect = QRect(ir.right() - arrowSize - 1, ir.height() - arrowSize - 2, arrowSize, arrowSize);
        drawPrimitive(PE_IndicatorArrowDown, &newBtn, painter, widget);
      }
    }
    break;

  case CC_ComboBox:
    if (const auto cb = qstyleoption_cast<const QStyleOptionComboBox*>(option)) {
      painter->save();
      auto isEmpty = cb->currentText.isEmpty() && cb->currentIcon.isNull();
      auto reverse = option->direction == Qt::RightToLeft;
      auto drawborder = !(widget && widget->property("hideborder").toBool());
      auto drawleftborder = (widget && widget->property("drawleftborder").toBool());
      auto alignarrow = !(widget && widget->property("alignarrow").toBool());

      if (drawborder) {
        drawButtonSeparator(painter, rect, reverse);
        if (drawleftborder)
          drawButtonSeparator(painter, rect.adjusted(0, 0, -rect.width() + 2, 0), reverse);
      }

      QStyleOption toolbutton = *option;
      if (isEmpty)
        toolbutton.state &= ~(State_Enabled | State_Sunken);
      painter->save();
      if (drawborder) {
        auto leftClipAdjust = 0;
        if (drawleftborder)
          leftClipAdjust = 2;
        painter->setClipRect(toolbutton.rect.adjusted(leftClipAdjust, 0, -2, 0));
      }
      drawPrimitive(PE_PanelButtonTool, &toolbutton, painter, widget);
      painter->restore();
      // Draw arrow
      auto menuButtonWidth = 12;
      auto left = !reverse ? rect.right() - menuButtonWidth : rect.left();
      auto right = !reverse ? rect.right() : rect.left() + menuButtonWidth;
      QRect arrowRect((left + right) / 2 + (reverse ? 6 : -6), rect.center().y() - 3, 9, 9);

      if (!alignarrow) {
        auto labelwidth = option->fontMetrics.horizontalAdvance(cb->currentText);
        if (reverse)
          arrowRect.moveLeft(qMax(rect.width() - labelwidth - menuButtonWidth - 2, 4));
        else
          arrowRect.moveLeft(qMin(labelwidth + menuButtonWidth - 2, rect.width() - menuButtonWidth - 4));
      }
      if (option->state & State_On)
        arrowRect.translate(QProxyStyle::pixelMetric(PM_ButtonShiftHorizontal, option, widget), QProxyStyle::pixelMetric(PM_ButtonShiftVertical, option, widget));

      QStyleOption arrowOpt = *option;
      arrowOpt.rect = arrowRect;
      if (isEmpty)
        arrowOpt.state &= ~(State_Enabled | State_Sunken);

      if (styleHint(SH_ComboBox_Popup, option, widget)) {
        arrowOpt.rect.translate(0, -3);
        drawPrimitive(PE_IndicatorArrowUp, &arrowOpt, painter, widget);
        arrowOpt.rect.translate(0, 6);
        drawPrimitive(PE_IndicatorArrowDown, &arrowOpt, painter, widget);
      } else {
        drawPrimitive(PE_IndicatorArrowDown, &arrowOpt, painter, widget);
      }

      painter->restore();
    }
    break;

  default:
    QProxyStyle::drawComplexControl(control, option, painter, widget);
    break;
  }
}

auto ManhattanStyle::drawButtonSeparator(QPainter *painter, const QRect &rect, bool reverse) -> void
{
  const auto borderRect = QRectF(rect).adjusted(0.5, 0.5, -0.5, -0.5);
  if (orcaTheme()->flag(Theme::FlatToolBars)) {
    const auto margin = 3;
    painter->setPen(orcaTheme()->color(Theme::FancyToolBarSeparatorColor));
    painter->drawLine(borderRect.topRight() + QPointF(0, margin), borderRect.bottomRight() - QPointF(0, margin));
  } else {
    QLinearGradient grad(rect.topRight(), rect.bottomRight());
    grad.setColorAt(0, QColor(255, 255, 255, 20));
    grad.setColorAt(0.4, QColor(255, 255, 255, 60));
    grad.setColorAt(0.7, QColor(255, 255, 255, 50));
    grad.setColorAt(1, QColor(255, 255, 255, 40));
    painter->setPen(QPen(grad, 1));
    painter->drawLine(borderRect.topRight(), borderRect.bottomRight());
    grad.setColorAt(0, QColor(0, 0, 0, 30));
    grad.setColorAt(0.4, QColor(0, 0, 0, 70));
    grad.setColorAt(0.7, QColor(0, 0, 0, 70));
    grad.setColorAt(1, QColor(0, 0, 0, 40));
    painter->setPen(QPen(grad, 1));
    if (!reverse)
      painter->drawLine(borderRect.topRight() - QPointF(1, 0), borderRect.bottomRight() - QPointF(1, 0));
    else
      painter->drawLine(borderRect.topLeft(), borderRect.bottomLeft());
  }
}

} // namespace Orca::Plugin::Core
