// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "abi.hpp"
#include "abiwidget.hpp"
#include "toolchain.hpp"
#include "toolchaincache.hpp"
#include "toolchainconfigwidget.hpp"

#include <QFutureWatcher>

#include <utils/environment.hpp>
#include <utils/optional.hpp>

QT_FORWARD_DECLARE_CLASS(QLabel)
QT_FORWARD_DECLARE_CLASS(QComboBox)
QT_FORWARD_DECLARE_CLASS(QVersionNumber)

namespace Utils {
class PathChooser;
}

namespace ProjectExplorer {
namespace Internal {

// --------------------------------------------------------------------------
// MsvcToolChain
// --------------------------------------------------------------------------

class MsvcToolChain : public ToolChain {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::MsvcToolChain)
public:
  enum Type {
    WindowsSDK,
    VS
  };

  enum Platform {
    x86,
    amd64,
    x86_amd64,
    ia64,
    x86_ia64,
    arm,
    x86_arm,
    amd64_arm,
    amd64_x86,
    x86_arm64,
    amd64_arm64
  };

  explicit MsvcToolChain(Utils::Id typeId);
  ~MsvcToolChain() override;

  auto isValid() const -> bool override;
  auto originalTargetTriple() const -> QString override;
  auto suggestedMkspecList() const -> QStringList override;
  auto supportedAbis() const -> Abis override;
  auto toMap() const -> QVariantMap override;
  auto fromMap(const QVariantMap &data) -> bool override;
  auto createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget> override;
  auto hostPrefersToolchain() const -> bool override;
  auto createMacroInspectionRunner() const -> MacroInspectionRunner override;
  auto languageExtensions(const QStringList &cxxflags) const -> Utils::LanguageExtensions override;
  auto warningFlags(const QStringList &cflags) const -> Utils::WarningFlags override;
  auto includedFiles(const QStringList &flags, const QString &directoryPath) const -> QStringList override;
  auto createBuiltInHeaderPathsRunner(const Utils::Environment &env) const -> BuiltInHeaderPathsRunner override;
  auto addToEnvironment(Utils::Environment &env) const -> void override;
  auto makeCommand(const Utils::Environment &environment) const -> Utils::FilePath override;
  auto createOutputParsers() const -> QList<Utils::OutputLineParser*> override;
  auto varsBatArg() const -> QString { return m_varsBatArg; }
  auto varsBat() const -> QString { return m_vcvarsBat; }
  auto setupVarsBat(const Abi &abi, const QString &varsBat, const QString &varsBatArg) -> void;
  auto resetVarsBat() -> void;
  auto platform() const -> Platform;
  auto operator==(const ToolChain &) const -> bool override;
  auto isJobCountSupported() const -> bool override { return false; }
  auto priority() const -> int override;
  static auto cancelMsvcToolChainDetection() -> void;
  static auto generateEnvironmentSettings(const Utils::Environment &env, const QString &batchFile, const QString &batchArgs, QMap<QString, QString> &envPairs) -> Utils::optional<QString>;

protected:
  class WarningFlagAdder {
    int m_warningCode = 0;
    Utils::WarningFlags &m_flags;
    bool m_doesEnable = false;
    bool m_triggered = false;

  public:
    WarningFlagAdder(const QString &flag, Utils::WarningFlags &flags);

    auto operator()(int warningCode, Utils::WarningFlags flagsSet) -> void;
    auto triggered() const -> bool;
  };

  static auto inferWarningsForLevel(int warningLevel, Utils::WarningFlags &flags) -> void;
  auto readEnvironmentSetting(const Utils::Environment &env) const -> Utils::Environment;
  // Function must be thread-safe!
  virtual auto msvcPredefinedMacros(const QStringList &cxxflags, const Utils::Environment &env) const -> Macros;
  virtual auto msvcLanguageVersion(const QStringList &cxxflags, const Utils::Id &language, const Macros &macros) const -> Utils::LanguageVersion;

  struct GenerateEnvResult {
    Utils::optional<QString> error;
    Utils::EnvironmentItems environmentItems;
  };

  static auto environmentModifications(QFutureInterface<GenerateEnvResult> &future, QString vcvarsBat, QString varsBatArg) -> void;
  auto initEnvModWatcher(const QFuture<GenerateEnvResult> &future) -> void;

  mutable QMutex m_headerPathsMutex;
  mutable QHash<QStringList, HeaderPaths> m_headerPathsPerEnv;

private:
  auto updateEnvironmentModifications(Utils::EnvironmentItems modifications) -> void;
  auto rescanForCompiler() -> void;

