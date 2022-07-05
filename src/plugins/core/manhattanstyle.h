// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"

#include <QProxyStyle>

class ManhattanStylePrivate;

class CORE_EXPORT ManhattanStyle : public QProxyStyle {
  Q_OBJECT

public:
  explicit ManhattanStyle(const QString &base_style_name);
  ~ManhattanStyle() override;

  auto drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const -> void override;
  auto drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget = nullptr) const -> void override;
  auto drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget = nullptr) const -> void override;
  auto sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size, const QWidget *widget) const -> QSize override;
  auto subElementRect(SubElement element, const QStyleOption *option, const QWidget *widget) const -> QRect override;
  auto subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl sub_control, const QWidget *widget) const -> QRect override;
  auto hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option, const QPoint &pos, const QWidget *widget = nullptr) const -> SubControl override;
  auto standardPixmap(StandardPixmap standard_pixmap, const QStyleOption *opt, const QWidget *widget = nullptr) const -> QPixmap override;
  auto standardIcon(StandardPixmap standard_icon, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const -> QIcon override;
  auto styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr, QStyleHintReturn *return_data = nullptr) const -> int override;
  auto itemRect(QPainter *p, const QRect &r, int flags, bool enabled, const QPixmap *pixmap, const QString &text, int len = -1) const -> QRect;
  auto generatedIconPixmap(QIcon::Mode icon_mode, const QPixmap &pixmap, const QStyleOption *opt) const -> QPixmap override;
  auto pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const -> int override;
  auto standardPalette() const -> QPalette override;
  auto polish(QWidget *widget) -> void override;
  auto polish(QPalette &pal) -> void override;
  auto polish(QApplication *app) -> void override;
  auto unpolish(QWidget *widget) -> void override;
  auto unpolish(QApplication *app) -> void override;

private:
  static auto drawButtonSeparator(QPainter *painter, const QRect &rect, bool reverse) -> void;

  ManhattanStylePrivate *d;
};
