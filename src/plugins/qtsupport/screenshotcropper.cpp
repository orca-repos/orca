// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "screenshotcropper.hpp"

#include <utils/fileutils.hpp>

#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>
#include <QFile>

namespace QtSupport {
namespace Internal {

class AreasOfInterest {
public:
  AreasOfInterest();
  QMap<QString, QRect> areas;
};

AreasOfInterest::AreasOfInterest()
{
  #ifdef QT_CREATOR
  areas = ScreenshotCropper::loadAreasOfInterest(":/qtsupport/images_areaofinterest.xml");
  #endif // QT_CREATOR
}

Q_GLOBAL_STATIC(AreasOfInterest, welcomeScreenAreas)

static auto fileNameForPath(const QString &path) -> QString
{
  return Utils::FilePath::fromString(path).fileName();
}

static auto cropRectForAreaOfInterest(const QSize &imageSize, const QSize &cropSize, const QRect &areaOfInterest) -> QRect
{
  QRect result;
  if (areaOfInterest.width() <= cropSize.width() && areaOfInterest.height() <= cropSize.height()) {
    const auto areaOfInterestCenter = areaOfInterest.center();
    const auto cropX = qBound(0, areaOfInterestCenter.x() - cropSize.width() / 2, imageSize.width() - cropSize.width());
    const auto cropY = qBound(0, areaOfInterestCenter.y() - cropSize.height() / 2, imageSize.height() - cropSize.height());
    const auto cropWidth = qMin(imageSize.width(), cropSize.width());
    const auto cropHeight = qMin(imageSize.height(), cropSize.height());
    result = QRect(cropX, cropY, cropWidth, cropHeight);
  } else {
    const auto resultSize = cropSize.scaled(areaOfInterest.width(), areaOfInterest.height(), Qt::KeepAspectRatioByExpanding);
    result = QRect(QPoint(), resultSize);
    result.moveCenter(areaOfInterest.center());
  }
  return result;
}

} // namespace Internal

namespace ScreenshotCropper {

auto croppedImage(const QImage &sourceImage, const QString &filePath, const QSize &cropSize, const QRect &areaOfInterest) -> QImage
{
  const auto area = areaOfInterest.isValid() ? areaOfInterest : Internal::welcomeScreenAreas()->areas.value(Internal::fileNameForPath(filePath));

  QImage result;
  if (area.isValid()) {
    const auto cropRect = Internal::cropRectForAreaOfInterest(sourceImage.size(), cropSize, areaOfInterest);
    const auto cropRectSize = cropRect.size();
    result = sourceImage.copy(cropRect);
    if (cropRectSize.width() <= cropSize.width() && cropRectSize.height() <= cropSize.height())
      return result;
  } else {
    result = sourceImage;
  }

  if (result.format() != QImage::Format_ARGB32)
    result = result.convertToFormat(QImage::Format_ARGB32);
  return result.scaled(cropSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

static auto areaAttribute(const QXmlStreamAttributes &attributes, const QString &name) -> int
{
  bool ok;
  const auto result = attributes.value(name).toString().toInt(&ok);
  if (!ok)
    qWarning() << Q_FUNC_INFO << "Could not parse" << name << "for" << attributes.value(QLatin1String("image")).toString();
  return result;
}

static const QString xmlTagAreas = QLatin1String("areas");
static const QString xmlTagArea = QLatin1String("area");
static const QString xmlAttributeImage = QLatin1String("image");
static const QString xmlAttributeX = QLatin1String("x");
static const QString xmlAttributeY = QLatin1String("y");
static const QString xmlAttributeWidth = QLatin1String("width");
static const QString xmlAttributeHeight = QLatin1String("height");

auto loadAreasOfInterest(const QString &areasXmlFile) -> QMap<QString, QRect>
{
  QMap<QString, QRect> areasOfInterest;
  QFile xmlFile(areasXmlFile);
  if (!xmlFile.open(QIODevice::ReadOnly)) {
    qWarning() << Q_FUNC_INFO << "Could not open file" << areasXmlFile;
    return areasOfInterest;
  }
  QXmlStreamReader reader(&xmlFile);
  while (!reader.atEnd()) {
    switch (reader.readNext()) {
    case QXmlStreamReader::StartElement:
      if (reader.name() == xmlTagArea) {
        const auto attributes = reader.attributes();
        const auto imageName = attributes.value(xmlAttributeImage).toString();
        if (imageName.isEmpty())
          qWarning() << Q_FUNC_INFO << "Could not parse name";

        const QRect area(areaAttribute(attributes, xmlAttributeX), areaAttribute(attributes, xmlAttributeY), areaAttribute(attributes, xmlAttributeWidth), areaAttribute(attributes, xmlAttributeHeight));
        areasOfInterest.insert(imageName, area);
      }
      break;
    default: // nothing
      break;
    }
  }

  return areasOfInterest;
}

auto saveAreasOfInterest(const QString &areasXmlFile, QMap<QString, QRect> &areas) -> bool
{
  QFile file(areasXmlFile);
  if (!file.open(QIODevice::WriteOnly))
    return false;
  QXmlStreamWriter writer(&file);
  writer.setAutoFormatting(true);
  writer.writeStartDocument();
  writer.writeStartElement(xmlTagAreas);
  for (auto i = areas.cbegin(), end = areas.cend(); i != end; ++i) {
    writer.writeStartElement(xmlTagArea);
    writer.writeAttribute(xmlAttributeImage, i.key());
    writer.writeAttribute(xmlAttributeX, QString::number(i.value().x()));
    writer.writeAttribute(xmlAttributeY, QString::number(i.value().y()));
    writer.writeAttribute(xmlAttributeWidth, QString::number(i.value().width()));
    writer.writeAttribute(xmlAttributeHeight, QString::number(i.value().height()));
    writer.writeEndElement(); // xmlTagArea
  }
  writer.writeEndElement(); // xmlTagAreas
  writer.writeEndDocument();
  return true;
}

} // ScreenshotCropper
} // namespace QtSupport
