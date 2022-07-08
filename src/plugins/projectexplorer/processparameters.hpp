// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/commandline.hpp>
#include <utils/environment.hpp>
#include <utils/fileutils.hpp>

namespace Utils {
class MacroExpander;
} // Utils

namespace ProjectExplorer {

// Documentation inside.
class PROJECTEXPLORER_EXPORT ProcessParameters {
public:
  ProcessParameters();

  auto setCommandLine(const Utils::CommandLine &cmdLine) -> void;
  auto command() const -> Utils::CommandLine { return m_command; }
  auto setWorkingDirectory(const Utils::FilePath &workingDirectory) -> void;
  auto workingDirectory() const -> Utils::FilePath { return m_workingDirectory; }
  auto setEnvironment(const Utils::Environment &env) -> void { m_environment = env; }
  auto environment() const -> Utils::Environment { return m_environment; }
  auto setMacroExpander(Utils::MacroExpander *mx) -> void { m_macroExpander = mx; }
  auto macroExpander() const -> Utils::MacroExpander* { return m_macroExpander; }

  /// Get the fully expanded working directory:
  auto effectiveWorkingDirectory() const -> Utils::FilePath;
  /// Get the fully expanded command name to run:
  auto effectiveCommand() const -> Utils::FilePath;
  /// Get the fully expanded arguments to use:
  auto effectiveArguments() const -> QString;

  /// True if effectiveCommand() would return only a fallback
  auto commandMissing() const -> bool;
  auto prettyCommand() const -> QString;
  auto prettyArguments() const -> QString;
  auto summary(const QString &displayName) const -> QString;
  auto summaryInWorkdir(const QString &displayName) const -> QString;

private:
  Utils::FilePath m_workingDirectory;
  Utils::CommandLine m_command;
  Utils::Environment m_environment;
  Utils::MacroExpander *m_macroExpander = nullptr;

  mutable Utils::FilePath m_effectiveWorkingDirectory;
  mutable Utils::FilePath m_effectiveCommand;
  mutable QString m_effectiveArguments;
  mutable bool m_commandMissing = false;
};

} // namespace ProjectExplorer
