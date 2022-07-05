// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "stylehelper.h"

#include "theme/theme.h"
#include "hostosinfo.h"

#include <utils/qtcassert.h>

#include <QPixmapCache>
#include <QPainter>
#include <QApplication>
#include <QFileInfo>
#include <QCommonStyle>
#include <QStyleOption>
#include <QWindow>
#include <QFontDatabase>
#include <qmath.h>

// Clamps float color values within (0, 255)
static auto clamp(float x) -> int
{
    const int val = x > 255 ? 255 : static_cast<int>(x);
    return val < 0 ? 0 : val;
}

// Clamps float color values within (0, 255)
/*
static int range(float x, int min, int max)
{
    int val = x > max ? max : x;
    return val < min ? min : val;
}
*/

namespace Utils {

auto StyleHelper::mergedColors(const QColor &colorA, const QColor &colorB, int factor) -> QColor
{
  const int maxFactor = 100;
  QColor tmp = colorA;
  tmp.setRed((tmp.red() * factor) / maxFactor + (colorB.red() * (maxFactor - factor)) / maxFactor);
  tmp.setGreen((tmp.green() * factor) / maxFactor + (colorB.green() * (maxFactor - factor)) / maxFactor);
  tmp.setBlue((tmp.blue() * factor) / maxFactor + (colorB.blue() * (maxFactor - factor)) / maxFactor);
  return tmp;
}

auto StyleHelper::alphaBlendedColors(const QColor &colorA, const QColor &colorB) -> QColor
{
  const int alpha = colorB.alpha();
  const int antiAlpha = 255 - alpha;

  return QColor((colorA.red() * antiAlpha + colorB.red() * alpha) / 255, (colorA.green() * antiAlpha + colorB.green() * alpha) / 255, (colorA.blue() * antiAlpha + colorB.blue() * alpha) / 255);
}

auto StyleHelper::sidebarFontSize() -> qreal
{
  return HostOsInfo::isMacHost() ? 10 : 7.5;
}

auto StyleHelper::notTooBrightHighlightColor() -> QColor
{
  QColor highlightColor = QApplication::palette().highlight().color();
  if (0.5 * highlightColor.saturationF() + 0.75 - highlightColor.valueF() < 0)
    highlightColor.setHsvF(highlightColor.hsvHueF(), 0.1 + highlightColor.saturationF() * 2.0, highlightColor.valueF());
  return highlightColor;
}

auto StyleHelper::sidebarFontPalette(const QPalette &original) -> QPalette
{
  QPalette palette = original;
  const QColor textColor = orcaTheme()->color(Theme::ProgressBarTitleColor);
  palette.setColor(QPalette::WindowText, textColor);
  palette.setColor(QPalette::Text, textColor);
  return palette;
}

auto StyleHelper::panelTextColor(bool lightColored) -> QColor
{
  if (!lightColored)
    return Qt::white;
  else
    return Qt::black;
}

// Invalid by default, setBaseColor needs to be called at least once
QColor StyleHelper::m_baseColor;
QColor StyleHelper::m_requestedBaseColor;

auto StyleHelper::baseColor(bool lightColored) -> QColor
{
  static const QColor windowColor = QApplication::palette().color(QPalette::Window);
  static const bool windowColorAsBase = orcaTheme()->flag(Theme::WindowColorAsBase);

  return (lightColored || windowColorAsBase) ? windowColor : m_baseColor;
}

auto StyleHelper::highlightColor(bool lightColored) -> QColor
{
  QColor result = baseColor(lightColored);
  if (!lightColored)
    result.setHsv(result.hue(), clamp(result.saturation()), clamp(result.value() * 1.16));
  else
    result.setHsv(result.hue(), clamp(result.saturation()), clamp(result.value() * 1.06));
  return result;
}

auto StyleHelper::shadowColor(bool lightColored) -> QColor
{
  QColor result = baseColor(lightColored);
  result.setHsv(result.hue(), clamp(result.saturation() * 1.1), clamp(result.value() * 0.70));
  return result;
}

auto StyleHelper::borderColor(bool lightColored) -> QColor
{
  QColor result = baseColor(lightColored);
  result.setHsv(result.hue(), result.saturation(), result.value() / 2);
  return result;
}

auto StyleHelper::toolBarBorderColor() -> QColor
{
  const QColor base = baseColor();
  return QColor::fromHsv(base.hue(), base.saturation(), clamp(base.value() * 0.80f));
}

// We try to ensure that the actual color used are within
// reasonalbe bounds while generating the actual baseColor
// from the users request.
auto StyleHelper::setBaseColor(const QColor &newcolor) -> void
{
  m_requestedBaseColor = newcolor;

  const QColor themeBaseColor = orcaTheme()->color(Theme::PanelStatusBarBackgroundColor);
  const QColor defaultBaseColor = QColor(DEFAULT_BASE_COLOR);
  QColor color;

  if (defaultBaseColor == newcolor) {
    color = themeBaseColor;
  } else {
    const int valueDelta = (newcolor.value() - defaultBaseColor.value()) / 3;
    const int value = qBound(0, themeBaseColor.value() + valueDelta, 255);

    color.setHsv(newcolor.hue(), newcolor.saturation() * 0.7, value);
  }

  if (color.isValid() && color != m_baseColor) {
    m_baseColor = color;
    const QList<QWidget*> widgets = QApplication::topLevelWidgets();
    for (QWidget *w : widgets)
      w->update();
  }
}

static auto verticalGradientHelper(QPainter *p, const QRect &spanRect, const QRect &rect, bool lightColored) -> void
{
  QColor highlight = StyleHelper::highlightColor(lightColored);
  QColor shadow = StyleHelper::shadowColor(lightColored);
  QLinearGradient grad(spanRect.topRight(), spanRect.topLeft());
  grad.setColorAt(0, highlight.lighter(117));
  grad.setColorAt(1, shadow.darker(109));
  p->fillRect(rect, grad);

  QColor light(255, 255, 255, 80);
  p->setPen(light);
  p->drawLine(rect.topRight() - QPoint(1, 0), rect.bottomRight() - QPoint(1, 0));
  QColor dark(0, 0, 0, 90);
  p->setPen(dark);
  p->drawLine(rect.topLeft(), rect.bottomLeft());
}

auto StyleHelper::verticalGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect, bool lightColored) -> void
{
  if (StyleHelper::usePixmapCache()) {

    QColor keyColor = baseColor(lightColored);
    const QString key = QString::asprintf("mh_vertical %d %d %d %d %d", spanRect.width(), spanRect.height(), clipRect.width(), clipRect.height(), keyColor.rgb());

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
      pixmap = QPixmap(clipRect.size());
      QPainter p(&pixmap);
      QRect rect(0, 0, clipRect.width(), clipRect.height());
      verticalGradientHelper(&p, spanRect, rect, lightColored);
      p.end();
      QPixmapCache::insert(key, pixmap);
    }

    painter->drawPixmap(clipRect.topLeft(), pixmap);
  } else {
    verticalGradientHelper(painter, spanRect, clipRect, lightColored);
  }
}

