// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"
#include <QLabel>

namespace Utils {

class ORCA_UTILS_EXPORT FixedSizeClickLabel : public QLabel {
  Q_OBJECT
  Q_PROPERTY(QString maxText READ maxText WRITE setMaxText DESIGNABLE true)

public:
  explicit FixedSizeClickLabel(QWidget *parent = nullptr);

  auto setText(const QString &text, const QString &maxText) -> void;
  using QLabel::setText;
  auto sizeHint() const -> QSize override;
  auto maxText() const -> QString;
  auto setMaxText(const QString &maxText) -> void;

protected:
  auto mousePressEvent(QMouseEvent *ev) -> void override;
  auto mouseReleaseEvent(QMouseEvent *ev) -> void override;

signals:
  auto clicked() -> void;

private:
  QString m_maxText;
  bool m_pressed = false;
};

} // namespace Utils
