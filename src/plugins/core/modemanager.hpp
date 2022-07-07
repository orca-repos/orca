// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>

#include <utils/id.hpp>

#include <QObject>

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

namespace Core {
class IMode;

namespace Internal {
class MainWindow;
class FancyTabWidget;
}

class CORE_EXPORT ModeManager final : public QObject {
  Q_OBJECT

public:
  enum class Style {
    IconsAndText,
    IconsOnly,
    Hidden
  };

  static auto instance() -> ModeManager*;
  static auto currentMode() -> IMode*;
  static auto currentModeId() -> Utils::Id;
  static auto addAction(QAction *action, int priority) -> void;
  static auto addProjectSelector(QAction *action) -> void;
  static auto activateMode(Utils::Id id) -> void;
  static auto setFocusToCurrentMode() -> void;
  static auto modeStyle() -> Style;
  static auto removeMode(IMode *mode) -> void;

public slots:
  static auto setModeStyle(Style style) -> void;
  static auto cycleModeStyle() -> void;

signals:
  auto currentModeAboutToChange(Utils::Id mode) -> void;
  // the default argument '=0' is important for connects without the oldMode argument.
  auto currentModeChanged(Utils::Id mode, Utils::Id old_mode = {}) -> void;

private:
  explicit ModeManager(Internal::MainWindow *main_window, Internal::FancyTabWidget *modeStack);
  ~ModeManager() override;

  static auto extensionsInitialized() -> void;
  static auto addMode(IMode *mode) -> void;
  auto currentTabAboutToChange(int index) -> void;
  auto currentTabChanged(int index) -> void;

  friend class IMode;
  friend class Internal::MainWindow;
};

} // namespace Core
