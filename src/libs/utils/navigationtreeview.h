// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "itemviews.h"

namespace Utils {

class ORCA_UTILS_EXPORT NavigationTreeView : public TreeView {
  Q_OBJECT

public:
  explicit NavigationTreeView(QWidget *parent = nullptr);
  auto scrollTo(const QModelIndex &index, ScrollHint hint = EnsureVisible) -> void override;

protected:
  auto focusInEvent(QFocusEvent *event) -> void override;
  auto focusOutEvent(QFocusEvent *event) -> void override;
  auto resizeEvent(QResizeEvent *event) -> void override;
};

} // Utils
