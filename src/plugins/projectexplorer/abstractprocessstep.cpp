// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "abstractprocessstep.hpp"
#include "buildconfiguration.hpp"
#include "buildstep.hpp"
#include "ioutputparser.hpp"
#include "processparameters.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectexplorersettings.hpp"
#include "target.hpp"
#include "task.hpp"

#include <utils/fileutils.hpp>
#include <utils/outputformatter.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include <QTextDecoder>

#include <algorithm>
#include <memory>

using namespace Utils;

namespace ProjectExplorer {

/*!
    \class ProjectExplorer::AbstractProcessStep

    \brief The AbstractProcessStep class is a convenience class that can be
    used as a base class instead of BuildStep.

    It should be used as a base class if your buildstep just needs to run a process.

    Usage:
    \list
    \li Use processParameters() to configure the process you want to run
    (you need to do that before calling AbstractProcessStep::init()).
    \li Inside YourBuildStep::init() call AbstractProcessStep::init().
    \li Inside YourBuildStep::run() call AbstractProcessStep::run(), which automatically starts the process
    and by default adds the output on stdOut and stdErr to the OutputWindow.
    \li If you need to process the process output override stdOut() and/or stdErr.
    \endlist

    The two functions processStarted() and processFinished() are called after starting/finishing the process.
    By default they add a message to the output window.

    Use setEnabled() to control whether the BuildStep needs to run. (A disabled BuildStep immediately returns true,
    from the run function.)

    \sa ProjectExplorer::ProcessParameters
*/

/*!
    \fn void ProjectExplorer::AbstractProcessStep::setEnabled(bool b)

    Enables or disables a BuildStep.

    Disabled BuildSteps immediately return true from their run function.
    Should be called from init().
*/

/*!
    \fn ProcessParameters *ProjectExplorer::AbstractProcessStep::processParameters()

    Obtains a reference to the parameters for the actual process to run.

     Should be used in init().
*/

class AbstractProcessStep::Private {
public:
  Private(AbstractProcessStep *q) : q(q) {}

  auto cleanUp(int exitCode, QProcess::ExitStatus status) -> void;

