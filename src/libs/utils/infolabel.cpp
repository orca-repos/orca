// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "infolabel.h"

#include <utils/icon.h>
#include <utils/utilsicons.h>

#include <QPainter>
#include <QPaintEvent>
#include <QLabel>

namespace Utils {

constexpr int iconSize = 16;

InfoLabel::InfoLabel(QWidget *parent) : InfoLabel({}, Information, parent) {}

InfoLabel::InfoLabel(const QString &text, InfoType type, QWidget *parent) : ElidingLabel(text, parent)
{
  setType(type);
}

auto InfoLabel::type() const -> InfoLabel::InfoType
{
  return m_type;
}

auto InfoLabel::setType(InfoType type) -> void
{
  m_type = type;
  setContentsMargins(m_type == None ? 0 : iconSize + 2, 0, 0, 0);
  update();
}

auto InfoLabel::filled() const -> bool
{
  return m_filled;
}

auto InfoLabel::setFilled(bool filled) -> void
{
  m_filled = filled;
}

auto InfoLabel::minimumSizeHint() const -> QSize
{
  QSize baseHint = ElidingLabel::minimumSizeHint();
  baseHint.setHeight(qMax(baseHint.height(), iconSize));
  return baseHint;
}

static auto fillColorForType(InfoLabel::InfoType type) -> Utils::Theme::Color
{
  using namespace Utils;
  switch (type) {
  case InfoLabel::Warning:
    return Theme::IconsWarningColor;
  case InfoLabel::Ok:
    return Theme::IconsRunColor;
  case InfoLabel::Error:
  case InfoLabel::NotOk:
    return Theme::IconsErrorColor;
  case InfoLabel::Information: default:
    return Theme::IconsInfoColor;
  }
}

static auto iconForType(InfoLabel::InfoType type) -> const QIcon&
{
  using namespace Utils;
  switch (type) {
  case InfoLabel::Information: {
    static const QIcon icon = Icons::INFO.icon();
    return icon;
  }
  case InfoLabel::Warning: {
    static const QIcon icon = Icons::WARNING.icon();
    return icon;
  }
  case InfoLabel::Error: {
    static const QIcon icon = Icons::CRITICAL.icon();
    return icon;
  }
  case InfoLabel::Ok: {
    static const QIcon icon = Icons::OK.icon();
    return icon;
  }
  case InfoLabel::NotOk: {
    static const QIcon icon = Icons::BROKEN.icon();
    return icon;
  }
  default: {
    static const QIcon undefined;
    return undefined;
  }
  }
}

auto InfoLabel::paintEvent(QPaintEvent *event) -> void
{
  if (m_type == None)
    return ElidingLabel::paintEvent(event);

  const bool centerIconVertically = wordWrap() || elideMode() == Qt::ElideNone;
  const QRect iconRect(0, centerIconVertically ? 0 : ((height() - iconSize) / 2), iconSize, iconSize);

  QPainter p(this);
  if (m_filled && isEnabled()) {
    p.save();
    p.setOpacity(0.175);
    p.fillRect(rect(), orcaTheme()->color(fillColorForType(m_type)));
    p.restore();
  }
  const QIcon &icon = iconForType(m_type);
  QWindow *window = this->window()->windowHandle();
  const QIcon::Mode mode = !this->isEnabled() ? QIcon::Disabled : QIcon::Normal;
  const QPixmap iconPx = icon.pixmap(window, QSize(iconSize, iconSize) * devicePixelRatio(), mode);
  p.drawPixmap(iconRect, iconPx);
  ElidingLabel::paintEvent(event);
}

} // namespace Utils
