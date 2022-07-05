// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "algorithm.h"
#include "icon.h"
#include "qtcassert.h"
#include "theme/theme.h"
#include "stylehelper.h"

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPaintEngine>
#include <QWidget>
#include <QDebug>

namespace Utils {

static const qreal PunchEdgeWidth = 0.5;
static const qreal PunchEdgeIntensity = 0.6;

static auto maskToColorAndAlpha(const QPixmap &mask, const QColor &color) -> QPixmap
{
  QImage result(mask.toImage().convertToFormat(QImage::Format_ARGB32));
  result.setDevicePixelRatio(mask.devicePixelRatio());
  auto bitsStart = reinterpret_cast<QRgb*>(result.bits());
  const QRgb *bitsEnd = bitsStart + result.width() * result.height();
  const QRgb tint = color.rgb() & 0x00ffffff;
  const auto alpha = QRgb(color.alpha());
  for (QRgb *pixel = bitsStart; pixel < bitsEnd; ++pixel) {
    QRgb pixelAlpha = (((~*pixel) & 0xff) * alpha) >> 8;
    *pixel = (pixelAlpha << 24) | tint;
  }
  return QPixmap::fromImage(result);
}

using MaskAndColor = QPair<QPixmap, QColor>;
using MasksAndColors = QList<MaskAndColor>;

static auto masksAndColors(const Icon &icon, int dpr) -> MasksAndColors
{
  MasksAndColors result;
  for (const IconMaskAndColor &i : icon) {
    const QString &fileName = i.first.toString();
    const QColor color = orcaTheme()->color(i.second);
    const QString dprFileName = StyleHelper::availableImageResolutions(i.first.toString()).contains(dpr) ? StyleHelper::imageFileWithResolution(fileName, dpr) : fileName;
    QPixmap pixmap;
    if (!pixmap.load(dprFileName)) {
      pixmap = QPixmap(1, 1);
      qWarning() << "Could not load image: " << dprFileName;
    }
    result.append({pixmap, color});
  }
  return result;
}

static auto smearPixmap(QPainter *painter, const QPixmap &pixmap, qreal radius) -> void
{
  const qreal nagative = -radius - 0.01; // Workaround for QPainter rounding behavior
  const qreal positive = radius;
  painter->drawPixmap(QPointF(nagative, nagative), pixmap);
  painter->drawPixmap(QPointF(0, nagative), pixmap);
  painter->drawPixmap(QPointF(positive, nagative), pixmap);
  painter->drawPixmap(QPointF(positive, 0), pixmap);
  painter->drawPixmap(QPointF(positive, positive), pixmap);
  painter->drawPixmap(QPointF(0, positive), pixmap);
  painter->drawPixmap(QPointF(nagative, positive), pixmap);
  painter->drawPixmap(QPointF(nagative, 0), pixmap);
}

static auto combinedMask(const MasksAndColors &masks, Icon::IconStyleOptions style) -> QPixmap
{
  if (masks.count() == 1)
    return masks.first().first;

  QPixmap result(masks.first().first);
  QPainter p(&result);
  p.setCompositionMode(QPainter::CompositionMode_Darken);
  auto maskImage = masks.constBegin();
  maskImage++;
  for (; maskImage != masks.constEnd(); ++maskImage) {
    if (style & Icon::PunchEdges) {
      p.save();
      p.setOpacity(PunchEdgeIntensity);
      p.setCompositionMode(QPainter::CompositionMode_Lighten);
      smearPixmap(&p, maskToColorAndAlpha((*maskImage).first, Qt::white), PunchEdgeWidth);
      p.restore();
    }
    p.drawPixmap(0, 0, (*maskImage).first);
  }
  p.end();
  return result;
}

static auto masksToIcon(const MasksAndColors &masks, const QPixmap &combinedMask, Icon::IconStyleOptions style) -> QPixmap
{
  QPixmap result(combinedMask.size());
  result.setDevicePixelRatio(combinedMask.devicePixelRatio());
  result.fill(Qt::transparent);
  QPainter p(&result);

  for (MasksAndColors::const_iterator maskImage = masks.constBegin(); maskImage != masks.constEnd(); ++maskImage) {
    if (style & Icon::PunchEdges && maskImage != masks.constBegin()) {
      // Punch a transparent outline around an overlay.
      p.save();
      p.setOpacity(PunchEdgeIntensity);
      p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
      smearPixmap(&p, maskToColorAndAlpha((*maskImage).first, Qt::white), PunchEdgeWidth);
      p.restore();
    }
    p.drawPixmap(0, 0, maskToColorAndAlpha((*maskImage).first, (*maskImage).second));
  }

  if (style & Icon::DropShadow && orcaTheme()->flag(Theme::ToolBarIconShadow)) {
    const QPixmap shadowMask = maskToColorAndAlpha(combinedMask, Qt::black);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOver);
    p.setOpacity(0.08);
    p.drawPixmap(QPointF(0, -0.501), shadowMask);
    p.drawPixmap(QPointF(-0.501, 0), shadowMask);
    p.drawPixmap(QPointF(0.5, 0), shadowMask);
    p.drawPixmap(QPointF(0.5, 0.5), shadowMask);
    p.drawPixmap(QPointF(-0.501, 0.5), shadowMask);
    p.setOpacity(0.3);
    p.drawPixmap(0, 1, shadowMask);
  }