static auto horizontalGradientHelper(QPainter *p, const QRect &spanRect, const QRect &rect, bool lightColored) -> void
{
  if (lightColored) {
    QLinearGradient shadowGradient(rect.topLeft(), rect.bottomLeft());
    shadowGradient.setColorAt(0, 0xf0f0f0);
    shadowGradient.setColorAt(1, 0xcfcfcf);
    p->fillRect(rect, shadowGradient);
    return;
  }

  QColor base = StyleHelper::baseColor(lightColored);
  QColor highlight = StyleHelper::highlightColor(lightColored);
  QColor shadow = StyleHelper::shadowColor(lightColored);
  QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
  grad.setColorAt(0, highlight.lighter(120));
  if (rect.height() == StyleHelper::navigationWidgetHeight()) {
    grad.setColorAt(0.4, highlight);
    grad.setColorAt(0.401, base);
  }
  grad.setColorAt(1, shadow);
  p->fillRect(rect, grad);

  QLinearGradient shadowGradient(spanRect.topLeft(), spanRect.topRight());
  shadowGradient.setColorAt(0, QColor(0, 0, 0, 30));
  QColor lighterHighlight;
  lighterHighlight = highlight.lighter(130);
  lighterHighlight.setAlpha(100);
  shadowGradient.setColorAt(0.7, lighterHighlight);
  shadowGradient.setColorAt(1, QColor(0, 0, 0, 40));
  p->fillRect(rect, shadowGradient);
}

