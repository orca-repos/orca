// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QAbstractItemView>

namespace Core {
namespace Internal {
namespace ItemDataRoles {

enum Roles {
  ResultItemRole = Qt::UserRole,
  ResultLineRole,
  ResultBeginLineNumberRole,
  ResultIconRole,
  ResultHighlightBackgroundColor,
  ResultHighlightForegroundColor,
  ResultBeginColumnNumberRole,
  SearchTermLengthRole,
  IsGeneratedRole
};

} // namespace Internal
} // namespace Core
} // namespace ItemDataRoles