  mutable Utils::EnvironmentItems m_environmentModifications;
  mutable QFutureWatcher<GenerateEnvResult> m_envModWatcher;
  mutable Utils::Environment m_lastEnvironment;   // Last checked 'incoming' environment.
  mutable Utils::Environment m_resultEnvironment; // Resulting environment for VC

protected:
  QString m_vcvarsBat;
  QString m_varsBatArg; // Argument
};

class PROJECTEXPLORER_EXPORT ClangClToolChain : public MsvcToolChain {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::ClangClToolChain)

public:
  ClangClToolChain();

  auto isValid() const -> bool override;
  auto suggestedMkspecList() const -> QStringList override;
  auto addToEnvironment(Utils::Environment &env) const -> void override;
  auto compilerCommand() const -> Utils::FilePath override; // FIXME: Remove
  auto createOutputParsers() const -> QList<Utils::OutputLineParser*> override;
  auto toMap() const -> QVariantMap override;
  auto fromMap(const QVariantMap &data) -> bool override;
  auto createConfigurationWidget() -> std::unique_ptr<ToolChainConfigWidget> override;
  auto createBuiltInHeaderPathsRunner(const Utils::Environment &env) const -> BuiltInHeaderPathsRunner override;
  auto msvcToolchains() const -> const QList<MsvcToolChain*>&;
  auto clangPath() const -> Utils::FilePath { return m_clangPath; }
  auto setClangPath(const Utils::FilePath &path) -> void { m_clangPath = path; }
  auto msvcPredefinedMacros(const QStringList &cxxflags, const Utils::Environment &env) const -> Macros override;
  auto msvcLanguageVersion(const QStringList &cxxflags, const Utils::Id &language, const Macros &macros) const -> Utils::LanguageVersion override;
  auto operator==(const ToolChain &) const -> bool override;
  auto priority() const -> int override;

private:
  Utils::FilePath m_clangPath;
};

// --------------------------------------------------------------------------
// MsvcToolChainFactory
// --------------------------------------------------------------------------

class MsvcToolChainFactory : public ToolChainFactory {
public:
  MsvcToolChainFactory();

  auto autoDetect(const ToolchainDetector &detector) const -> Toolchains final;
  auto canCreate() const -> bool final;
  static auto vcVarsBatFor(const QString &basePath, MsvcToolChain::Platform platform, const QVersionNumber &v) -> QString;
};

class ClangClToolChainFactory : public ToolChainFactory {
public:
  ClangClToolChainFactory();

  auto autoDetect(const ToolchainDetector &detector) const -> Toolchains final;
  auto canCreate() const -> bool final;
};

// --------------------------------------------------------------------------
// MsvcBasedToolChainConfigWidget
// --------------------------------------------------------------------------

class MsvcBasedToolChainConfigWidget : public ToolChainConfigWidget {
  Q_OBJECT public:
  explicit MsvcBasedToolChainConfigWidget(ToolChain *);

protected:
  auto applyImpl() -> void override {}
  auto discardImpl() -> void override { setFromMsvcToolChain(); }
  auto isDirtyImpl() const -> bool override { return false; }
  auto makeReadOnlyImpl() -> void override {}
  auto setFromMsvcToolChain() -> void;
  
  QLabel *m_nameDisplayLabel;
  QLabel *m_varsBatDisplayLabel;
};

// --------------------------------------------------------------------------
// MsvcToolChainConfigWidget
// --------------------------------------------------------------------------

class MsvcToolChainConfigWidget : public MsvcBasedToolChainConfigWidget {
  Q_OBJECT public:
  explicit MsvcToolChainConfigWidget(ToolChain *);

private:
  auto applyImpl() -> void override;
  auto discardImpl() -> void override;
  auto isDirtyImpl() const -> bool override;
  auto makeReadOnlyImpl() -> void override;
  auto setFromMsvcToolChain() -> void;
  auto updateAbis() -> void;
  auto handleVcVarsChange(const QString &vcVars) -> void;
  auto handleVcVarsArchChange(const QString &arch) -> void;
  auto vcVarsArguments() const -> QString;

  QComboBox *m_varsBatPathCombo;
  QComboBox *m_varsBatArchCombo;
  QLineEdit *m_varsBatArgumentsEdit;
  AbiWidget *m_abiWidget;
};

// --------------------------------------------------------------------------
// ClangClToolChainConfigWidget
// --------------------------------------------------------------------------

class ClangClToolChainConfigWidget : public MsvcBasedToolChainConfigWidget {
  Q_OBJECT

public:
  explicit ClangClToolChainConfigWidget(ToolChain *);

protected:
  auto applyImpl() -> void override;
  auto discardImpl() -> void override;
  auto isDirtyImpl() const -> bool override { return false; }
  auto makeReadOnlyImpl() -> void override;

private:
  auto setFromClangClToolChain() -> void;

  QLabel *m_llvmDirLabel = nullptr;
  QComboBox *m_varsBatDisplayCombo = nullptr;
  Utils::PathChooser *m_compilerCommand = nullptr;
};

} // namespace Internal
} // namespace ProjectExplorer
