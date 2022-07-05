// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "theme.h"
#include "../utils_global.h"

#include <QColor>
#include <QMap>

namespace Utils {

class ORCA_UTILS_EXPORT ThemePrivate {
public:
  ThemePrivate();

  QString id;
  QString fileName;
  QString displayName;
  QStringList preferredStyles;
  QString defaultTextEditorColorScheme;
  QVector<QPair<QColor, QString>> colors;
  QVector<QString> imageFiles;
  QVector<QGradientStops> gradients;
  QVector<bool> flags;
  QMap<QString, QColor> palette;
};

ORCA_UTILS_EXPORT auto setOrcaTheme(Theme *theme) -> void;
ORCA_UTILS_EXPORT auto setThemeApplicationPalette() -> void;

} // namespace Utils
