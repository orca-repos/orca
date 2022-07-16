// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "abstractprocessstep.hpp"

#include <utils/aspects.hpp>
#include <utils/fileutils.hpp>

namespace Utils {
class Environment;
}

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT MakeStep : public AbstractProcessStep {
  Q_OBJECT

public:
  enum MakeCommandType {
    Display,
    Execution
  };

  explicit MakeStep(BuildStepList *parent, Utils::Id id);

  auto setAvailableBuildTargets(const QStringList &buildTargets) -> void;
  auto setSelectedBuildTarget(const QString &buildTarget) -> void;
  auto init() -> bool override;
  auto setupOutputFormatter(Utils::OutputFormatter *formatter) -> void override;
  auto createConfigWidget() -> QWidget* override;
  auto availableTargets() const -> QStringList;
  auto userArguments() const -> QString;
  auto setUserArguments(const QString &args) -> void;
  auto makeCommand() const -> Utils::FilePath;
  auto setMakeCommand(const Utils::FilePath &command) -> void;
  auto makeExecutable() const -> Utils::FilePath;
  auto effectiveMakeCommand(MakeCommandType type) const -> Utils::CommandLine;
  static auto defaultDisplayName() -> QString;
  auto defaultMakeCommand() const -> Utils::FilePath;
  static auto msgNoMakeCommand() -> QString;
  static auto makeCommandMissingTask() -> Task;
  virtual auto isJobCountSupported() const -> bool;
  auto jobCount() const -> int;
  auto jobCountOverridesMakeflags() const -> bool;
  auto makeflagsContainsJobCount() const -> bool;
  auto userArgsContainsJobCount() const -> bool;
  auto makeflagsJobCountMismatch() const -> bool;
  auto disablingForSubdirsSupported() const -> bool { return m_disablingForSubDirsSupported; }
  auto enabledForSubDirs() const -> bool;
  auto makeEnvironment() const -> Utils::Environment;

  // FIXME: All unused, remove in 4.15.
  auto setBuildTarget(const QString &buildTarget) -> void { setSelectedBuildTarget(buildTarget); }
  auto buildsTarget(const QString &target) const -> bool;
  auto setBuildTarget(const QString &target, bool on) -> void;

protected:
  auto supportDisablingForSubdirs() -> void { m_disablingForSubDirsSupported = true; }
  virtual auto displayArguments() const -> QStringList;
  auto makeCommandAspect() const -> Utils::StringAspect* { return m_makeCommandAspect; }
  auto buildTargetsAspect() const -> Utils::MultiSelectionAspect* { return m_buildTargetsAspect; }
  auto userArgumentsAspect() const -> Utils::StringAspect* { return m_userArgumentsAspect; }
  auto overrideMakeflagsAspect() const -> Utils::BoolAspect* { return m_overrideMakeflagsAspect; }
  auto nonOverrideWarning() const -> Utils::TextDisplay* { return m_nonOverrideWarning; }
  auto jobCountAspect() const -> Utils::IntegerAspect* { return m_userJobCountAspect; }
  auto disabledForSubdirsAspect() const -> Utils::BoolAspect* { return m_disabledForSubdirsAspect; }

private:
  static auto defaultJobCount() -> int;
  auto jobArguments() const -> QStringList;

  Utils::MultiSelectionAspect *m_buildTargetsAspect = nullptr;
  QStringList m_availableTargets; // FIXME: Unused, remove in 4.15.
  Utils::StringAspect *m_makeCommandAspect = nullptr;
  Utils::StringAspect *m_userArgumentsAspect = nullptr;
  Utils::IntegerAspect *m_userJobCountAspect = nullptr;
  Utils::BoolAspect *m_overrideMakeflagsAspect = nullptr;
  Utils::BoolAspect *m_disabledForSubdirsAspect = nullptr;
  Utils::TextDisplay *m_nonOverrideWarning = nullptr;
  bool m_disablingForSubDirsSupported = false;
};

} // namespace ProjectExplorer
