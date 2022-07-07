// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <utils/id.hpp>

#include <QWidget>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Utils {
class QtcSettings;
}

namespace Core {

class RightPaneWidget;

class CORE_EXPORT RightPanePlaceHolder final : public QWidget {
  Q_OBJECT
  friend class RightPaneWidget;

public:
  explicit RightPanePlaceHolder(Utils::Id mode, QWidget *parent = nullptr);
  ~RightPanePlaceHolder() override;

  static auto current() -> RightPanePlaceHolder*;

private:
  auto currentModeChanged(Utils::Id mode) -> void;
  auto applyStoredSize(int width) -> void;

  Utils::Id m_mode;
  static RightPanePlaceHolder *m_current;
};

class CORE_EXPORT RightPaneWidget final : public QWidget {
  Q_OBJECT

public:
  RightPaneWidget();
  ~RightPaneWidget() override;

  auto saveSettings(Utils::QtcSettings *settings) const -> void;
  auto readSettings(const QSettings *settings) -> void;
  auto isShown() const -> bool;
  auto setShown(bool b) -> void;
  static auto instance() -> RightPaneWidget*;
  auto setWidget(QWidget *widget) -> void;
  auto widget() const -> QWidget*;
  auto storedWidth() const -> int;

protected:
  auto resizeEvent(QResizeEvent *) -> void override;

private:
  auto clearWidget() -> void;
  bool m_shown = true;
  int m_width = 0;
  QPointer<QWidget> m_widget;
  static RightPaneWidget *m_instance;
};

} // namespace Core
