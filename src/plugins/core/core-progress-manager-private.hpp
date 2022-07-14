// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-progress-manager.hpp"

#include <QFutureWatcher>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QList>
#include <QPointer>
#include <QPropertyAnimation>
#include <QToolButton> // required

namespace Orca::Plugin::Core {

class StatusBarWidget;
class ProgressBar;
class ProgressView;

class ProgressManagerPrivate : public Core::ProgressManager {
  Q_OBJECT

public:
  ProgressManagerPrivate();
  ~ProgressManagerPrivate() override;

  auto init() -> void;
  static auto cleanup() -> void;
  auto doAddTask(const QFuture<void> &future, const QString &title, Utils::Id type, ProgressFlags flags) -> FutureProgress*;
  static auto doSetApplicationLabel(const QString &text) -> void;
  auto progressView() -> ProgressView*;

public slots:
  auto doCancelTasks(Utils::Id type) -> void;

protected:
  auto eventFilter(QObject *obj, QEvent *event) -> bool override;

private:
  auto taskFinished() -> void;
  auto cancelAllRunningTasks() -> void;
  auto setApplicationProgressRange(int min, int max) -> void;
  auto setApplicationProgressValue(int value) -> void;
  auto setApplicationProgressVisible(bool visible) -> void;
  auto disconnectApplicationTask() -> void;
  auto updateSummaryProgressBar() -> void;
  auto fadeAwaySummaryProgress() -> void;
  auto summaryProgressFinishedFading() const -> void;
  auto progressDetailsToggled(bool checked) -> void;
  auto updateVisibility() const -> void;
  auto updateVisibilityWithDelay() -> void;
  auto updateStatusDetailsWidget() -> void;
  auto slotRemoveTask() -> void;
  auto readSettings() -> void;
  static auto initInternal() -> void;
  auto stopFadeOfSummaryProgress() -> void;
  auto hasError() const -> bool;
  auto isLastFading() const -> bool;
  auto removeOldTasks(Utils::Id type, bool keep_one = false) -> void;
  auto removeOneOldTask() -> void;
  auto removeTask(FutureProgress *task) -> void;
  auto deleteTask(FutureProgress *progress) const -> void;

  QPointer<ProgressView> m_progressView;
  QList<FutureProgress*> m_taskList;
  QMap<QFutureWatcher<void>*, Utils::Id> m_runningTasks;
  QFutureWatcher<void> *m_applicationTask = nullptr;
  StatusBarWidget *m_statusBarWidgetContainer{};
  QWidget *m_statusBarWidget{};
  QWidget *m_summaryProgressWidget{};
  QWidget *m_statusDetailsWidgetContainer = nullptr;
  QHBoxLayout *m_statusDetailsWidgetLayout = nullptr;
  QWidget *m_currentStatusDetailsWidget = nullptr;
  QPointer<FutureProgress> m_currentStatusDetailsProgress;
  QLabel *m_statusDetailsLabel = nullptr;
  ProgressBar *m_summaryProgressBar{};
  QGraphicsOpacityEffect *m_opacityEffect;
  QPointer<QPropertyAnimation> m_opacityAnimation;
  bool m_progressViewPinned = false;
  bool m_hovered = false;
};

} // namespace Orca::Plugin::Core
