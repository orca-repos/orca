// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "makestep.hpp"

#include "buildconfiguration.hpp"
#include "gnumakeparser.hpp"
#include "kitinformation.hpp"
#include "project.hpp"
#include "processparameters.hpp"
#include "projectexplorer.hpp"
#include "projectexplorerconstants.hpp"
#include "target.hpp"
#include "toolchain.hpp"

#include <utils/aspects.hpp>
#include <utils/environment.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/optional.hpp>
#include <utils/pathchooser.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/utilsicons.hpp>
#include <utils/variablechooser.hpp>

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QThread>

using namespace Core;
using namespace Utils;

constexpr char BUILD_TARGETS_SUFFIX[] = ".BuildTargets";
constexpr char MAKE_ARGUMENTS_SUFFIX[] = ".MakeArguments";
constexpr char MAKE_COMMAND_SUFFIX[] = ".MakeCommand";
constexpr char OVERRIDE_MAKEFLAGS_SUFFIX[] = ".OverrideMakeflags";
constexpr char JOBCOUNT_SUFFIX[] = ".JobCount";
constexpr char MAKEFLAGS[] = "MAKEFLAGS";

namespace ProjectExplorer {

MakeStep::MakeStep(BuildStepList *parent, Id id) : AbstractProcessStep(parent, id)
{
  setLowPriority();

  setCommandLineProvider([this] { return effectiveMakeCommand(Execution); });

  m_makeCommandAspect = addAspect<StringAspect>();
  m_makeCommandAspect->setSettingsKey(id.withSuffix(MAKE_COMMAND_SUFFIX).toString());
  m_makeCommandAspect->setDisplayStyle(StringAspect::PathChooserDisplay);
  m_makeCommandAspect->setExpectedKind(PathChooser::ExistingCommand);
  m_makeCommandAspect->setBaseFileName(PathChooser::homePath());
  m_makeCommandAspect->setHistoryCompleter("PE.MakeCommand.History");

  m_userArgumentsAspect = addAspect<StringAspect>();
  m_userArgumentsAspect->setSettingsKey(id.withSuffix(MAKE_ARGUMENTS_SUFFIX).toString());
  m_userArgumentsAspect->setLabelText(tr("Make arguments:"));
  m_userArgumentsAspect->setDisplayStyle(StringAspect::LineEditDisplay);

  m_userJobCountAspect = addAspect<IntegerAspect>();
  m_userJobCountAspect->setSettingsKey(id.withSuffix(JOBCOUNT_SUFFIX).toString());
  m_userJobCountAspect->setLabel(tr("Parallel jobs:"));
  m_userJobCountAspect->setRange(1, 999);
  m_userJobCountAspect->setValue(defaultJobCount());
  m_userJobCountAspect->setDefaultValue(defaultJobCount());

  const auto text = tr("Override MAKEFLAGS");
  m_overrideMakeflagsAspect = addAspect<BoolAspect>();
  m_overrideMakeflagsAspect->setSettingsKey(id.withSuffix(OVERRIDE_MAKEFLAGS_SUFFIX).toString());
  m_overrideMakeflagsAspect->setLabel(text, BoolAspect::LabelPlacement::AtCheckBox);

  m_nonOverrideWarning = addAspect<TextDisplay>();
  m_nonOverrideWarning->setToolTip("<html><body><p>" + tr("<code>MAKEFLAGS</code> specifies parallel jobs. Check \"%1\" to override.").arg(text) + "</p></body></html>");
  m_nonOverrideWarning->setIconType(InfoLabel::Warning);

  m_disabledForSubdirsAspect = addAspect<BoolAspect>();
  m_disabledForSubdirsAspect->setSettingsKey(id.withSuffix(".disabledForSubdirs").toString());
  m_disabledForSubdirsAspect->setLabel(tr("Disable in subdirectories:"));
  m_disabledForSubdirsAspect->setToolTip(tr("Runs this step only for a top-level build."));

  m_buildTargetsAspect = addAspect<MultiSelectionAspect>();
  m_buildTargetsAspect->setSettingsKey(id.withSuffix(BUILD_TARGETS_SUFFIX).toString());
  m_buildTargetsAspect->setLabelText(tr("Targets:"));

  const auto updateMakeLabel = [this] {
    const auto defaultMake = defaultMakeCommand();
    const auto labelText = defaultMake.isEmpty() ? tr("Make:") : tr("Override %1:").arg(defaultMake.toUserOutput());
    m_makeCommandAspect->setLabelText(labelText);
  };

  updateMakeLabel();

  connect(m_makeCommandAspect, &StringAspect::changed, this, updateMakeLabel);
}

auto MakeStep::setSelectedBuildTarget(const QString &buildTarget) -> void
{
  m_buildTargetsAspect->setValue({buildTarget});
}

auto MakeStep::setAvailableBuildTargets(const QStringList &buildTargets) -> void
{
  m_buildTargetsAspect->setAllValues(buildTargets);
}

auto MakeStep::init() -> bool
{
  if (!AbstractProcessStep::init())
    return false;

  const auto make = effectiveMakeCommand(Execution);
  if (make.executable().isEmpty()) emit addTask(makeCommandMissingTask());

  if (make.executable().isEmpty()) {
    emitFaultyConfigurationMessage();
    return false;
  }

  return true;
}

auto MakeStep::setupOutputFormatter(OutputFormatter *formatter) -> void
{
  formatter->addLineParser(new GnuMakeParser());
  formatter->addLineParsers(kit()->createOutputParsers());
  formatter->addSearchDir(processParameters()->effectiveWorkingDirectory());
  AbstractProcessStep::setupOutputFormatter(formatter);
}

auto MakeStep::defaultDisplayName() -> QString
{
  return tr("Make");
}

static auto preferredToolChains(const Kit *kit) -> const QList<ToolChain*>
{
  auto tcs = ToolChainKitAspect::toolChains(kit);
  // prefer CXX, then C, then others
  sort(tcs, [](ToolChain *tcA, ToolChain *tcB) {
    if (tcA->language() == tcB->language())
      return false;
    if (tcA->language() == Constants::CXX_LANGUAGE_ID)
      return true;
    if (tcB->language() == Constants::CXX_LANGUAGE_ID)
      return false;
    if (tcA->language() == Constants::C_LANGUAGE_ID)
      return true;
    return false;
  });
  return tcs;
}

auto MakeStep::defaultMakeCommand() const -> FilePath
{
  const auto env = makeEnvironment();
  for (const ToolChain *tc : preferredToolChains(kit())) {
    auto make = tc->makeCommand(env);
    if (!make.isEmpty())
      return mapFromBuildDeviceToGlobalPath(make);
  }
  return {};
}

auto MakeStep::msgNoMakeCommand() -> QString
{
  return tr("Make command missing. Specify Make command in step configuration.");
}

auto MakeStep::makeCommandMissingTask() -> Task
{
  return BuildSystemTask(Task::Error, msgNoMakeCommand());
}

auto MakeStep::isJobCountSupported() const -> bool
{
  const auto tcs = preferredToolChains(kit());
  const ToolChain *tc = tcs.isEmpty() ? nullptr : tcs.constFirst();
  return tc && tc->isJobCountSupported();
}

auto MakeStep::jobCount() const -> int
{
  return m_userJobCountAspect->value();
}

auto MakeStep::jobCountOverridesMakeflags() const -> bool
{
  return m_overrideMakeflagsAspect->value();
}

static auto argsJobCount(const QString &str) -> optional<int>
{
  const auto args = ProcessArgs::splitArgs(str, HostOsInfo::hostOs());
  const auto argIndex = indexOf(args, [](const QString &arg) { return arg.startsWith("-j"); });
  if (argIndex == -1)
    return nullopt;
  auto arg = args.at(argIndex);
  auto requireNumber = false;
  // -j [4] as separate arguments (or no value)
  if (arg == "-j") {
    if (args.size() <= argIndex + 1)
      return 1000; // unlimited
    arg = args.at(argIndex + 1);
  } else {
    // -j4
    arg = arg.mid(2).trimmed();
    requireNumber = true;
  }
  auto ok = false;
  const auto res = arg.toInt(&ok);
  if (!ok && requireNumber)
    return nullopt;
  return make_optional(ok && res > 0 ? res : 1000);
}

auto MakeStep::makeflagsJobCountMismatch() const -> bool
{
  const auto env = makeEnvironment();
  if (!env.hasKey(MAKEFLAGS))
    return false;
  const auto makeFlagsJobCount = argsJobCount(env.expandedValueForKey(MAKEFLAGS));
  return makeFlagsJobCount.has_value() && *makeFlagsJobCount != m_userJobCountAspect->value();
}

auto MakeStep::enabledForSubDirs() const -> bool
{
  return !m_disabledForSubdirsAspect->value();
}

auto MakeStep::makeflagsContainsJobCount() const -> bool
{
  const auto env = makeEnvironment();
  if (!env.hasKey(MAKEFLAGS))
    return false;
  return argsJobCount(env.expandedValueForKey(MAKEFLAGS)).has_value();
}

auto MakeStep::userArgsContainsJobCount() const -> bool
{
  return argsJobCount(userArguments()).has_value();
}

auto MakeStep::makeEnvironment() const -> Environment
{
  auto env = buildEnvironment();
  env.setupEnglishOutput();
  if (makeCommand().isEmpty()) {
    // We also prepend "L" to the MAKEFLAGS, so that nmake / jom are less verbose
    const auto tcs = preferredToolChains(target()->kit());
    const ToolChain *tc = tcs.isEmpty() ? nullptr : tcs.constFirst();
    if (tc && tc->targetAbi().os() == Abi::WindowsOS && tc->targetAbi().osFlavor() != Abi::WindowsMSysFlavor) {
      env.set(MAKEFLAGS, 'L' + env.expandedValueForKey(MAKEFLAGS));
    }
  }
  return env;
}

auto MakeStep::setMakeCommand(const FilePath &command) -> void
{
  m_makeCommandAspect->setFilePath(command);
}

auto MakeStep::defaultJobCount() -> int
{
  return QThread::idealThreadCount();
}

auto MakeStep::jobArguments() const -> QStringList
{
  if (!isJobCountSupported() || userArgsContainsJobCount() || (makeflagsContainsJobCount() && !jobCountOverridesMakeflags())) {
    return {};
  }
  return {"-j" + QString::number(m_userJobCountAspect->value())};
}

auto MakeStep::userArguments() const -> QString
{
  return m_userArgumentsAspect->value();
}

auto MakeStep::setUserArguments(const QString &args) -> void
{
  m_userArgumentsAspect->setValue(args);
}

auto MakeStep::displayArguments() const -> QStringList
{
  return {};
}

auto MakeStep::makeCommand() const -> FilePath
{
  return m_makeCommandAspect->filePath();
}

auto MakeStep::makeExecutable() const -> FilePath
{
  const auto cmd = makeCommand();
  return cmd.isEmpty() ? defaultMakeCommand() : cmd;
}

auto MakeStep::effectiveMakeCommand(MakeCommandType type) const -> CommandLine
{
  CommandLine cmd(makeExecutable());

  if (type == Display)
    cmd.addArgs(displayArguments());
  cmd.addArgs(userArguments(), CommandLine::Raw);
  cmd.addArgs(jobArguments());
  cmd.addArgs(m_buildTargetsAspect->value());

  return cmd;
}

auto MakeStep::createConfigWidget() -> QWidget*
{
  Layouting::Form builder;
  builder.addRow(m_makeCommandAspect);
  builder.addRow(m_userArgumentsAspect);
  builder.addRow({m_userJobCountAspect, m_overrideMakeflagsAspect, m_nonOverrideWarning});
  if (m_disablingForSubDirsSupported)
    builder.addRow(m_disabledForSubdirsAspect);
  builder.addRow(m_buildTargetsAspect);

  const auto widget = builder.emerge(false);

  VariableChooser::addSupportForChildWidgets(widget, macroExpander());

  setSummaryUpdater([this] {
    const auto make = effectiveMakeCommand(Display);
    if (make.executable().isEmpty())
      return tr("<b>Make:</b> %1").arg(msgNoMakeCommand());

    if (!buildConfiguration())
      return tr("<b>Make:</b> No build configuration.");

    ProcessParameters param;
    param.setMacroExpander(macroExpander());
    param.setWorkingDirectory(buildDirectory());
    param.setCommandLine(make);
    param.setEnvironment(buildEnvironment());

    if (param.commandMissing()) {
      return tr("<b>Make:</b> %1 not found in the environment.").arg(param.command().executable().toUserOutput()); // Override display text
    }

    return param.summaryInWorkdir(displayName());
  });

  auto updateDetails = [this] {
    const auto jobCountVisible = isJobCountSupported();
    m_userJobCountAspect->setVisible(jobCountVisible);
    m_overrideMakeflagsAspect->setVisible(jobCountVisible);

    const auto jobCountEnabled = !userArgsContainsJobCount();
    m_userJobCountAspect->setEnabled(jobCountEnabled);
    m_overrideMakeflagsAspect->setEnabled(jobCountEnabled);
    m_nonOverrideWarning->setVisible(makeflagsJobCountMismatch() && !jobCountOverridesMakeflags());
  };

  updateDetails();

  connect(m_makeCommandAspect, &StringAspect::changed, widget, updateDetails);
  connect(m_userArgumentsAspect, &StringAspect::changed, widget, updateDetails);
  connect(m_userJobCountAspect, &IntegerAspect::changed, widget, updateDetails);
  connect(m_overrideMakeflagsAspect, &BoolAspect::changed, widget, updateDetails);
  connect(m_buildTargetsAspect, &BaseAspect::changed, widget, updateDetails);

  connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::settingsChanged, widget, updateDetails);

  connect(target(), &Target::kitChanged, widget, updateDetails);

  connect(buildConfiguration(), &BuildConfiguration::environmentChanged, widget, updateDetails);
  connect(buildConfiguration(), &BuildConfiguration::buildDirectoryChanged, widget, updateDetails);
  connect(target(), &Target::parsingFinished, widget, updateDetails);

  return widget;
}

auto MakeStep::buildsTarget(const QString &target) const -> bool
{
  return m_buildTargetsAspect->value().contains(target);
}

auto MakeStep::setBuildTarget(const QString &target, bool on) -> void
{
  auto old = m_buildTargetsAspect->value();
  if (on && !old.contains(target))
    old << target;
  else if (!on && old.contains(target))
    old.removeOne(target);

  m_buildTargetsAspect->setValue(old);
}

auto MakeStep::availableTargets() const -> QStringList
{
  return m_buildTargetsAspect->allValues();
}

} // namespace ProjectExplorer
