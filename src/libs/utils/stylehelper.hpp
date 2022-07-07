// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QStyle>

QT_BEGIN_NAMESPACE
class QPalette;
class QPainter;
class QRect;
// Note, this is exported but in a private header as qtopengl depends on it.
// We should consider adding this as a public helper function.
auto qt_blurImage(QPainter *p, QImage &blurImage, qreal radius, bool quality, bool alphaOnly, int transposed = 0) -> void;
QT_END_NAMESPACE

// Helper class holding all custom color values

namespace Utils {

class ORCA_UTILS_EXPORT StyleHelper {
public:
  static const unsigned int DEFAULT_BASE_COLOR = 0x666666;
  static const int progressFadeAnimationDuration = 600;

  // Height of the project explorer navigation bar
  static auto navigationWidgetHeight() -> int { return 24; }
  static auto sidebarFontSize() -> qreal;
  static auto sidebarFontPalette(const QPalette &original) -> QPalette;

  // This is our color table, all colors derive from baseColor
  static auto requestedBaseColor() -> QColor { return m_requestedBaseColor; }
  static auto baseColor(bool lightColored = false) -> QColor;
  static auto panelTextColor(bool lightColored = false) -> QColor;
  static auto highlightColor(bool lightColored = false) -> QColor;
  static auto shadowColor(bool lightColored = false) -> QColor;
  static auto borderColor(bool lightColored = false) -> QColor;
  static auto toolBarBorderColor() -> QColor;
  static auto buttonTextColor() -> QColor { return QColor(0x4c4c4c); }
  static auto mergedColors(const QColor &colorA, const QColor &colorB, int factor = 50) -> QColor;
  static auto alphaBlendedColors(const QColor &colorA, const QColor &colorB) -> QColor;
  static auto sidebarHighlight() -> QColor { return QColor(255, 255, 255, 40); }
  static auto sidebarShadow() -> QColor { return QColor(0, 0, 0, 40); }
  static auto toolBarDropShadowColor() -> QColor { return QColor(0, 0, 0, 70); }
  static auto notTooBrightHighlightColor() -> QColor;

  // Sets the base color and makes sure all top level widgets are updated
  static auto setBaseColor(const QColor &color) -> void;

  // Draws a shaded anti-aliased arrow
  static auto drawArrow(QStyle::PrimitiveElement element, QPainter *painter, const QStyleOption *option) -> void;

  // Gradients used for panels
  static auto horizontalGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect, bool lightColored = false) -> void;
  static auto verticalGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect, bool lightColored = false) -> void;
  static auto menuGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect) -> void;
  static auto usePixmapCache() -> bool { return true; }
  static auto disabledSideBarIcon(const QPixmap &enabledicon) -> QPixmap;
  static auto drawIconWithShadow(const QIcon &icon, const QRect &rect, QPainter *p, QIcon::Mode iconMode, int dipRadius = 3, const QColor &color = QColor(0, 0, 0, 130), const QPoint &dipOffset = QPoint(1, -2)) -> void;
  static auto drawCornerImage(const QImage &img, QPainter *painter, const QRect &rect, int left = 0, int top = 0, int right = 0, int bottom = 0) -> void;
  static auto tintImage(QImage &img, const QColor &tintColor) -> void;
  static auto statusBarGradient(const QRect &statusBarRect) -> QLinearGradient;

  class IconFontHelper {
  public:
    IconFontHelper(const QString &iconSymbol, const QColor &color, const QSize &size, QIcon::Mode mode = QIcon::Normal, QIcon::State state = QIcon::Off) : m_iconSymbol(iconSymbol), m_color(color), m_size(size), m_mode(mode), m_state(state) {}

    auto iconSymbol() const -> QString { return m_iconSymbol; }
    auto color() const -> QColor { return m_color; }
    auto size() const -> QSize { return m_size; }
    auto mode() const -> QIcon::Mode { return m_mode; }
    auto state() const -> QIcon::State { return m_state; }

  private:
    QString m_iconSymbol;
    QColor m_color;
    QSize m_size;
    QIcon::Mode m_mode;
    QIcon::State m_state;
  };

  static auto getIconFromIconFont(const QString &fontName, const QList<IconFontHelper> &parameters) -> QIcon;
  static auto getIconFromIconFont(const QString &fontName, const QString &iconSymbol, int fontSize, int iconSize, QColor color) -> QIcon;
  static auto getIconFromIconFont(const QString &fontName, const QString &iconSymbol, int fontSize, int iconSize) -> QIcon;
  static auto getCursorFromIconFont(const QString &fontname, const QString &cursorFill, const QString &cursorOutline, int fontSize, int iconSize) -> QIcon;
  static auto dpiSpecificImageFile(const QString &fileName) -> QString;
  static auto imageFileWithResolution(const QString &fileName, int dpr) -> QString;
  static auto availableImageResolutions(const QString &fileName) -> QList<int>;
  static auto luminance(const QColor &color) -> double;
  static auto isReadableOn(const QColor &background, const QColor &foreground) -> bool;

private:
  static QColor m_baseColor;
  static QColor m_requestedBaseColor;
};

} // namespace Utils
