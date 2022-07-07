// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>

#include <utils/id.hpp>

#include <QFuture>
#include <QFutureInterfaceBase>
#include <QObject>

QT_FORWARD_DECLARE_CLASS(QTimer)

namespace Core {

class FutureProgress;

namespace Internal {
class ProgressManagerPrivate;
}

class CORE_EXPORT ProgressManager : public QObject {
  Q_OBJECT

public:
  enum ProgressFlag {
    KeepOnFinish = 0x01,
    ShowInApplicationIcon = 0x02
  };

  Q_DECLARE_FLAGS(ProgressFlags, ProgressFlag)

  static auto instance() -> ProgressManager*;

  template <typename T>
  static auto addTask(const QFuture<T> &future, const QString &title, const Utils::Id type, const ProgressFlags flags = {}) -> FutureProgress*
  {
    return addTask(QFuture<void>(future), title, type, flags);
  }

  static auto addTask(const QFuture<void> &future, const QString &title, Utils::Id type, ProgressFlags flags = {}) -> FutureProgress*;
  static auto addTimedTask(const QFutureInterface<void> &fi, const QString &title, Utils::Id type, int expected_seconds, ProgressFlags flags = {}) -> FutureProgress*;
  static auto setApplicationLabel(const QString &text) -> void;

public slots:
  static auto cancelTasks(Utils::Id type) -> void;

signals:
  auto taskStarted(Utils::Id type) -> void;
  auto allTasksFinished(Utils::Id type) -> void;

private:
  ProgressManager();
  ~ProgressManager() override;

  friend class Core::Internal::ProgressManagerPrivate;
};

class CORE_EXPORT ProgressTimer final : public QObject {
public:
  ProgressTimer(QFutureInterfaceBase future_interface, int expected_seconds, QObject *parent = nullptr);

private:
  auto handleTimeout() -> void;

  QFutureInterfaceBase m_future_interface;
  int m_expected_time;
  int m_current_time = 0;
  QTimer *m_timer;
};

} // namespace Core

Q_DECLARE_OPERATORS_FOR_FLAGS(Core::ProgressManager::ProgressFlags)
