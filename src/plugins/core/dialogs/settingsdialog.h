// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/id.h>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace Core {
namespace Internal {

// Run the settings dialog and wait for it to finish.
// Returns if the changes have been applied.
auto executeSettingsDialog(QWidget *parent, Utils::Id initial_page) -> bool;

} // namespace Internal
} // namespace Core
