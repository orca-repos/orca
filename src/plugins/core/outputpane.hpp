// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <utils/id.hpp>

#include <QWidget>

QT_BEGIN_NAMESPACE
class QSplitter;
QT_END_NAMESPACE

namespace Core {

class OutputPanePlaceHolderPrivate;

class CORE_EXPORT OutputPanePlaceHolder final : public QWidget {
  Q_OBJECT

public:
  explicit OutputPanePlaceHolder(Utils::Id mode, QSplitter *parent = nullptr);
  ~OutputPanePlaceHolder() override;

  static auto getCurrent() -> OutputPanePlaceHolder*;
  static auto isCurrentVisible() -> bool;
  auto isMaximized() const -> bool;
  auto setMaximized(bool maximize) -> void;
  auto ensureSizeHintAsMinimum() -> void;
  auto nonMaximizedSize() const -> int;

signals:
  auto visibilityChangeRequested(bool visible) -> void;

protected:
  auto resizeEvent(QResizeEvent *event) -> void override;
  auto showEvent(QShowEvent *) -> void override;

private:
  auto setHeight(int height) -> void;
  auto currentModeChanged(Utils::Id mode) -> void;

  OutputPanePlaceHolderPrivate *d;
};

} // namespace Core
