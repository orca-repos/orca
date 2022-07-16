// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "classviewsymbollocation.hpp"
#include "classviewsymbolinformation.hpp"

#include <QVariant>
#include <QList>
#include <QSet>

QT_FORWARD_DECLARE_CLASS(QStandardItem)

namespace ClassView {
namespace Internal {

auto roleToLocations(const QList<QVariant> &locations) -> QSet<SymbolLocation>;
auto symbolInformationFromItem(const QStandardItem *item) -> SymbolInformation;

} // namespace Internal
} // namespace ClassView
