// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include "processreaper.hpp"
#include "processutils.hpp"
#include "singleton.hpp"

#include <QThread>

namespace Utils {
namespace Internal {
class CallerHandle;
class LauncherHandle;
class LauncherInterfacePrivate;
class ProcessLauncherImpl;
}

class ORCA_UTILS_EXPORT LauncherInterface final : public SingletonWithOptionalDependencies<LauncherInterface, ProcessReaper> {
public:
  static auto setPathToLauncher(const QString &pathToLauncher) -> void;

private:
  friend class Utils::Internal::CallerHandle;
  friend class Utils::Internal::LauncherHandle;
  friend class Utils::Internal::ProcessLauncherImpl;

  static auto isStarted() -> bool;
  static auto isReady() -> bool;
  static auto sendData(const QByteArray &data) -> void;
  static auto registerHandle(QObject *parent, quintptr token, ProcessMode mode) -> Utils::Internal::CallerHandle*;
  static auto unregisterHandle(quintptr token) -> void;

  LauncherInterface();
  ~LauncherInterface();

  QThread m_thread;
  Internal::LauncherInterfacePrivate *m_private;
  friend class SingletonWithOptionalDependencies<LauncherInterface, ProcessReaper>;
};

} // namespace Utils
