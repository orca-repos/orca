// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

QT_BEGIN_NAMESPACE
class QString;
class QWidget;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

CORE_EXPORT auto warning(const QString &title, const QString &desciption) -> QWidget*;
CORE_EXPORT auto information(const QString &title, const QString &desciption) -> QWidget*;
CORE_EXPORT auto critical(const QString &title, const QString &desciption) -> QWidget*;

} // namespace Orca::Plugin::Core