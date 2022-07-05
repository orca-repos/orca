// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include <QLineEdit>

namespace Utils {

class ORCA_UTILS_EXPORT CompletingLineEdit : public QLineEdit {
  Q_OBJECT

public:
  explicit CompletingLineEdit(QWidget *parent = nullptr);

protected:
  auto event(QEvent *e) -> bool override;
  auto keyPressEvent(QKeyEvent *e) -> void override;
};

} // namespace Utils
