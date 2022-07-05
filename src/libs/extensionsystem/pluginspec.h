// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.h"

#include <utils/porting.h>

#include <QString>
#include <QHash>
#include <QVector>

QT_BEGIN_NAMESPACE
class QRegularExpression;
QT_END_NAMESPACE

namespace ExtensionSystem {

namespace Internal {

class OptionsParser;
class PluginSpecPrivate;
class PluginManagerPrivate;

} // Internal

class IPlugin;
class PluginView;

struct EXTENSIONSYSTEM_EXPORT PluginDependency {
  enum Type {
    Required,
    Optional,
    Test
  };

  PluginDependency() : type(Required) {}

  friend auto qHash(const PluginDependency &value) -> Utils::QHashValueType;

  QString name;
  QString version;
  Type type;
  auto operator==(const PluginDependency &other) const -> bool;
  auto toString() const -> QString;
};

struct EXTENSIONSYSTEM_EXPORT PluginArgumentDescription {
  QString name;
  QString parameter;
  QString description;
};

class EXTENSIONSYSTEM_EXPORT PluginSpec {
public:
  enum State {
    Invalid,
    Read,
    Resolved,
    Loaded,
    Initialized,
    Running,
    Stopped,
    Deleted
  };

  ~PluginSpec();

  // information from the xml file, valid after 'Read' state is reached
  auto name() const -> QString;
  auto version() const -> QString;
  auto compatVersion() const -> QString;
  auto vendor() const -> QString;
  auto copyright() const -> QString;
  auto license() const -> QString;
  auto description() const -> QString;
  auto url() const -> QString;
  auto category() const -> QString;
  auto revision() const -> QString;
  auto platformSpecification() const -> QRegularExpression;
  auto isAvailableForHostPlatform() const -> bool;
  auto isRequired() const -> bool;
  auto isExperimental() const -> bool;
  auto isEnabledByDefault() const -> bool;
  auto isEnabledBySettings() const -> bool;
  auto isEffectivelyEnabled() const -> bool;
  auto isEnabledIndirectly() const -> bool;
  auto isForceEnabled() const -> bool;
  auto isForceDisabled() const -> bool;
  auto dependencies() const -> QVector<PluginDependency>;
  auto metaData() const -> QJsonObject;

  using PluginArgumentDescriptions = QVector<PluginArgumentDescription>;
  auto argumentDescriptions() const -> PluginArgumentDescriptions;

  // other information, valid after 'Read' state is reached
  auto location() const -> QString;
  auto filePath() const -> QString;
  auto arguments() const -> QStringList;
  auto setArguments(const QStringList &arguments) -> void;
  auto addArgument(const QString &argument) -> void;
  auto provides(const QString &pluginName, const QString &version) const -> bool;

  // dependency specs, valid after 'Resolved' state is reached
  auto dependencySpecs() const -> QHash<PluginDependency, PluginSpec*>;
  auto requiresAny(const QSet<PluginSpec*> &plugins) const -> bool;

  // linked plugin instance, valid after 'Loaded' state is reached
  auto plugin() const -> IPlugin*;

  // state
  auto state() const -> State;
  auto hasError() const -> bool;
  auto errorString() const -> QString;
  auto setEnabledBySettings(bool value) -> void;

  static auto read(const QString &filePath) -> PluginSpec*;

private:
  PluginSpec();

  Internal::PluginSpecPrivate *d;
  friend class PluginView;
  friend class Internal::OptionsParser;
  friend class Internal::PluginManagerPrivate;
  friend class Internal::PluginSpecPrivate;
};

} // namespace ExtensionSystem
