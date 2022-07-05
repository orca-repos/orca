// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include <QLabel>

namespace Utils {

class ORCA_UTILS_EXPORT ElidingLabel : public QLabel {
  Q_OBJECT
  Q_PROPERTY(Qt::TextElideMode elideMode READ elideMode WRITE setElideMode DESIGNABLE true)

public:
  explicit ElidingLabel(QWidget *parent = nullptr);
  explicit ElidingLabel(const QString &text, QWidget *parent = nullptr);

  auto elideMode() const -> Qt::TextElideMode;
  auto setElideMode(const Qt::TextElideMode &elideMode) -> void;

protected:
  auto paintEvent(QPaintEvent *event) -> void override;

private:
  Qt::TextElideMode m_elideMode;
};

} // namespace Utils