  p.end();

  return result;
}

Icon::Icon() = default;

Icon::Icon(std::initializer_list<IconMaskAndColor> args, Icon::IconStyleOptions style) : QVector<IconMaskAndColor>(args), m_style(style) {}

Icon::Icon(const FilePath &imageFileName)
{
  append({imageFileName, Theme::Color(-1)});
}

auto Icon::icon() const -> QIcon
{
  if (isEmpty()) {
    return QIcon();
  } else if (m_style == None) {
    return QIcon(constFirst().first.toString());
  } else {
    QIcon result;
    const int maxDpr = qRound(qApp->devicePixelRatio());
    for (int dpr = 1; dpr <= maxDpr; dpr++) {
      const MasksAndColors masks = masksAndColors(*this, dpr);
      const QPixmap combinedMask = Utils::combinedMask(masks, m_style);
      result.addPixmap(masksToIcon(masks, combinedMask, m_style));

      const QColor disabledColor = orcaTheme()->color(Theme::IconsDisabledColor);
      result.addPixmap(maskToColorAndAlpha(combinedMask, disabledColor), QIcon::Disabled);
    }
    return result;
  }
}

auto Icon::pixmap(QIcon::Mode iconMode) const -> QPixmap
{
  if (isEmpty()) {
    return QPixmap();
  } else if (m_style == None) {
    return QPixmap(StyleHelper::dpiSpecificImageFile(constFirst().first.toString()));
  } else {
    const MasksAndColors masks = masksAndColors(*this, qRound(qApp->devicePixelRatio()));
    const QPixmap combinedMask = Utils::combinedMask(masks, m_style);
    return iconMode == QIcon::Disabled ? maskToColorAndAlpha(combinedMask, orcaTheme()->color(Theme::IconsDisabledColor)) : masksToIcon(masks, combinedMask, m_style);
  }
}

auto Icon::imageFilePath() const -> FilePath
{
  QTC_ASSERT(length() == 1, return {});
  return first().first;
}

auto Icon::sideBarIcon(const Icon &classic, const Icon &flat) -> QIcon
{
  QIcon result;
  if (orcaTheme()->flag(Theme::FlatSideBarIcons)) {
    result = flat.icon();
  } else {
    const QPixmap pixmap = classic.pixmap();
    result.addPixmap(pixmap);
    // Ensure that the icon contains a disabled state of that size, since
    // Since we have icons with mixed sizes (e.g. DEBUG_START), and want to
    // avoid that QIcon creates scaled versions of missing QIcon::Disabled
    // sizes.
    result.addPixmap(StyleHelper::disabledSideBarIcon(pixmap), QIcon::Disabled);
  }
  return result;
}

auto Icon::modeIcon(const Icon &classic, const Icon &flat, const Icon &flatActive) -> QIcon
{
  QIcon result = sideBarIcon(classic, flat);
  if (orcaTheme()->flag(Theme::FlatSideBarIcons))
    result.addPixmap(flatActive.pixmap(), QIcon::Active);
  return result;
}

auto Icon::combinedIcon(const QList<QIcon> &icons) -> QIcon
{
  QIcon result;
  QWindow *window = QApplication::allWidgets().constFirst()->windowHandle();
  for (const QIcon &icon : icons)
    for (const QIcon::Mode mode : {QIcon::Disabled, QIcon::Normal})
      for (const QSize &size : icon.availableSizes(mode))
        result.addPixmap(icon.pixmap(window, size, mode), mode);
  return result;
}

auto Icon::combinedIcon(const QList<Icon> &icons) -> QIcon
{
  const QList<QIcon> qIcons = transform(icons, &Icon::icon);
  return combinedIcon(qIcons);
}

} // namespace Utils
