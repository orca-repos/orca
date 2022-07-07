// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QSortFilterProxyModel>

namespace Utils {

class ORCA_UTILS_EXPORT CategorySortFilterModel : public QSortFilterProxyModel {
public:
  CategorySortFilterModel(QObject *parent = nullptr);

protected:
  auto filterAcceptsRow(int source_row, const QModelIndex &source_parent) const -> bool override;
};

} // Utils
