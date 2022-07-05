// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.h"

#include <QObject>

namespace ExtensionSystem {
namespace Internal {
  class IPluginPrivate;
  class PluginSpecPrivate;
}

class PluginManager;
class PluginSpec;

class EXTENSIONSYSTEM_EXPORT IPlugin : public QObject {
  Q_OBJECT

public:
  enum ShutdownFlag {
    SynchronousShutdown,
    AsynchronousShutdown
  };

  IPlugin();
  ~IPlugin() override;

  virtual auto initialize(const QStringList &arguments, QString *errorString) -> bool = 0;
  virtual auto extensionsInitialized() -> void {}
  virtual auto delayedInitialize() -> bool { return false; }
  virtual auto aboutToShutdown() -> ShutdownFlag { return SynchronousShutdown; }
  virtual auto remoteCommand(const QStringList & /* options */, const QString & /* workingDirectory */, const QStringList & /* arguments */) -> QObject* { return nullptr; }
  virtual auto createTestObjects() const -> QVector<QObject*>;

  auto pluginSpec() const -> PluginSpec*;

signals:
  auto asynchronousShutdownFinished() -> void;

private:
  Internal::IPluginPrivate *d;
  friend class Internal::PluginSpecPrivate;
};

} // namespace ExtensionSystem
