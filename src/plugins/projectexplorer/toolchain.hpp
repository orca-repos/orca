// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include "abi.hpp"
#include "devicesupport/idevice.hpp"
#include "headerpath.hpp"
#include "projectmacro.hpp"
#include "task.hpp"
#include "toolchaincache.hpp"

#include <utils/cpplanguage_details.hpp>
#include <utils/environment.hpp>
#include <utils/fileutils.hpp>
#include <utils/id.hpp>

#include <QDateTime>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

#include <functional>
#include <memory>

namespace Utils {
class OutputLineParser;
}

namespace ProjectExplorer {

namespace Internal {
class ToolChainPrivate;
}

namespace Deprecated {
// Deprecated in 4.3:
namespace Toolchain {

enum Language {
  None = 0,
  C,
  Cxx
};

auto languageId(Language l) -> QString;

} // namespace Toolchain
} // namespace Deprecated

class ToolChainConfigWidget;
class ToolChainFactory;
class Kit;

namespace Internal {
class ToolChainSettingsAccessor;
}

class PROJECTEXPLORER_EXPORT ToolChainDescription {
public:
  Utils::FilePath compilerPath;
  Utils::Id language;
};

// --------------------------------------------------------------------------
// ToolChain (documentation inside)
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT ToolChain {
public:
  enum Detection {
    ManualDetection,
    AutoDetection,
    AutoDetectionFromSdk,
    UninitializedDetection,
  };

  using Predicate = std::function<bool(const ToolChain *)>;

  virtual ~ToolChain();

  auto displayName() const -> QString;
  auto setDisplayName(const QString &name) -> void;
  auto isAutoDetected() const -> bool;
  auto isSdkProvided() const -> bool { return detection() == AutoDetectionFromSdk; }
  auto detection() const -> Detection;
  auto detectionSource() const -> QString;
  auto id() const -> QByteArray;
  virtual auto suggestedMkspecList() const -> QStringList;
  auto typeId() const -> Utils::Id;
  auto typeDisplayName() const -> QString;
  auto targetAbi() const -> Abi;
  auto setTargetAbi(const Abi &abi) -> void;
  virtual auto supportedAbis() const -> Abis;
  virtual auto originalTargetTriple() const -> QString { return QString(); }
  virtual auto extraCodeModelFlags() const -> QStringList { return QStringList(); }
  virtual auto installDir() const -> Utils::FilePath { return Utils::FilePath(); }
  virtual auto hostPrefersToolchain() const -> bool { return true; }
  virtual auto isValid() const -> bool;
  virtual auto languageExtensions(const QStringList &cxxflags) const -> Utils::LanguageExtensions = 0;
  virtual auto warningFlags(const QStringList &cflags) const -> Utils::WarningFlags = 0;
  virtual auto includedFiles(const QStringList &flags, const QString &directory) const -> QStringList;
  virtual auto sysRoot() const -> QString;
  auto explicitCodeModelTargetTriple() const -> QString;
  auto effectiveCodeModelTargetTriple() const -> QString;
  auto setExplicitCodeModelTargetTriple(const QString &triple) -> void;

  class MacroInspectionReport {
  public:
    Macros macros;
    Utils::LanguageVersion languageVersion;
  };

  using MacrosCache = std::shared_ptr<Cache<QStringList, MacroInspectionReport, 64>>;
  using HeaderPathsCache = std::shared_ptr<Cache<QPair<Utils::Environment, QStringList>, HeaderPaths>>;

  // A MacroInspectionRunner is created in the ui thread and runs in another thread.
  using MacroInspectionRunner = std::function<MacroInspectionReport(const QStringList &cxxflags)>;
  virtual auto createMacroInspectionRunner() const -> MacroInspectionRunner = 0;

  // A BuiltInHeaderPathsRunner is created in the ui thread and runs in another thread.
  using BuiltInHeaderPathsRunner = std::function<HeaderPaths(const QStringList &cxxflags, const QString &sysRoot, const QString &originalTargetTriple)>;
  virtual auto createBuiltInHeaderPathsRunner(const Utils::Environment &env) const -> BuiltInHeaderPathsRunner = 0;
  virtual auto addToEnvironment(Utils::Environment &env) const -> void = 0;
  virtual auto makeCommand(const Utils::Environment &env) const -> Utils::FilePath = 0;
  auto language() const -> Utils::Id;
  virtual auto compilerCommand() const -> Utils::FilePath; // FIXME: De-virtualize.
  auto setCompilerCommand(const Utils::FilePath &command) -> void;
  virtual auto createOutputParsers() const -> QList<Utils::OutputLineParser*> = 0;
  virtual auto operator ==(const ToolChain &) const -> bool;
  virtual auto createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget> = 0;
  auto clone() const -> ToolChain*;

  // Used by the toolchainmanager to save user-generated tool chains.
  // Make sure to call this function when deriving!
  virtual auto toMap() const -> QVariantMap;
  virtual auto validateKit(const Kit *k) const -> Tasks;
  virtual auto isJobCountSupported() const -> bool { return true; }
  auto setLanguage(Utils::Id language) -> void;
  auto setDetection(Detection d) -> void;
  auto setDetectionSource(const QString &source) -> void;
  static auto cxxLanguageVersion(const QByteArray &cplusplusMacroValue) -> Utils::LanguageVersion;
  static auto languageVersion(const Utils::Id &language, const Macros &macros) -> Utils::LanguageVersion;

  enum Priority {
    PriorityLow = 0,
    PriorityNormal = 10,
    PriorityHigh = 20,
  };

  virtual auto priority() const -> int { return PriorityNormal; }

protected:
  explicit ToolChain(Utils::Id typeId);

  auto setTypeDisplayName(const QString &typeName) -> void;
  auto setTargetAbiNoSignal(const Abi &abi) -> void;
  auto setTargetAbiKey(const QString &abiKey) -> void;
  auto setCompilerCommandKey(const QString &commandKey) -> void;
  auto predefinedMacrosCache() const -> const MacrosCache&;
  auto headerPathsCache() const -> const HeaderPathsCache&;
  auto toolChainUpdated() -> void;

  // Make sure to call this function when deriving!
  virtual auto fromMap(const QVariantMap &data) -> bool;
  static auto includedFiles(const QString &option, const QStringList &flags, const QString &directoryPath) -> QStringList;

private:
  ToolChain(const ToolChain &) = delete;
  auto operator=(const ToolChain &) -> ToolChain& = delete;

  const std::unique_ptr<Internal::ToolChainPrivate> d;

  friend class Internal::ToolChainSettingsAccessor;
  friend class ToolChainFactory;
};

using Toolchains = QList<ToolChain*>;

class PROJECTEXPLORER_EXPORT BadToolchain {
public:
  BadToolchain(const Utils::FilePath &filePath);
  BadToolchain(const Utils::FilePath &filePath, const Utils::FilePath &symlinkTarget, const QDateTime &timestamp);

