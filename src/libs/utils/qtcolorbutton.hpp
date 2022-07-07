// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QToolButton>

namespace Utils {

class ORCA_UTILS_EXPORT QtColorButton : public QToolButton {
  Q_OBJECT
  Q_PROPERTY(bool backgroundCheckered READ isBackgroundCheckered WRITE setBackgroundCheckered)
  Q_PROPERTY(bool alphaAllowed READ isAlphaAllowed WRITE setAlphaAllowed)
  Q_PROPERTY(QColor color READ color WRITE setColor)

public:
  QtColorButton(QWidget *parent = nullptr);
  ~QtColorButton() override;

  auto isBackgroundCheckered() const -> bool;
  auto setBackgroundCheckered(bool checkered) -> void;
  auto isAlphaAllowed() const -> bool;
  auto setAlphaAllowed(bool allowed) -> void;
  auto color() const -> QColor;
  auto isDialogOpen() const -> bool;

public slots:
  auto setColor(const QColor &color) -> void;

signals:
  auto colorChangeStarted() -> void;
  auto colorChanged(const QColor &color) -> void;
  auto colorUnchanged() -> void;

protected:
  auto paintEvent(QPaintEvent *event) -> void override;
  auto mousePressEvent(QMouseEvent *event) -> void override;
  auto mouseMoveEvent(QMouseEvent *event) -> void override;
  #ifndef QT_NO_DRAGANDDROP
  auto dragEnterEvent(QDragEnterEvent *event) -> void override;
  auto dragLeaveEvent(QDragLeaveEvent *event) -> void override;
  auto dropEvent(QDropEvent *event) -> void override;
  #endif
private:
  class QtColorButtonPrivate *d_ptr;
  friend class QtColorButtonPrivate;
};

} // namespace Utils
