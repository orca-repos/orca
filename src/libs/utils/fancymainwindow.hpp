// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Utils {

struct FancyMainWindowPrivate;

class ORCA_UTILS_EXPORT FancyMainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit FancyMainWindow(QWidget *parent = nullptr);
  ~FancyMainWindow() override;

  /* The widget passed in should have an objectname set
   * which will then be used as key for QSettings. */
  auto addDockForWidget(QWidget *widget, bool immutable = false) -> QDockWidget*;
  auto dockWidgets() const -> const QList<QDockWidget*>;
  auto setTrackingEnabled(bool enabled) -> void;
  auto saveSettings(QSettings *settings) const -> void;
  auto restoreSettings(const QSettings *settings) -> void;
  auto saveSettings() const -> QHash<QString, QVariant>;
  auto restoreSettings(const QHash<QString, QVariant> &settings) -> void;

  // Additional context menu actions
  auto menuSeparator1() const -> QAction*;
  auto autoHideTitleBarsAction() const -> QAction*;
  auto menuSeparator2() const -> QAction*;
  auto resetLayoutAction() const -> QAction*;
  auto showCentralWidgetAction() const -> QAction*;
  auto addDockActionsToMenu(QMenu *menu) -> void;
  auto autoHideTitleBars() const -> bool;
  auto setAutoHideTitleBars(bool on) -> void;
  auto isCentralWidgetShown() const -> bool;
  auto showCentralWidget(bool on) -> void;

signals:
  // Emitted by resetLayoutAction(). Connect to a slot
  // restoring the default layout.
  auto resetLayout() -> void;

public slots:
  auto setDockActionsVisible(bool v) -> void;

protected:
  auto hideEvent(QHideEvent *event) -> void override;
  auto showEvent(QShowEvent *event) -> void override;
  auto contextMenuEvent(QContextMenuEvent *event) -> void override;

private:
  auto onDockActionTriggered() -> void;
  auto handleVisibilityChanged(bool visible) -> void;

  FancyMainWindowPrivate *d;
};

} // namespace Utils
