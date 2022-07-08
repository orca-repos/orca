// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include "projectexplorer_export.hpp"

#include "projectexplorerconstants.hpp"
#include "toolchain.hpp"
#include "abi.hpp"
#include "headerpath.hpp"

#include <utils/fileutils.hpp>

#include <functional>
#include <memory>

namespace ProjectExplorer {

namespace Internal {
class ClangToolChainFactory;
class ClangToolChainConfigWidget;
class GccToolChainConfigWidget;
class GccToolChainFactory;
class MingwToolChainFactory;
class LinuxIccToolChainFactory;
}

// --------------------------------------------------------------------------
// GccToolChain
// --------------------------------------------------------------------------

inline auto languageOption(Utils::Id languageId) -> const QStringList
{
  if (languageId == Constants::C_LANGUAGE_ID)
    return {"-x", "c"};
  return {"-x", "c++"};
}

inline auto gccPredefinedMacrosOptions(Utils::Id languageId) -> const QStringList
{
  return languageOption(languageId) + QStringList({"-E", "-dM"});
}

class PROJECTEXPLORER_EXPORT GccToolChain : public ToolChain {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::GccToolChain)

public:
  GccToolChain(Utils::Id typeId);

  auto originalTargetTriple() const -> QString override;
  auto installDir() const -> Utils::FilePath override;
  auto version() const -> QString;
  auto supportedAbis() const -> Abis override;
  auto languageExtensions(const QStringList &cxxflags) const -> Utils::LanguageExtensions override;
  auto warningFlags(const QStringList &cflags) const -> Utils::WarningFlags override;
  auto includedFiles(const QStringList &flags, const QString &directoryPath) const -> QStringList override;
  auto createMacroInspectionRunner() const -> MacroInspectionRunner override;
  auto createBuiltInHeaderPathsRunner(const Utils::Environment &env) const -> BuiltInHeaderPathsRunner override;
  auto addToEnvironment(Utils::Environment &env) const -> void override;
  auto makeCommand(const Utils::Environment &environment) const -> Utils::FilePath override;
  auto suggestedMkspecList() const -> QStringList override;
  auto createOutputParsers() const -> QList<Utils::OutputLineParser*> override;
  auto toMap() const -> QVariantMap override;
  auto fromMap(const QVariantMap &data) -> bool override;
  auto createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget> override;
  auto operator ==(const ToolChain &) const -> bool override;
  auto resetToolChain(const Utils::FilePath &) -> void;
  auto setPlatformCodeGenFlags(const QStringList &) -> void;
  auto extraCodeModelFlags() const -> QStringList override;
  auto platformCodeGenFlags() const -> QStringList;
  auto setPlatformLinkerFlags(const QStringList &) -> void;
  auto platformLinkerFlags() const -> QStringList;
  static auto addCommandPathToEnvironment(const Utils::FilePath &command, Utils::Environment &env) -> void;

  class DetectedAbisResult {
  public:
    DetectedAbisResult() = default;
    DetectedAbisResult(const Abis &supportedAbis, const QString &originalTargetTriple = {}) : supportedAbis(supportedAbis), originalTargetTriple(originalTargetTriple) { }

    Abis supportedAbis;
    QString originalTargetTriple;
  };

protected:
  using CacheItem = QPair<QStringList, Macros>;
  using GccCache = QVector<CacheItem>;

  auto setSupportedAbis(const Abis &abis) -> void;
  auto setOriginalTargetTriple(const QString &targetTriple) -> void;
  auto setInstallDir(const Utils::FilePath &installDir) -> void;
  auto setMacroCache(const QStringList &allCxxflags, const Macros &macroCache) const -> void;
  auto macroCache(const QStringList &allCxxflags) const -> Macros;

  virtual auto defaultDisplayName() const -> QString;
  virtual auto defaultLanguageExtensions() const -> Utils::LanguageExtensions;
  virtual auto detectSupportedAbis() const -> DetectedAbisResult;
  virtual auto detectVersion() const -> QString;
  virtual auto detectInstallDir() const -> Utils::FilePath;

  // Reinterpret options for compiler drivers inheriting from GccToolChain (e.g qcc) to apply -Wp option
  // that passes the initial options directly down to the gcc compiler
  using OptionsReinterpreter = std::function<QStringList(const QStringList &options)>;
  auto setOptionsReinterpreter(const OptionsReinterpreter &optionsReinterpreter) -> void;

  using ExtraHeaderPathsFunction = std::function<void(HeaderPaths &)>;
  auto initExtraHeaderPathsFunction(ExtraHeaderPathsFunction &&extraHeaderPathsFunction) const -> void;

  static auto builtInHeaderPaths(const Utils::Environment &env, const Utils::FilePath &compilerCommand, const QStringList &platformCodeGenFlags, OptionsReinterpreter reinterpretOptions, HeaderPathsCache headerCache, Utils::Id languageId, ExtraHeaderPathsFunction extraHeaderPathsFunction, const QStringList &flags, const QString &sysRoot, const QString &originalTargetTriple) -> HeaderPaths;
  static auto gccHeaderPaths(const Utils::FilePath &gcc, const QStringList &args, const Utils::Environment &env) -> HeaderPaths;

  class WarningFlagAdder {
  public:
    WarningFlagAdder(const QString &flag, Utils::WarningFlags &flags);

    auto operator ()(const char name[], Utils::WarningFlags flagsSet) -> void;
    auto triggered() const -> bool;

