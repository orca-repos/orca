// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QString>
#include <QWidget>

namespace Utils {
namespace FadingIndicator {

enum TextSize {
  SmallText,
  LargeText
};

ORCA_UTILS_EXPORT auto showText(QWidget *parent, const QString &text, TextSize size = LargeText) -> void;
ORCA_UTILS_EXPORT auto showPixmap(QWidget *parent, const QString &pixmap) -> void;

} // FadingIndicator
} // Utils
