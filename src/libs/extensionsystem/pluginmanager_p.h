// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "pluginspec.h"
#include "pluginmanager.h"

#include <utils/algorithm.h>

#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <QReadWriteLock>
#include <QScopedPointer>
#include <QSet>
#include <QStringList>
#include <QWaitCondition>

#include <queue>

QT_BEGIN_NAMESPACE
class QTime;
class QTimer;
class QEventLoop;
QT_END_NAMESPACE

namespace Utils {
class QtcSettings;
}

namespace ExtensionSystem {

class PluginManager;

namespace Internal {

class PluginSpecPrivate;

class EXTENSIONSYSTEM_EXPORT PluginManagerPrivate : public QObject {
  Q_OBJECT public:
  PluginManagerPrivate(PluginManager *pluginManager);
  ~PluginManagerPrivate() override;

  // Object pool operations
  auto addObject(QObject *obj) -> void;
  auto removeObject(QObject *obj) -> void;

  // Plugin operations
  auto checkForProblematicPlugins() -> void;
  auto loadPlugins() -> void;
  auto shutdown() -> void;
  auto setPluginPaths(const QStringList &paths) -> void;
  auto loadQueue() -> const QVector<ExtensionSystem::PluginSpec*>;
  auto loadPlugin(PluginSpec *spec, PluginSpec::State destState) -> void;
  auto resolveDependencies() -> void;
  auto enableDependenciesIndirectly() -> void;
  auto initProfiling() -> void;
  auto profilingSummary() const -> void;
  auto profilingReport(const char *what, const PluginSpec *spec = nullptr) -> void;
  auto setSettings(Utils::QtcSettings *settings) -> void;
  auto setGlobalSettings(Utils::QtcSettings *settings) -> void;
  auto readSettings() -> void;
  auto writeSettings() -> void;

  class TestSpec {
  public:
    TestSpec(PluginSpec *pluginSpec, const QStringList &testFunctionsOrObjects = QStringList()) : pluginSpec(pluginSpec), testFunctionsOrObjects(testFunctionsOrObjects) {}
    PluginSpec *pluginSpec = nullptr;
    QStringList testFunctionsOrObjects;
  };

  auto containsTestSpec(PluginSpec *pluginSpec) const -> bool
  {
    return Utils::contains(testSpecs, [pluginSpec](const TestSpec &s) { return s.pluginSpec == pluginSpec; });
  }

  auto removeTestSpec(PluginSpec *pluginSpec) -> void
  {
    testSpecs = Utils::filtered(testSpecs, [pluginSpec](const TestSpec &s) { return s.pluginSpec != pluginSpec; });
  }

  QHash<QString, QVector<PluginSpec*>> pluginCategories;
  QVector<PluginSpec*> pluginSpecs;
  std::vector<TestSpec> testSpecs;
  QStringList pluginPaths;
  QString pluginIID;
  QVector<QObject*> allObjects;       // ### make this a QVector<QPointer<QObject> > > ?
  QStringList defaultDisabledPlugins; // Plugins/Ignored from install settings
  QStringList defaultEnabledPlugins;  // Plugins/ForceEnabled from install settings
  QStringList disabledPlugins;
  QStringList forceEnabledPlugins;
  // delayed initialization
  QTimer *delayedInitializeTimer = nullptr;
  std::queue<PluginSpec*> delayedInitializeQueue;
  // ansynchronous shutdown
  QSet<PluginSpec*> asynchronousPlugins;   // plugins that have requested async shutdown
  QEventLoop *shutdownEventLoop = nullptr; // used for async shutdown
  QStringList arguments;
  QStringList argumentsForRestart;
  QScopedPointer<QElapsedTimer> m_profileTimer;
  QHash<const PluginSpec*, int> m_profileTotal;
  int m_profileElapsedMS = 0;
  unsigned m_profilingVerbosity = 0;
  Utils::QtcSettings *settings = nullptr;
  Utils::QtcSettings *globalSettings = nullptr;

  // Look in argument descriptions of the specs for the option.
  auto pluginForOption(const QString &option, bool *requiresArgument) const -> PluginSpec*;
  auto pluginByName(const QString &name) const -> PluginSpec*;

  // used by tests
  static auto createSpec() -> PluginSpec*;
  static auto privateSpec(PluginSpec *spec) -> PluginSpecPrivate*;

  mutable QReadWriteLock m_lock;
  bool m_isInitializationDone = false;
  bool enableCrashCheck = true;
  QHash<QString, std::function<bool()>> m_scenarios;
  QString m_requestedScenario;
  std::atomic_bool m_isScenarioRunning = false;  // if it's running, the running one is m_requestedScenario
  std::atomic_bool m_isScenarioFinished = false; // if it's running, the running one is m_requestedScenario
  bool m_scenarioFullyInitialized = false;
  QMutex m_scenarioMutex;
  QWaitCondition m_scenarioWaitCondition;
  PluginManager::ProcessData m_creatorProcessData;

private:
  PluginManager *q;

  auto nextDelayedInitialize() -> void;
  auto asyncShutdownFinished() -> void;
  auto readPluginPaths() -> void;
  auto loadQueue(PluginSpec *spec, QVector<ExtensionSystem::PluginSpec*> &queue, QVector<ExtensionSystem::PluginSpec*> &circularityCheckQueue) -> bool;
  auto stopAll() -> void;
  auto deleteAll() -> void;

  #ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
  auto startTests() -> void;
  #endif
};

} // namespace Internal
} // namespace ExtensionSystem