auto StyleHelper::horizontalGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect, bool lightColored) -> void
{
  if (StyleHelper::usePixmapCache()) {

    QColor keyColor = baseColor(lightColored);
    const QString key = QString::asprintf("mh_horizontal %d %d %d %d %d %d", spanRect.width(), spanRect.height(), clipRect.width(), clipRect.height(), keyColor.rgb(), spanRect.x());

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
      pixmap = QPixmap(clipRect.size());
      QPainter p(&pixmap);
      QRect rect = QRect(0, 0, clipRect.width(), clipRect.height());
      horizontalGradientHelper(&p, spanRect, rect, lightColored);
      p.end();
      QPixmapCache::insert(key, pixmap);
    }

    painter->drawPixmap(clipRect.topLeft(), pixmap);

  } else {
    horizontalGradientHelper(painter, spanRect, clipRect, lightColored);
  }
}

static auto menuGradientHelper(QPainter *p, const QRect &spanRect, const QRect &rect) -> void
{
  QLinearGradient grad(spanRect.topLeft(), spanRect.bottomLeft());
  QColor menuColor = StyleHelper::mergedColors(StyleHelper::baseColor(), QColor(244, 244, 244), 25);
  grad.setColorAt(0, menuColor.lighter(112));
  grad.setColorAt(1, menuColor);
  p->fillRect(rect, grad);
}

auto StyleHelper::drawArrow(QStyle::PrimitiveElement element, QPainter *painter, const QStyleOption *option) -> void
{
  if (option->rect.width() <= 1 || option->rect.height() <= 1)
    return;

  const qreal devicePixelRatio = painter->device()->devicePixelRatio();
  const bool enabled = option->state & QStyle::State_Enabled;
  QRect r = option->rect;
  int size = qMin(r.height(), r.width());
  QPixmap pixmap;
  const QString pixmapName = QString::asprintf("StyleHelper::drawArrow-%d-%d-%d-%f", element, size, enabled, devicePixelRatio);
  if (!QPixmapCache::find(pixmapName, &pixmap)) {
    QImage image(size * devicePixelRatio, size * devicePixelRatio, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);

    QStyleOption tweakedOption(*option);
    tweakedOption.state = QStyle::State_Enabled;

    auto drawCommonStyleArrow = [&tweakedOption, element, &painter](const QRect &rect, const QColor &color) -> void {
      static const QCommonStyle *const style = qobject_cast<QCommonStyle*>(QApplication::style());
      if (!style)
        return;
      tweakedOption.palette.setColor(QPalette::ButtonText, color.rgb());
      tweakedOption.rect = rect;
      painter.setOpacity(color.alphaF());
      style->QCommonStyle::drawPrimitive(element, &tweakedOption, &painter);
    };

    if (!enabled) {
      drawCommonStyleArrow(image.rect(), orcaTheme()->color(Theme::IconsDisabledColor));
    } else {
      if (orcaTheme()->flag(Theme::ToolBarIconShadow))
        drawCommonStyleArrow(image.rect().translated(0, devicePixelRatio), toolBarDropShadowColor());
      drawCommonStyleArrow(image.rect(), orcaTheme()->color(Theme::IconsBaseColor));
    }
    painter.end();
    pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    QPixmapCache::insert(pixmapName, pixmap);
  }
  int xOffset = r.x() + (r.width() - size) / 2;
  int yOffset = r.y() + (r.height() - size) / 2;
  painter->drawPixmap(xOffset, yOffset, pixmap);
}

