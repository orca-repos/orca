// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include <QMainWindow>

namespace Utils {

class ORCA_UTILS_EXPORT AppMainWindow : public QMainWindow {
  Q_OBJECT

public:
  AppMainWindow();

public slots:
  auto raiseWindow() -> void;

signals:
  auto deviceChange() -> void;

  #ifdef Q_OS_WIN
protected:
  virtual auto winEvent(MSG *message, long *result) -> bool;
  auto event(QEvent *event) -> bool override;
  #endif

private:
  const int m_deviceEventId;
};

} // Utils
