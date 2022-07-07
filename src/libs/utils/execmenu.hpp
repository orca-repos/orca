// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

QT_BEGIN_NAMESPACE
class QAction;
class QMenu;
class QWidget;
QT_END_NAMESPACE

namespace Utils {

ORCA_UTILS_EXPORT auto execMenuAtWidget(QMenu *menu, QWidget *widget) -> QAction*;

} // namespace Utils