auto StyleHelper::menuGradient(QPainter *painter, const QRect &spanRect, const QRect &clipRect) -> void
{
  if (StyleHelper::usePixmapCache()) {
    const QString key = QString::asprintf("mh_menu %d %d %d %d %d", spanRect.width(), spanRect.height(), clipRect.width(), clipRect.height(), StyleHelper::baseColor().rgb());

    QPixmap pixmap;
    if (!QPixmapCache::find(key, &pixmap)) {
      pixmap = QPixmap(clipRect.size());
      QPainter p(&pixmap);
      QRect rect = QRect(0, 0, clipRect.width(), clipRect.height());
      menuGradientHelper(&p, spanRect, rect);
      p.end();
      QPixmapCache::insert(key, pixmap);
    }

    painter->drawPixmap(clipRect.topLeft(), pixmap);
  } else {
    menuGradientHelper(painter, spanRect, clipRect);
  }
}

auto StyleHelper::disabledSideBarIcon(const QPixmap &enabledicon) -> QPixmap
{
  QImage im = enabledicon.toImage().convertToFormat(QImage::Format_ARGB32);
  for (int y = 0; y < im.height(); ++y) {
    auto scanLine = reinterpret_cast<QRgb*>(im.scanLine(y));
    for (int x = 0; x < im.width(); ++x) {
      QRgb pixel = *scanLine;
      char intensity = char(qGray(pixel));
      *scanLine = qRgba(intensity, intensity, intensity, qAlpha(pixel));
      ++scanLine;
    }
  }
  return QPixmap::fromImage(im);
}

// Draws a cached pixmap with shadow
auto StyleHelper::drawIconWithShadow(const QIcon &icon, const QRect &rect, QPainter *p, QIcon::Mode iconMode, int dipRadius, const QColor &color, const QPoint &dipOffset) -> void
{
  QPixmap cache;
  const int devicePixelRatio = p->device()->devicePixelRatio();
  QString pixmapName = QString::fromLatin1("icon %0 %1 %2 %3").arg(icon.cacheKey()).arg(iconMode).arg(rect.height()).arg(devicePixelRatio);

  if (!QPixmapCache::find(pixmapName, &cache)) {
    // High-dpi support: The in parameters (rect, radius, offset) are in
    // device-independent pixels. The call to QIcon::pixmap() below might
    // return a high-dpi pixmap, which will in that case have a devicePixelRatio
    // different than 1. The shadow drawing caluculations are done in device
    // pixels.
    QWindow *window = dynamic_cast<QWidget*>(p->device())->window()->windowHandle();
    QPixmap px = icon.pixmap(window, rect.size(), iconMode);
    int radius = dipRadius * devicePixelRatio;
    QPoint offset = dipOffset * devicePixelRatio;
    cache = QPixmap(px.size() + QSize(radius * 2, radius * 2));
    cache.fill(Qt::transparent);

    QPainter cachePainter(&cache);
    if (iconMode == QIcon::Disabled) {
      const bool hasDisabledState = icon.availableSizes().count() == icon.availableSizes(QIcon::Disabled).count();
      if (!hasDisabledState)
        px = disabledSideBarIcon(icon.pixmap(window, rect.size()));
    } else if (orcaTheme()->flag(Theme::ToolBarIconShadow)) {
      // Draw shadow
      QImage tmp(px.size() + QSize(radius * 2, radius * 2 + 1), QImage::Format_ARGB32_Premultiplied);
      tmp.fill(Qt::transparent);

      QPainter tmpPainter(&tmp);
      tmpPainter.setCompositionMode(QPainter::CompositionMode_Source);
      tmpPainter.drawPixmap(QRect(radius, radius, px.width(), px.height()), px);
      tmpPainter.end();

      // blur the alpha channel
      QImage blurred(tmp.size(), QImage::Format_ARGB32_Premultiplied);
      blurred.fill(Qt::transparent);
      QPainter blurPainter(&blurred);
      qt_blurImage(&blurPainter, tmp, radius, false, true);
      blurPainter.end();

      tmp = blurred;

      // blacken the image...
      tmpPainter.begin(&tmp);
      tmpPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
      tmpPainter.fillRect(tmp.rect(), color);
      tmpPainter.end();

      tmpPainter.begin(&tmp);
      tmpPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
      tmpPainter.fillRect(tmp.rect(), color);
      tmpPainter.end();

      // draw the blurred drop shadow...
      cachePainter.drawImage(QRect(0, 0, cache.rect().width(), cache.rect().height()), tmp);
    }

    // Draw the actual pixmap...
    cachePainter.drawPixmap(QRect(QPoint(radius, radius) + offset, QSize(px.width(), px.height())), px);
    cachePainter.end();
    cache.setDevicePixelRatio(devicePixelRatio);
    QPixmapCache::insert(pixmapName, cache);
  }

  QRect targetRect = cache.rect();
  targetRect.setSize(targetRect.size() / cache.devicePixelRatio());
  targetRect.moveCenter(rect.center() - dipOffset);
  p->drawPixmap(targetRect, cache);
}