  private:
    QByteArray m_flagUtf8;
    Utils::WarningFlags &m_flags;
    bool m_doesEnable = false;
    bool m_triggered = false;
  };

private:
  auto updateSupportedAbis() const -> void;
  static auto gccPrepareArguments(const QStringList &flags, const QString &sysRoot, const QStringList &platformCodeGenFlags, Utils::Id languageId, OptionsReinterpreter reinterpretOptions) -> QStringList;

protected:
  QStringList m_platformCodeGenFlags;
  QStringList m_platformLinkerFlags;
  OptionsReinterpreter m_optionsReinterpreter = [](const QStringList &v) { return v; };
  mutable ExtraHeaderPathsFunction m_extraHeaderPathsFunction = [](HeaderPaths &) {};

private:
  mutable Abis m_supportedAbis;
  mutable QString m_originalTargetTriple;
  mutable HeaderPaths m_headerPaths;
  mutable QString m_version;
  mutable Utils::FilePath m_installDir;

  friend class Internal::GccToolChainConfigWidget;
  friend class Internal::GccToolChainFactory;
  friend class ToolChainFactory;
};

// --------------------------------------------------------------------------
// ClangToolChain
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT ClangToolChain : public GccToolChain {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::ClangToolChain)
public:
  ClangToolChain();
  explicit ClangToolChain(Utils::Id typeId);
  ~ClangToolChain() override;

  auto makeCommand(const Utils::Environment &environment) const -> Utils::FilePath override;
  auto languageExtensions(const QStringList &cxxflags) const -> Utils::LanguageExtensions override;
  auto warningFlags(const QStringList &cflags) const -> Utils::WarningFlags override;
  auto createOutputParsers() const -> QList<Utils::OutputLineParser*> override;
  auto suggestedMkspecList() const -> QStringList override;
  auto addToEnvironment(Utils::Environment &env) const -> void override;
  auto originalTargetTriple() const -> QString override;
  auto sysRoot() const -> QString override;
  auto createBuiltInHeaderPathsRunner(const Utils::Environment &env) const -> BuiltInHeaderPathsRunner override;
  auto createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget> override;
  auto toMap() const -> QVariantMap override;
  auto fromMap(const QVariantMap &data) -> bool override;

protected:
  auto defaultLanguageExtensions() const -> Utils::LanguageExtensions override;
  auto syncAutodetectedWithParentToolchains() -> void;

private:
  QByteArray m_parentToolChainId;
  QMetaObject::Connection m_mingwToolchainAddedConnection;
  QMetaObject::Connection m_thisToolchainRemovedConnection;

  friend class Internal::ClangToolChainFactory;
  friend class Internal::ClangToolChainConfigWidget;
  friend class ToolChainFactory;
};

// --------------------------------------------------------------------------
// MingwToolChain
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT MingwToolChain : public GccToolChain {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::MingwToolChain)

public:
  auto makeCommand(const Utils::Environment &environment) const -> Utils::FilePath override;
  auto suggestedMkspecList() const -> QStringList override;

private:
  MingwToolChain();

  friend class Internal::MingwToolChainFactory;
  friend class ToolChainFactory;
};

// --------------------------------------------------------------------------
// LinuxIccToolChain
// --------------------------------------------------------------------------

class PROJECTEXPLORER_EXPORT LinuxIccToolChain : public GccToolChain {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::LinuxIccToolChain)

public:
  auto languageExtensions(const QStringList &cxxflags) const -> Utils::LanguageExtensions override;
  auto createOutputParsers() const -> QList<Utils::OutputLineParser*> override;
  auto suggestedMkspecList() const -> QStringList override;

private:
  LinuxIccToolChain();

  friend class Internal::LinuxIccToolChainFactory;
  friend class ToolChainFactory;
};

// --------------------------------------------------------------------------
// Factories
// --------------------------------------------------------------------------

namespace Internal {

class GccToolChainFactory : public ToolChainFactory {
public:
  GccToolChainFactory();

  auto autoDetect(const ToolchainDetector &detector) const -> Toolchains override;
  auto detectForImport(const ToolChainDescription &tcd) const -> Toolchains override;

protected:
  enum class DetectVariants {
    Yes,
    No
  };

  using ToolchainChecker = std::function<bool(const ToolChain *)>;
  auto autoDetectToolchains(const QString &compilerName, DetectVariants detectVariants, const Utils::Id language, const Utils::Id requiredTypeId, const ToolchainDetector &detector, const ToolchainChecker &checker = {}) const -> Toolchains;
  auto autoDetectToolChain(const ToolChainDescription &tcd, const ToolchainChecker &checker = {}) const -> Toolchains;
};

class ClangToolChainFactory : public GccToolChainFactory {
public:
  ClangToolChainFactory();

  auto autoDetect(const ToolchainDetector &detector) const -> Toolchains final;
  auto detectForImport(const ToolChainDescription &tcd) const -> Toolchains final;
};

class MingwToolChainFactory : public GccToolChainFactory {
public:
  MingwToolChainFactory();

  auto autoDetect(const ToolchainDetector &detector) const -> Toolchains final;
  auto detectForImport(const ToolChainDescription &tcd) const -> Toolchains final;
};

class LinuxIccToolChainFactory : public GccToolChainFactory {
public:
  LinuxIccToolChainFactory();

  auto autoDetect(const ToolchainDetector &detector) const -> Toolchains final;
  auto detectForImport(const ToolChainDescription &tcd) const -> Toolchains final;
};

} // namespace Internal
} // namespace ProjectExplorer
