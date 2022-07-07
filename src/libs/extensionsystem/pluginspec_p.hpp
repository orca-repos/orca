// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "pluginspec.hpp"
#include "iplugin.hpp"

#include <QJsonObject>
#include <QObject>
#include <QPluginLoader>
#include <QRegularExpression>
#include <QStringList>
#include <QVector>
#include <QXmlStreamReader>

namespace ExtensionSystem {

class IPlugin;
class PluginManager;

namespace Internal {

class EXTENSIONSYSTEM_EXPORT PluginSpecPrivate : public QObject {
  Q_OBJECT

public:
  PluginSpecPrivate(PluginSpec *spec);

  auto read(const QString &fileName) -> bool;
  auto provides(const QString &pluginName, const QString &version) const -> bool;
  auto resolveDependencies(const QVector<PluginSpec*> &specs) -> bool;
  auto loadLibrary() -> bool;
  auto initializePlugin() -> bool;
  auto initializeExtensions() -> bool;
  auto delayedInitialize() -> bool;
  auto stop() -> IPlugin::ShutdownFlag;
  auto kill() -> void;
  auto setEnabledBySettings(bool value) -> void;
  auto setEnabledByDefault(bool value) -> void;
  auto setForceEnabled(bool value) -> void;
  auto setForceDisabled(bool value) -> void;

  QPluginLoader loader;
  QString name;
  QString version;
  QString compatVersion;
  bool required = false;
  bool hiddenByDefault = false;
  bool experimental = false;
  bool enabledByDefault = true;
  QString vendor;
  QString copyright;
  QString license;
  QString description;
  QString url;
  QString category;
  QRegularExpression platformSpecification;
  QVector<PluginDependency> dependencies;
  QJsonObject metaData;
  bool enabledBySettings = true;
  bool enabledIndirectly = false;
  bool forceEnabled = false;
  bool forceDisabled = false;
  QString location;
  QString filePath;
  QStringList arguments;
  QHash<PluginDependency, PluginSpec*> dependencySpecs;
  PluginSpec::PluginArgumentDescriptions argumentDescriptions;
  IPlugin *plugin = nullptr;
  PluginSpec::State state = PluginSpec::Invalid;
  bool hasError = false;
  QString errorString;

  static auto isValidVersion(const QString &version) -> bool;
  static auto versionCompare(const QString &version1, const QString &version2) -> int;
  auto enableDependenciesIndirectly(bool enableTestDependencies = false) -> QVector<PluginSpec*>;
  auto readMetaData(const QJsonObject &pluginMetaData) -> bool;

private:
  PluginSpec *q;

  auto reportError(const QString &err) -> bool;
  static auto versionRegExp() -> const QRegularExpression&;
};

} // namespace Internal
} // namespace ExtensionSystem
