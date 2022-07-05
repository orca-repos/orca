// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QWidget>

#include <functional>

namespace Utils {

class ORCA_UTILS_EXPORT OverlayWidget : public QWidget {
  Q_OBJECT

public:
  using PaintFunction = std::function<void(QWidget *, QPainter &, QPaintEvent *)>;

  explicit OverlayWidget(QWidget *parent = nullptr);

  auto attachToWidget(QWidget *parent) -> void;
  auto setPaintFunction(const PaintFunction &paint) -> void;

protected:
  auto eventFilter(QObject *obj, QEvent *ev) -> bool override;
  auto paintEvent(QPaintEvent *ev) -> void override;

private:
  auto resizeToParent() -> void;

  PaintFunction m_paint;
};

} // namespace Utils
