// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakeprojectmanager_global.hpp"

#include <projectexplorer/abstractprocessstep.hpp>

#include <utils/aspects.hpp>
#include <utils/commandline.hpp>

#include <memory>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QListWidget;
QT_END_NAMESPACE

namespace ProjectExplorer {
class Abi;
class ArgumentsAspect;
} // namespace ProjectExplorer

namespace QtSupport {
class QtVersion;
}

namespace QmakeProjectManager {

class QmakeBuildConfiguration;
class QmakeBuildSystem;

namespace Internal {

class QMakeStepFactory : public ProjectExplorer::BuildStepFactory {
public:
  QMakeStepFactory();
};

} // namespace Internal

class QMAKEPROJECTMANAGER_EXPORT QMakeStepConfig {
public:
  // TODO remove, does nothing
  enum TargetArchConfig {
    NoArch,
    X86,
    X86_64,
    PowerPC,
    PowerPC64
  };

  enum OsType {
    NoOsType,
    IphoneSimulator,
    IphoneOS
  };

  // TODO remove, does nothing
  static auto targetArchFor(const ProjectExplorer::Abi &targetAbi, const QtSupport::QtVersion *version) -> TargetArchConfig;
  static auto osTypeFor(const ProjectExplorer::Abi &targetAbi, const QtSupport::QtVersion *version) -> OsType;

  auto toArguments() const -> QStringList;

  friend auto operator==(const QMakeStepConfig &a, const QMakeStepConfig &b) -> bool
  {
    return std::tie(a.archConfig, a.osType, a.linkQmlDebuggingQQ2) == std::tie(b.archConfig, b.osType, b.linkQmlDebuggingQQ2) && std::tie(a.useQtQuickCompiler, a.separateDebugInfo) == std::tie(b.useQtQuickCompiler, b.separateDebugInfo);
  }

  friend auto operator!=(const QMakeStepConfig &a, const QMakeStepConfig &b) -> bool { return !(a == b); }

  friend auto operator<<(QDebug dbg, const QMakeStepConfig &c) -> QDebug
  {
    dbg << c.archConfig << c.osType << (c.linkQmlDebuggingQQ2 == Utils::TriState::Enabled) << (c.useQtQuickCompiler == Utils::TriState::Enabled) << (c.separateDebugInfo == Utils::TriState::Enabled);
    return dbg;
  }

  // Actual data
  QString sysRoot;
  QString targetTriple;
  // TODO remove, does nothing
  TargetArchConfig archConfig = NoArch;
  OsType osType = NoOsType;
  Utils::TriState separateDebugInfo;
  Utils::TriState linkQmlDebuggingQQ2;
  Utils::TriState useQtQuickCompiler;
};

class QMAKEPROJECTMANAGER_EXPORT QMakeStep : public ProjectExplorer::AbstractProcessStep {
  Q_OBJECT
  friend class Internal::QMakeStepFactory;

public:
  QMakeStep(ProjectExplorer::BuildStepList *parent, Utils::Id id);

  auto qmakeBuildConfiguration() const -> QmakeBuildConfiguration*;
  auto qmakeBuildSystem() const -> QmakeBuildSystem*;
  auto init() -> bool override;
  auto setupOutputFormatter(Utils::OutputFormatter *formatter) -> void override;
  auto doRun() -> void override;
  auto createConfigWidget() -> QWidget* override;
  auto setForced(bool b) -> void;

  enum class ArgumentFlag {
    OmitProjectPath = 0x01,
    Expand = 0x02
  };

  Q_DECLARE_FLAGS(ArgumentFlags, ArgumentFlag);

  // the complete argument line
  auto allArguments(const QtSupport::QtVersion *v, ArgumentFlags flags = ArgumentFlags()) const -> QString;
  auto deducedArguments() const -> QMakeStepConfig;
  // arguments passed to the pro file parser
  auto parserArguments() -> QStringList;
  // arguments set by the user
  auto userArguments() const -> QString;
  auto setUserArguments(const QString &arguments) -> void;
  // Extra arguments for qmake and pro file parser. Not user editable via UI.
  auto extraArguments() const -> QStringList;
  auto setExtraArguments(const QStringList &args) -> void;
  /* Extra arguments for pro file parser only. Not user editable via UI.
   * This function is used in 3rd party plugin SailfishOS. */
  auto extraParserArguments() const -> QStringList;
  auto setExtraParserArguments(const QStringList &args) -> void;
  auto mkspec() const -> QString;
  auto makeCommand() const -> Utils::FilePath;
  auto makeArguments(const QString &makefile) const -> QString;
  auto effectiveQMakeCall() const -> QString;
  auto toMap() const -> QVariantMap override;

protected:
  auto fromMap(const QVariantMap &map) -> bool override;
  auto processStartupFailed() -> void override;
  auto processSucceeded(int exitCode, QProcess::ExitStatus status) -> bool override;

private:
  auto doCancel() -> void override;
  auto finish(bool success) -> void override;
  auto startOneCommand(const Utils::CommandLine &command) -> void;
  auto runNextCommand() -> void;

  // slots for handling buildconfiguration/step signals
  auto qtVersionChanged() -> void;
  auto qmakeBuildConfigChanged() -> void;
  auto linkQmlDebuggingLibraryChanged() -> void;
  auto useQtQuickCompilerChanged() -> void;
  auto separateDebugInfoChanged() -> void;
  auto abisChanged() -> void;

  // slots for dealing with user changes in our UI
  auto qmakeArgumentsLineEdited() -> void;
  auto buildConfigurationSelected() -> void;
  auto askForRebuild(const QString &title) -> void;
  auto recompileMessageBoxFinished(int button) -> void;
  auto updateAbiWidgets() -> void;
  auto updateEffectiveQMakeCall() -> void;

  Utils::CommandLine m_qmakeCommand;
  Utils::CommandLine m_makeCommand;
  ProjectExplorer::ArgumentsAspect *m_userArgs = nullptr;
  // Extra arguments for qmake and pro file parser
  QStringList m_extraArgs;
  // Extra arguments for pro file parser only
  QStringList m_extraParserArgs;

  // last values
  enum class State {
    IDLE = 0,
    RUN_QMAKE,
    RUN_MAKE_QMAKE_ALL,
    POST_PROCESS
  };

  bool m_wasSuccess = true;
  State m_nextState = State::IDLE;
  bool m_forced = false;
  bool m_needToRunQMake = false; // set in init(), read in run()
  bool m_runMakeQmake = false;
  bool m_scriptTemplate = false;
  QStringList m_selectedAbis;
  Utils::OutputFormatter *m_outputFormatter = nullptr;
  bool m_ignoreChange = false;
  QLabel *abisLabel = nullptr;
  Utils::SelectionAspect *m_buildType = nullptr;
  Utils::StringAspect *m_effectiveCall = nullptr;
  QListWidget *abisListWidget = nullptr;
};

} // namespace QmakeProjectManager

Q_DECLARE_OPERATORS_FOR_FLAGS(QmakeProjectManager::QMakeStep::ArgumentFlags);