  auto toMap() const -> QVariantMap;
  static auto fromMap(const QVariantMap &map) -> BadToolchain;

  Utils::FilePath filePath;
  Utils::FilePath symlinkTarget;
  QDateTime timestamp;
};

class PROJECTEXPLORER_EXPORT BadToolchains {
public:
  BadToolchains(const QList<BadToolchain> &toolchains = {});
  auto isBadToolchain(const Utils::FilePath &toolchain) const -> bool;

  auto toVariant() const -> QVariant;
  static auto fromVariant(const QVariant &v) -> BadToolchains;

  QList<BadToolchain> toolchains;
};

class PROJECTEXPLORER_EXPORT ToolchainDetector {
public:
  ToolchainDetector(const Toolchains &alreadyKnown, const IDevice::ConstPtr &device, const Utils::FilePaths &searchPaths);

  auto isBadToolchain(const Utils::FilePath &toolchain) const -> bool;
  auto addBadToolchain(const Utils::FilePath &toolchain) const -> void;

  const Toolchains alreadyKnown;
  const IDevice::ConstPtr device;
  const Utils::FilePaths searchPaths; // If empty use device path and/or magic.
};

class PROJECTEXPLORER_EXPORT ToolChainFactory {
  ToolChainFactory(const ToolChainFactory &) = delete;
  auto operator=(const ToolChainFactory &) -> ToolChainFactory& = delete;

public:
  ToolChainFactory();
  virtual ~ToolChainFactory();

  static auto allToolChainFactories() -> const QList<ToolChainFactory*>;
  auto displayName() const -> QString { return m_displayName; }
  auto supportedToolChainType() const -> Utils::Id;
  virtual auto autoDetect(const ToolchainDetector &detector) const -> Toolchains;
  virtual auto detectForImport(const ToolChainDescription &tcd) const -> Toolchains;
  virtual auto canCreate() const -> bool;
  virtual auto create() const -> ToolChain*;
  auto restore(const QVariantMap &data) -> ToolChain*;
  static auto idFromMap(const QVariantMap &data) -> QByteArray;
  static auto typeIdFromMap(const QVariantMap &data) -> Utils::Id;
  static auto autoDetectionToMap(QVariantMap &data, bool detected) -> void;
  static auto createToolChain(Utils::Id toolChainType) -> ToolChain*;
  auto supportedLanguages() const -> QList<Utils::Id>;
  auto setUserCreatable(bool userCreatable) -> void;

protected:
  auto setDisplayName(const QString &name) -> void { m_displayName = name; }
  auto setSupportedToolChainType(const Utils::Id &supportedToolChainType) -> void;
  auto setSupportedLanguages(const QList<Utils::Id> &supportedLanguages) -> void;
  auto setSupportsAllLanguages(bool supportsAllLanguages) -> void;
  auto setToolchainConstructor(const std::function<ToolChain *()> &constructor) -> void;

  class Candidate {
  public:
    Utils::FilePath compilerPath;
    QString compilerVersion;

    auto operator==(const Candidate &other) const -> bool
    {
      return compilerPath == other.compilerPath && compilerVersion == other.compilerVersion;
    }
  };

  using Candidates = QVector<Candidate>;

private:
  QString m_displayName;
  Utils::Id m_supportedToolChainType;
  QList<Utils::Id> m_supportedLanguages;
  bool m_supportsAllLanguages = false;
  bool m_userCreatable = false;
  std::function<ToolChain *()> m_toolchainConstructor;
};

} // namespace ProjectExplorer
