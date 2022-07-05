// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

QT_FORWARD_DECLARE_CLASS(QTimer)

#include <QLabel>

namespace Utils {

class ORCA_UTILS_EXPORT StatusLabel : public QLabel {
  Q_OBJECT

public:
  explicit StatusLabel(QWidget *parent = nullptr);

public slots:
  auto showStatusMessage(const QString &message, int timeoutMS = 5000) -> void;
  auto clearStatusMessage() -> void;

private:
  auto slotTimeout() -> void;
  auto stopTimer() -> void;

  QTimer *m_timer = nullptr;
  QString m_lastPermanentStatusMessage;
};

} // namespace Utils
