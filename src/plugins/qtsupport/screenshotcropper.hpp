// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <QMap>
#include <QRect>

QT_BEGIN_NAMESPACE
class QImage;
class QSize;
QT_END_NAMESPACE

namespace QtSupport {
namespace ScreenshotCropper {

QTSUPPORT_EXPORT auto croppedImage(const QImage &sourceImage, const QString &filePath, const QSize &cropSize, const QRect &areaOfInterest = {}) -> QImage;
QTSUPPORT_EXPORT auto loadAreasOfInterest(const QString &areasXmlFile) -> QMap<QString, QRect>;
QTSUPPORT_EXPORT auto saveAreasOfInterest(const QString &areasXmlFile, QMap<QString, QRect> &areas) -> bool;

} // ScreenshotCropper
} // namespace QtSupport
