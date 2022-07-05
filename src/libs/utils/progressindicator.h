// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "overlaywidget.h"
#include "utils_global.h"

#include <QTimer>
#include <QWidget>

#include <functional>
#include <memory>

namespace Utils {

namespace Internal {
class ProgressIndicatorPrivate;
}

enum class ProgressIndicatorSize {
  Small,
  Medium,
  Large
};

class ORCA_UTILS_EXPORT ProgressIndicatorPainter {
public:
  using UpdateCallback = std::function<void()>;

  ProgressIndicatorPainter(ProgressIndicatorSize size);
  virtual ~ProgressIndicatorPainter() = default;

  auto setIndicatorSize(ProgressIndicatorSize size) -> void;
  auto indicatorSize() const -> ProgressIndicatorSize;
  auto setUpdateCallback(const UpdateCallback &cb) -> void;
  auto size() const -> QSize;
  auto paint(QPainter &painter, const QRect &rect) const -> void;
  auto startAnimation() -> void;
  auto stopAnimation() -> void;

protected:
  auto nextAnimationStep() -> void;

private:
  ProgressIndicatorSize m_size = ProgressIndicatorSize::Small;
  int m_rotationStep = 45;
  int m_rotation = 0;
  QTimer m_timer;
  QPixmap m_pixmap;
  UpdateCallback m_callback;
};

class ORCA_UTILS_EXPORT ProgressIndicator : public OverlayWidget {
  Q_OBJECT

public:
  explicit ProgressIndicator(ProgressIndicatorSize size, QWidget *parent = nullptr);

  auto setIndicatorSize(ProgressIndicatorSize size) -> void;
  auto sizeHint() const -> QSize final;

protected:
  auto showEvent(QShowEvent *) -> void final;
  auto hideEvent(QHideEvent *) -> void final;

private:
  ProgressIndicatorPainter m_paint;
};

} // Utils