  AbstractProcessStep *q;
  std::unique_ptr<QtcProcess> m_process;
  ProcessParameters m_param;
  std::function<CommandLine()> m_commandLineProvider;
  std::function<FilePath()> m_workingDirectoryProvider;
  std::function<void(Environment &)> m_environmentModifier;
  bool m_ignoreReturnValue = false;
  bool m_lowPriority = false;
  std::unique_ptr<QTextDecoder> stdoutStream;
  std::unique_ptr<QTextDecoder> stderrStream;
  OutputFormatter *outputFormatter = nullptr;
};

AbstractProcessStep::AbstractProcessStep(BuildStepList *bsl, Id id) : BuildStep(bsl, id), d(new Private(this)) {}

AbstractProcessStep::~AbstractProcessStep()
{
  delete d;
}

auto AbstractProcessStep::emitFaultyConfigurationMessage() -> void
{
  emit addOutput(tr("Configuration is faulty. Check the Issues view for details."), OutputFormat::NormalMessage);
}

auto AbstractProcessStep::ignoreReturnValue() const -> bool
{
  return d->m_ignoreReturnValue;
}

/*!
    If \a ignoreReturnValue is set to true, then the abstractprocess step will
    return success even if the return value indicates otherwise.
*/

auto AbstractProcessStep::setIgnoreReturnValue(bool b) -> void
{
  d->m_ignoreReturnValue = b;
}

auto AbstractProcessStep::setEnvironmentModifier(const std::function<void (Environment &)> &modifier) -> void
{
  d->m_environmentModifier = modifier;
}

auto AbstractProcessStep::setUseEnglishOutput() -> void
{
  d->m_environmentModifier = [](Environment &env) { env.setupEnglishOutput(); };
}

auto AbstractProcessStep::setCommandLineProvider(const std::function<CommandLine()> &provider) -> void
{
  d->m_commandLineProvider = provider;
}

auto AbstractProcessStep::setWorkingDirectoryProvider(const std::function<FilePath()> &provider) -> void
{
  d->m_workingDirectoryProvider = provider;
}

/*!
    Reimplemented from BuildStep::init(). You need to call this from
    YourBuildStep::init().
*/

auto AbstractProcessStep::init() -> bool
{
  if (d->m_process)
    return false;

  setupProcessParameters(processParameters());

  return true;
}

auto AbstractProcessStep::setupOutputFormatter(OutputFormatter *formatter) -> void
{
  formatter->setDemoteErrorsToWarnings(d->m_ignoreReturnValue);
  d->outputFormatter = formatter;
  BuildStep::setupOutputFormatter(formatter);
}

/*!
    Reimplemented from BuildStep::init(). You need to call this from
    YourBuildStep::run().
*/

auto AbstractProcessStep::doRun() -> void
{
  const auto wd = d->m_param.effectiveWorkingDirectory();
  if (!wd.exists()) {
    if (!wd.createDir()) {
      emit addOutput(tr("Could not create directory \"%1\"").arg(wd.toUserOutput()), OutputFormat::ErrorMessage);
      finish(false);
      return;
    }
  }

  const CommandLine effectiveCommand(d->m_param.effectiveCommand(), d->m_param.effectiveArguments(), CommandLine::Raw);
  if (!effectiveCommand.executable().isExecutableFile()) {
    processStartupFailed();
    finish(false);
    return;
  }

  d->stdoutStream = std::make_unique<QTextDecoder>(buildEnvironment().hasKey("VSLANG") ? QTextCodec::codecForName("UTF-8") : QTextCodec::codecForLocale());
  d->stderrStream = std::make_unique<QTextDecoder>(QTextCodec::codecForLocale());

  d->m_process.reset(new QtcProcess());
  d->m_process->setUseCtrlCStub(HostOsInfo::isWindowsHost());
  d->m_process->setWorkingDirectory(wd);
  // Enforce PWD in the environment because some build tools use that.
  // PWD can be different from getcwd in case of symbolic links (getcwd resolves symlinks).
  // For example Clang uses PWD for paths in debug info, see QTCREATORBUG-23788
  auto envWithPwd = d->m_param.environment();
  envWithPwd.set("PWD", d->m_process->workingDirectory().path());
  d->m_process->setEnvironment(envWithPwd);
  d->m_process->setCommand(effectiveCommand);
  if (d->m_lowPriority && ProjectExplorerPlugin::projectExplorerSettings().lowBuildPriority)
    d->m_process->setLowPriority();

  connect(d->m_process.get(), &QtcProcess::readyReadStandardOutput, this, &AbstractProcessStep::processReadyReadStdOutput);
  connect(d->m_process.get(), &QtcProcess::readyReadStandardError, this, &AbstractProcessStep::processReadyReadStdError);
  connect(d->m_process.get(), &QtcProcess::finished, this, &AbstractProcessStep::slotProcessFinished);

  d->m_process->start();
  if (!d->m_process->waitForStarted()) {
    processStartupFailed();
    d->m_process.reset();
    finish(false);
    return;
  }
  processStarted();
}

auto AbstractProcessStep::setLowPriority() -> void
{
  d->m_lowPriority = true;
}

auto AbstractProcessStep::doCancel() -> void
{
  d->cleanUp(-1, QProcess::CrashExit);
}

auto AbstractProcessStep::processParameters() -> ProcessParameters*
{
  return &d->m_param;
}

auto AbstractProcessStep::setupProcessParameters(ProcessParameters *params) const -> void
{
  params->setMacroExpander(macroExpander());

  auto env = buildEnvironment();
  if (d->m_environmentModifier)
    d->m_environmentModifier(env);
  params->setEnvironment(env);

  if (d->m_workingDirectoryProvider)
    params->setWorkingDirectory(d->m_workingDirectoryProvider());
  else
    params->setWorkingDirectory(buildDirectory());

  if (d->m_commandLineProvider)
    params->setCommandLine(d->m_commandLineProvider());
}

auto AbstractProcessStep::Private::cleanUp(int exitCode, QProcess::ExitStatus status) -> void
{
  // The process has finished, leftover data is read in processFinished
  q->processFinished(exitCode, status);
  const auto returnValue = q->processSucceeded(exitCode, status) || m_ignoreReturnValue;

  m_process.reset();

  // Report result
  q->finish(returnValue);
}

/*!
    Called after the process is started.

    The default implementation adds a process-started message to the output
    message.
*/

auto AbstractProcessStep::processStarted() -> void
{
  emit addOutput(tr("Starting: \"%1\" %2").arg(d->m_param.effectiveCommand().toUserOutput(), d->m_param.prettyArguments()), OutputFormat::NormalMessage);
}

/*!
    Called after the process is finished.

    The default implementation adds a line to the output window.
*/

auto AbstractProcessStep::processFinished(int exitCode, QProcess::ExitStatus status) -> void
{
  auto command = d->m_param.effectiveCommand().toUserOutput();
  if (status == QProcess::NormalExit && exitCode == 0) {
    emit addOutput(tr("The process \"%1\" exited normally.").arg(command), OutputFormat::NormalMessage);
  } else if (status == QProcess::NormalExit) {
    emit addOutput(tr("The process \"%1\" exited with code %2.").arg(command, QString::number(exitCode)), OutputFormat::ErrorMessage);
  } else {
    emit addOutput(tr("The process \"%1\" crashed.").arg(command), OutputFormat::ErrorMessage);
  }
}

/*!
    Called if the process could not be started.

    By default, adds a message to the output window.
*/

auto AbstractProcessStep::processStartupFailed() -> void
{
  emit addOutput(tr("Could not start process \"%1\" %2.").arg(d->m_param.effectiveCommand().toUserOutput(), d->m_param.prettyArguments()), OutputFormat::ErrorMessage);

  const auto err = d->m_process ? d->m_process->errorString() : QString();
  if (!err.isEmpty()) emit addOutput(err, OutputFormat::ErrorMessage);
}

/*!
    Called to test whether a process succeeded or not.
*/

auto AbstractProcessStep::processSucceeded(int exitCode, QProcess::ExitStatus status) -> bool
{
  if (d->outputFormatter->hasFatalErrors())
    return false;

  return exitCode == 0 && status == QProcess::NormalExit;
}

auto AbstractProcessStep::processReadyReadStdOutput() -> void
{
  QTC_ASSERT(d->m_process.get(), return);
  stdOutput(d->stdoutStream->toUnicode(d->m_process->readAllStandardOutput()));
}

/*!
    Called for each line of output on stdOut().

    The default implementation adds the line to the application output window.
*/

auto AbstractProcessStep::stdOutput(const QString &output) -> void
{
  emit addOutput(output, OutputFormat::Stdout, DontAppendNewline);
}

auto AbstractProcessStep::processReadyReadStdError() -> void
{
  QTC_ASSERT(d->m_process.get(), return);
  stdError(d->stderrStream->toUnicode(d->m_process->readAllStandardError()));
}

/*!
    Called for each line of output on StdErrror().

    The default implementation adds the line to the application output window.
*/

auto AbstractProcessStep::stdError(const QString &output) -> void
{
  emit addOutput(output, OutputFormat::Stderr, DontAppendNewline);
}

auto AbstractProcessStep::finish(bool success) -> void
{
  emit finished(success);
}

auto AbstractProcessStep::slotProcessFinished() -> void
{
  QTC_ASSERT(d->m_process.get(), return);
  stdError(d->stderrStream->toUnicode(d->m_process->readAllStandardError()));
  stdOutput(d->stdoutStream->toUnicode(d->m_process->readAllStandardOutput()));
  d->cleanUp(d->m_process->exitCode(), d->m_process->exitStatus());
}

} // namespace ProjectExplorer
