// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QObject>

namespace Utils {

class ORCA_UTILS_EXPORT GlobalFileChangeBlocker : public QObject {
  Q_OBJECT

public:
  static auto instance() -> GlobalFileChangeBlocker*;
  auto forceBlocked(bool blocked) -> void;
  auto isBlocked() const -> bool { return m_blockedState; }

signals:
  auto stateChanged(bool blocked) -> void;

private:
  GlobalFileChangeBlocker();
  auto eventFilter(QObject *obj, QEvent *e) -> bool override;
  auto emitIfChanged() -> void;

  int m_forceBlocked = 0;
  bool m_blockedState = false;
};

} // namespace Utils
