// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "processparameters.hpp"

#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/theme/theme.hpp>

#include <QDir>

/*!
    \class ProjectExplorer::ProcessParameters

    \brief The ProcessParameters class aggregates all parameters needed to start
    a process.

    It offers a set of functions which expand macros and environment variables
    inside the raw parameters to obtain final values for starting a process
    or for display purposes.

    \sa ProjectExplorer::AbstractProcessStep
*/

using namespace Utils;

namespace ProjectExplorer {

ProcessParameters::ProcessParameters() = default;

/*!
    Sets the command to run.
*/
auto ProcessParameters::setCommandLine(const CommandLine &cmdLine) -> void
{
  m_command = cmdLine;
  m_effectiveCommand.clear();
  m_effectiveArguments.clear();

  effectiveCommand();
  effectiveArguments();
}

/*!
    Sets the \a workingDirectory for the process for a build configuration.

    Should be called from init().
*/

auto ProcessParameters::setWorkingDirectory(const FilePath &workingDirectory) -> void
{
  m_workingDirectory = workingDirectory;
  m_effectiveWorkingDirectory.clear();

  effectiveWorkingDirectory();
}

/*!
    \fn void ProjectExplorer::ProcessParameters::setEnvironment(const Utils::Environment &env)
    Sets the environment \a env for running the command.

    Should be called from init().
*/

/*!
   \fn  void ProjectExplorer::ProcessParameters::setMacroExpander(Utils::MacroExpander *mx)
   Sets the macro expander \a mx to use on the command, arguments, and working
   dir.

   \note The caller retains ownership of the object.
*/

/*!
    Gets the fully expanded working directory.
*/

auto ProcessParameters::effectiveWorkingDirectory() const -> FilePath
{
  if (m_effectiveWorkingDirectory.isEmpty()) {
    m_effectiveWorkingDirectory = m_workingDirectory;
    auto path = m_workingDirectory.path();
    if (m_macroExpander)
      path = m_macroExpander->expand(path);
    m_effectiveWorkingDirectory.setPath(QDir::cleanPath(m_environment.expandVariables(path)));
  }
  return m_effectiveWorkingDirectory;
}

/*!
    Gets the fully expanded command name to run.
*/

auto ProcessParameters::effectiveCommand() const -> FilePath
{
  if (m_effectiveCommand.isEmpty()) {
    auto cmd = m_command.executable();
    if (m_macroExpander)
      cmd = m_macroExpander->expand(cmd);
    if (cmd.needsDevice()) {
      // Assume this is already good. FIXME: It is possibly not, so better fix  searchInPath.
      m_effectiveCommand = cmd;
    } else {
      m_effectiveCommand = m_environment.searchInPath(cmd.toString(), {effectiveWorkingDirectory()});
    }
    m_commandMissing = m_effectiveCommand.isEmpty();
    if (m_commandMissing)
      m_effectiveCommand = cmd;
  }
  return m_effectiveCommand;
}

/*!
    Returns \c true if effectiveCommand() would return only a fallback.
*/

auto ProcessParameters::commandMissing() const -> bool
{
  effectiveCommand();
  return m_commandMissing;
}

auto ProcessParameters::effectiveArguments() const -> QString
{
  if (m_effectiveArguments.isEmpty()) {
    m_effectiveArguments = m_command.arguments();
    if (m_macroExpander)
      m_effectiveArguments = m_macroExpander->expand(m_effectiveArguments);
  }
  return m_effectiveArguments;
}

auto ProcessParameters::prettyCommand() const -> QString
{
  auto cmd = m_command.executable().toString();
  if (m_macroExpander)
    cmd = m_macroExpander->expand(cmd);
  return FilePath::fromString(cmd).fileName();
}

auto ProcessParameters::prettyArguments() const -> QString
{
  auto margs = effectiveArguments();
  const auto workDir = effectiveWorkingDirectory();
  ProcessArgs::SplitError err;
  const auto args = ProcessArgs::prepareArgs(margs, &err, HostOsInfo::hostOs(), &m_environment, &workDir);
  if (err != ProcessArgs::SplitOk)
    return margs; // Sorry, too complex - just fall back.
  return args.toString();
}

static auto invalidCommandMessage(const QString &displayName) -> QString
{
  return QString("<b>%1:</b> <font color='%3'>%2</font>").arg(displayName, QtcProcess::tr("Invalid command"), orcaTheme()->color(Theme::TextColorError).name());
}

auto ProcessParameters::summary(const QString &displayName) const -> QString
{
  if (m_commandMissing)
    return invalidCommandMessage(displayName);

  return QString::fromLatin1("<b>%1:</b> %2 %3").arg(displayName, ProcessArgs::quoteArg(prettyCommand()), prettyArguments());
}

auto ProcessParameters::summaryInWorkdir(const QString &displayName) const -> QString
{
  if (m_commandMissing)
    return invalidCommandMessage(displayName);

  return QString::fromLatin1("<b>%1:</b> %2 %3 in %4").arg(displayName, ProcessArgs::quoteArg(prettyCommand()), prettyArguments(), QDir::toNativeSeparators(effectiveWorkingDirectory().toString()));
}

} // ProcessExplorer
