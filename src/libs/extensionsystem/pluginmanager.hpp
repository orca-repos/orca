// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.hpp"

#include <aggregation/aggregate.hpp>
#include <utils/qtcsettings.hpp>

#include <QObject>
#include <QStringList>

QT_BEGIN_NAMESPACE
class QTextStream;
QT_END_NAMESPACE

namespace ExtensionSystem {

class IPlugin;
class PluginSpec;

namespace Internal {
class PluginManagerPrivate;
}

class EXTENSIONSYSTEM_EXPORT PluginManager : public QObject {
  Q_OBJECT

public:
  static auto instance() -> PluginManager*;

  PluginManager();
  ~PluginManager() override;

  // Object pool operations
  static auto addObject(QObject *obj) -> void;
  static auto removeObject(QObject *obj) -> void;
  static auto allObjects() -> QVector<QObject*>;
  static auto listLock() -> QReadWriteLock*;

  // This is useful for soft dependencies using pure interfaces.
  template <typename T>
  static auto getObject() -> T*
  {
    QReadLocker lock(listLock());
    const QVector<QObject*> all = allObjects();
    for (QObject *obj : all) {
      if (T *result = qobject_cast<T*>(obj))
        return result;
    }
    return nullptr;
  }

  template <typename T, typename Predicate>
  static auto getObject(Predicate predicate) -> T*
  {
    QReadLocker lock(listLock());
    const QVector<QObject*> all = allObjects();
    for (QObject *obj : all) {
      if (T *result = qobject_cast<T*>(obj))
        if (predicate(result))
          return result;
    }
    return 0;
  }

  static auto getObjectByName(const QString &name) -> QObject*;

  // Plugin operations
  static auto loadQueue() -> QVector<PluginSpec*>;
  static auto loadPlugins() -> void;
  static auto pluginPaths() -> QStringList;
  static auto setPluginPaths(const QStringList &paths) -> void;
  static auto pluginIID() -> QString;
  static auto setPluginIID(const QString &iid) -> void;
  static auto plugins() -> const QVector<PluginSpec*>;
  static auto pluginCollections() -> QHash<QString, QVector<PluginSpec*>>;
  static auto hasError() -> bool;
  static auto allErrors() -> const QStringList;
  static auto pluginsRequiringPlugin(PluginSpec *spec) -> const QSet<PluginSpec*>;
  static auto pluginsRequiredByPlugin(PluginSpec *spec) -> const QSet<PluginSpec*>;
  static auto checkForProblematicPlugins() -> void;

  // Settings
  static auto setSettings(Utils::QtcSettings *settings) -> void;
  static auto settings() -> Utils::QtcSettings*;
  static auto setGlobalSettings(Utils::QtcSettings *settings) -> void;
  static auto globalSettings() -> Utils::QtcSettings*;
  static auto writeSettings() -> void;

  // command line arguments
  static auto arguments() -> QStringList;
  static auto argumentsForRestart() -> QStringList;
  static auto parseOptions(const QStringList &args, const QMap<QString, bool> &appOptions, QMap<QString, QString> *foundAppOptions, QString *errorString) -> bool;
  static auto formatOptions(QTextStream &str, int optionIndentation, int descriptionIndentation) -> void;
  static auto formatPluginOptions(QTextStream &str, int optionIndentation, int descriptionIndentation) -> void;
  static auto formatPluginVersions(QTextStream &str) -> void;
  static auto serializedArguments() -> QString;
  static auto testRunRequested() -> bool;

  #ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
  static auto registerScenario(const QString &scenarioId, std::function<bool()> scenarioStarter) -> bool;
  static auto isScenarioRequested() -> bool;
  static auto runScenario() -> bool;
  static auto isScenarioRunning(const QString &scenarioId) -> bool;
  // static void triggerScenarioPoint(const QVariant pointData); // ?? called from scenario point
  static auto finishScenario() -> bool;
  static auto waitForScenarioFullyInitialized() -> void;
  // signals:
  // void scenarioPointTriggered(const QVariant pointData); // ?? e.g. in StringTable::GC() -> post a call to quit into main thread and sleep for 5 seconds in the GC thread
  #endif

  struct ProcessData {
    QString m_executable;
    QStringList m_args;
    QString m_workingPath;
    QString m_settingsPath;
  };

  static auto setCreatorProcessData(const ProcessData &data) -> void;
  static auto creatorProcessData() -> ProcessData;
  static auto profilingReport(const char *what, const PluginSpec *spec = nullptr) -> void;
  static auto platformName() -> QString;
  static auto isInitializationDone() -> bool;
  static auto remoteArguments(const QString &serializedArguments, QObject *socket) -> void;
  static auto shutdown() -> void;
  static auto systemInformation() -> QString;

signals:
  auto objectAdded(QObject *obj) -> void;
  auto aboutToRemoveObject(QObject *obj) -> void;
  auto pluginsChanged() -> void;
  auto initializationDone() -> void;
  auto testsFinished(int failedTests) -> void;
  auto scenarioFinished(int exitCode) -> void;

  friend class Internal::PluginManagerPrivate;
};

} // namespace ExtensionSystem