// Draws a CSS-like border image where the defined borders are not stretched
// Unit for rect, left, top, right and bottom is user pixels
auto StyleHelper::drawCornerImage(const QImage &img, QPainter *painter, const QRect &rect, int left, int top, int right, int bottom) -> void
{
  // source rect for drawImage() calls needs to be specified in DIP unit of the image
  const qreal imagePixelRatio = img.devicePixelRatio();
  const qreal leftDIP = left * imagePixelRatio;
  const qreal topDIP = top * imagePixelRatio;
  const qreal rightDIP = right * imagePixelRatio;
  const qreal bottomDIP = bottom * imagePixelRatio;

  const QSize size = img.size();
  if (top > 0) {
    //top
    painter->drawImage(QRectF(rect.left() + left, rect.top(), rect.width() - right - left, top), img, QRectF(leftDIP, 0, size.width() - rightDIP - leftDIP, topDIP));
    if (left > 0) //top-left
      painter->drawImage(QRectF(rect.left(), rect.top(), left, top), img, QRectF(0, 0, leftDIP, topDIP));
    if (right > 0) //top-right
      painter->drawImage(QRectF(rect.left() + rect.width() - right, rect.top(), right, top), img, QRectF(size.width() - rightDIP, 0, rightDIP, topDIP));
  }
  //left
  if (left > 0)
    painter->drawImage(QRectF(rect.left(), rect.top() + top, left, rect.height() - top - bottom), img, QRectF(0, topDIP, leftDIP, size.height() - bottomDIP - topDIP));
  //center
  painter->drawImage(QRectF(rect.left() + left, rect.top() + top, rect.width() - right - left, rect.height() - bottom - top), img, QRectF(leftDIP, topDIP, size.width() - rightDIP - leftDIP, size.height() - bottomDIP - topDIP));
  if (right > 0) //right
    painter->drawImage(QRectF(rect.left() + rect.width() - right, rect.top() + top, right, rect.height() - top - bottom), img, QRectF(size.width() - rightDIP, topDIP, rightDIP, size.height() - bottomDIP - topDIP));
  if (bottom > 0) {
    //bottom
    painter->drawImage(QRectF(rect.left() + left, rect.top() + rect.height() - bottom, rect.width() - right - left, bottom), img, QRectF(leftDIP, size.height() - bottomDIP, size.width() - rightDIP - leftDIP, bottomDIP));
    if (left > 0) //bottom-left
      painter->drawImage(QRectF(rect.left(), rect.top() + rect.height() - bottom, left, bottom), img, QRectF(0, size.height() - bottomDIP, leftDIP, bottomDIP));
    if (right > 0) //bottom-right
      painter->drawImage(QRectF(rect.left() + rect.width() - right, rect.top() + rect.height() - bottom, right, bottom), img, QRectF(size.width() - rightDIP, size.height() - bottomDIP, rightDIP, bottomDIP));
  }
}

