// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include <QObject>

QT_BEGIN_NAMESPACE
class QHeaderView;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT HeaderViewStretcher : public QObject {
  const int m_columnToStretch;

public:
  explicit HeaderViewStretcher(QHeaderView *headerView, int columnToStretch);

  auto stretch() -> void;
  auto softStretch() -> void;
  auto eventFilter(QObject *obj, QEvent *ev) -> bool override;
};

} // namespace Utils
