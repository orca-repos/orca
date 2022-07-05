// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QWidget>

namespace Utils {

class ORCA_UTILS_EXPORT FakeToolTip : public QWidget {
  Q_OBJECT

public:
  explicit FakeToolTip(QWidget *parent = nullptr);

protected:
  auto paintEvent(QPaintEvent *e) -> void override;
  auto resizeEvent(QResizeEvent *e) -> void override;
};

} // namespace Utils