// Tints an image with tintColor, while preserving alpha and lightness
auto StyleHelper::tintImage(QImage &img, const QColor &tintColor) -> void
{
  QPainter p(&img);
  p.setCompositionMode(QPainter::CompositionMode_Screen);

  for (int x = 0; x < img.width(); ++x) {
    for (int y = 0; y < img.height(); ++y) {
      QRgb rgbColor = img.pixel(x, y);
      int alpha = qAlpha(rgbColor);
      QColor c = QColor(rgbColor);

      if (alpha > 0) {
        c.toHsl();
        qreal l = c.lightnessF();
        QColor newColor = QColor::fromHslF(tintColor.hslHueF(), tintColor.hslSaturationF(), l);
        newColor.setAlpha(alpha);
        img.setPixel(x, y, newColor.rgba());
      }
    }
  }
}

auto StyleHelper::statusBarGradient(const QRect &statusBarRect) -> QLinearGradient
{
  QLinearGradient grad(statusBarRect.topLeft(), QPoint(statusBarRect.center().x(), statusBarRect.bottom()));
  QColor startColor = shadowColor().darker(164);
  QColor endColor = baseColor().darker(130);
  grad.setColorAt(0, startColor);
  grad.setColorAt(1, endColor);
  return grad;
}

auto StyleHelper::getIconFromIconFont(const QString &fontName, const QList<IconFontHelper> &parameters) -> QIcon
{
  QFontDatabase a;

  QTC_ASSERT(a.hasFamily(fontName), {});

  if (!a.hasFamily(fontName))
    return {};

  QIcon icon;

  for (const IconFontHelper &p : parameters) {
    const int maxDpr = qRound(qApp->devicePixelRatio());
    for (int dpr = 1; dpr <= maxDpr; dpr++) {
      QPixmap pixmap(p.size() * dpr);
      pixmap.setDevicePixelRatio(dpr);
      pixmap.fill(Qt::transparent);

      QFont font(fontName);
      font.setPixelSize(p.size().height());

      QPainter painter(&pixmap);
      painter.save();
      painter.setPen(p.color());
      painter.setFont(font);
      painter.drawText(QRectF(QPoint(0, 0), p.size()), p.iconSymbol());
      painter.restore();

      icon.addPixmap(pixmap, p.mode(), p.state());
    }
  }

  return icon;
}

auto StyleHelper::getIconFromIconFont(const QString &fontName, const QString &iconSymbol, int fontSize, int iconSize, QColor color) -> QIcon
{
  QFontDatabase a;

  QTC_ASSERT(a.hasFamily(fontName), {});

  if (a.hasFamily(fontName)) {

    QIcon icon;
    QSize size(iconSize, iconSize);

    const int maxDpr = qRound(qApp->devicePixelRatio());
    for (int dpr = 1; dpr <= maxDpr; dpr++) {
      QPixmap pixmap(size * dpr);
      pixmap.setDevicePixelRatio(dpr);
      pixmap.fill(Qt::transparent);

      QFont font(fontName);
      font.setPixelSize(fontSize);

      QPainter painter(&pixmap);
      painter.save();
      painter.setPen(color);
      painter.setFont(font);
      painter.drawText(QRectF(QPoint(0, 0), size), iconSymbol);
      painter.restore();

      icon.addPixmap(pixmap);
    }

    return icon;
  }

  return {};
}

auto StyleHelper::getIconFromIconFont(const QString &fontName, const QString &iconSymbol, int fontSize, int iconSize) -> QIcon
{
  QColor penColor = QApplication::palette("QWidget").color(QPalette::Normal, QPalette::ButtonText);
  return getIconFromIconFont(fontName, iconSymbol, fontSize, iconSize, penColor);
}

