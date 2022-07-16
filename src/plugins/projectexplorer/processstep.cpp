// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "processstep.hpp"

#include "abstractprocessstep.hpp"
#include "buildconfiguration.hpp"
#include "kit.hpp"
#include "processparameters.hpp"
#include "projectexplorerconstants.hpp"
#include "projectexplorer_export.hpp"
#include "target.hpp"

#include <utils/aspects.hpp>
#include <utils/fileutils.hpp>
#include <utils/outputformatter.hpp>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

constexpr char PROCESS_COMMAND_KEY[] = "ProjectExplorer.ProcessStep.Command";
constexpr char PROCESS_WORKINGDIRECTORY_KEY[] = "ProjectExplorer.ProcessStep.WorkingDirectory";
constexpr char PROCESS_ARGUMENTS_KEY[] = "ProjectExplorer.ProcessStep.Arguments";

class ProcessStep final : public AbstractProcessStep {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::ProcessStep)

public:
  ProcessStep(BuildStepList *bsl, Id id);

  auto setupOutputFormatter(OutputFormatter *formatter) -> void final;
};

ProcessStep::ProcessStep(BuildStepList *bsl, Id id) : AbstractProcessStep(bsl, id)
{
  auto command = addAspect<StringAspect>();
  command->setSettingsKey(PROCESS_COMMAND_KEY);
  command->setDisplayStyle(StringAspect::PathChooserDisplay);
  command->setLabelText(tr("Command:"));
  command->setExpectedKind(PathChooser::Command);
  command->setHistoryCompleter("PE.ProcessStepCommand.History");

  auto arguments = addAspect<StringAspect>();
  arguments->setSettingsKey(PROCESS_ARGUMENTS_KEY);
  arguments->setDisplayStyle(StringAspect::LineEditDisplay);
  arguments->setLabelText(tr("Arguments:"));

  auto workingDirectory = addAspect<StringAspect>();
  workingDirectory->setSettingsKey(PROCESS_WORKINGDIRECTORY_KEY);
  workingDirectory->setValue(Constants::DEFAULT_WORKING_DIR);
  workingDirectory->setDisplayStyle(StringAspect::PathChooserDisplay);
  workingDirectory->setLabelText(tr("Working directory:"));
  workingDirectory->setExpectedKind(PathChooser::Directory);

  setWorkingDirectoryProvider([this, workingDirectory] {
    const auto workingDir = workingDirectory->filePath();
    if (workingDir.isEmpty())
      return FilePath::fromString(fallbackWorkingDirectory());
    return workingDir;
  });

  setCommandLineProvider([command, arguments] {
    return CommandLine{command->filePath(), arguments->value(), CommandLine::Raw};
  });

  setSummaryUpdater([this] {
    auto display = displayName();
    if (display.isEmpty())
      display = tr("Custom Process Step");
    ProcessParameters param;
    setupProcessParameters(&param);
    return param.summary(display);
  });

  addMacroExpander();
}

auto ProcessStep::setupOutputFormatter(OutputFormatter *formatter) -> void
{
  formatter->addLineParsers(kit()->createOutputParsers());
  AbstractProcessStep::setupOutputFormatter(formatter);
}

// ProcessStepFactory

ProcessStepFactory::ProcessStepFactory()
{
  registerStep<ProcessStep>("ProjectExplorer.ProcessStep");
  //: Default ProcessStep display name
  setDisplayName(ProcessStep::tr("Custom Process Step"));
}

} // Internal
} // ProjectExplorer