auto StyleHelper::getCursorFromIconFont(const QString &fontName, const QString &cursorFill, const QString &cursorOutline, int fontSize, int iconSize) -> QIcon
{
  QFontDatabase a;

  QTC_ASSERT(a.hasFamily(fontName), {});

  const QColor outlineColor = Qt::black;
  const QColor fillColor = Qt::white;

  if (a.hasFamily(fontName)) {

    QIcon icon;
    QSize size(iconSize, iconSize);

    const int maxDpr = qRound(qApp->devicePixelRatio());
    for (int dpr = 1; dpr <= maxDpr; dpr++) {
      QPixmap pixmap(size * dpr);
      pixmap.setDevicePixelRatio(dpr);
      pixmap.fill(Qt::transparent);

      QFont font(fontName);
      font.setPixelSize(fontSize);

      QPainter painter(&pixmap);
      painter.save();
      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setRenderHint(QPainter::TextAntialiasing, true);
      painter.setRenderHint(QPainter::LosslessImageRendering, true);
      painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

      painter.setFont(font);
      painter.setPen(outlineColor);
      painter.drawText(QRectF(QPointF(0.0, 0.0), size), Qt::AlignCenter, cursorOutline);

      painter.setPen(fillColor);
      painter.drawText(QRectF(QPointF(0.0, 0.0), size), Qt::AlignCenter, cursorFill);

      painter.restore();

      icon.addPixmap(pixmap);
    }

    return icon;
  }

  return {};
}

auto StyleHelper::dpiSpecificImageFile(const QString &fileName) -> QString
{
  // See QIcon::addFile()
  if (qApp->devicePixelRatio() > 1.0) {
    const QString atDprfileName = imageFileWithResolution(fileName, qRound(qApp->devicePixelRatio()));
    if (QFile::exists(atDprfileName))
      return atDprfileName;
  }
  return fileName;
}

auto StyleHelper::imageFileWithResolution(const QString &fileName, int dpr) -> QString
{
  const QFileInfo fi(fileName);
  return dpr == 1 ? fileName : fi.path() + QLatin1Char('/') + fi.completeBaseName() + QLatin1Char('@') + QString::number(dpr) + QLatin1String("x.") + fi.suffix();
}

auto StyleHelper::availableImageResolutions(const QString &fileName) -> QList<int>
{
  QList<int> result;
  const int maxResolutions = qApp->devicePixelRatio();
  for (int i = 1; i <= maxResolutions; ++i)
    if (QFile::exists(imageFileWithResolution(fileName, i)))
      result.append(i);
  return result;
}

auto StyleHelper::luminance(const QColor &color) -> double
{
  // calculate the luminance based on
  // https://www.w3.org/TR/2008/REC-WCAG20-20081211/#relativeluminancedef
  auto val = [](const double &colorVal) {
    return colorVal < 0.03928 ? colorVal / 12.92 : std::pow((colorVal + 0.055) / 1.055, 2.4);
  };

  static QHash<QRgb, double> cache;
  QHash<QRgb, double>::iterator it = cache.find(color.rgb());
  if (it == cache.end()) {
    it = cache.insert(color.rgb(), 0.2126 * val(color.redF()) + 0.7152 * val(color.greenF()) + 0.0722 * val(color.blueF()));
  }
  return it.value();
}

static auto contrastRatio(const QColor &color1, const QColor &color2) -> double
{
  // calculate the contrast ratio based on
  // https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef
  auto contrast = (StyleHelper::luminance(color1) + .05) / (StyleHelper::luminance(color2) + .05);
  if (contrast < 1)
    return 1 / contrast;
  return contrast;
}

auto StyleHelper::isReadableOn(const QColor &background, const QColor &foreground) -> bool
{
  // following the W3C Recommendation on contrast for large Text
  // https://www.w3.org/TR/2008/REC-WCAG20-20081211/#contrast-ratiodef
  return contrastRatio(background, foreground) > 3;
}

} // namespace Utils
