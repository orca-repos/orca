// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakestep.hpp"

#include "qmakemakestep.hpp"
#include "qmakebuildconfiguration.hpp"
#include "qmakekitinformation.hpp"
#include "qmakenodes.hpp"
#include "qmakeparser.hpp"
#include "qmakeproject.hpp"
#include "qmakeprojectmanagerconstants.hpp"
#include "qmakesettings.hpp"

#include <constants/android/androidconstants.hpp>

#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/buildsteplist.hpp>
#include <projectexplorer/gnumakeparser.hpp>
#include <projectexplorer/processparameters.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/runconfigurationaspects.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/toolchain.hpp>

#include <core/icore.hpp>
#include <core/icontext.hpp>
#include <qtsupport/qtkitinformation.hpp>
#include <qtsupport/qtversionmanager.hpp>
#include <qtsupport/qtsupportconstants.hpp>

#include <constants/ios/iosconstants.hpp>

#include <utils/algorithm.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/layoutbuilder.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/utilsicons.hpp>
#include <utils/variablechooser.hpp>

#include <QDir>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>

using namespace QtSupport;
using namespace ProjectExplorer;
using namespace Utils;

using namespace QmakeProjectManager::Internal;

namespace QmakeProjectManager {

constexpr char QMAKE_ARGUMENTS_KEY[] = "QtProjectManager.QMakeBuildStep.QMakeArguments";
constexpr char QMAKE_FORCED_KEY[] = "QtProjectManager.QMakeBuildStep.QMakeForced";
constexpr char QMAKE_SELECTED_ABIS_KEY[] = "QtProjectManager.QMakeBuildStep.SelectedAbis";

QMakeStep::QMakeStep(BuildStepList *bsl, Id id) : AbstractProcessStep(bsl, id)
{
  setLowPriority();

  m_buildType = addAspect<SelectionAspect>();
  m_buildType->setDisplayStyle(SelectionAspect::DisplayStyle::ComboBox);
  m_buildType->setDisplayName(tr("qmake build configuration:"));
  m_buildType->addOption(tr("Debug"));
  m_buildType->addOption(tr("Release"));

  m_userArgs = addAspect<ArgumentsAspect>();
  m_userArgs->setSettingsKey(QMAKE_ARGUMENTS_KEY);
  m_userArgs->setLabelText(tr("Additional arguments:"));

  m_effectiveCall = addAspect<StringAspect>();
  m_effectiveCall->setDisplayStyle(StringAspect::TextEditDisplay);
  m_effectiveCall->setLabelText(tr("Effective qmake call:"));
  m_effectiveCall->setReadOnly(true);
  m_effectiveCall->setUndoRedoEnabled(false);
  m_effectiveCall->setEnabled(true);

  auto updateSummary = [this] {
    auto qtVersion = QtKitAspect::qtVersion(target()->kit());
    if (!qtVersion)
      return tr("<b>qmake:</b> No Qt version set. Cannot run qmake.");
    const auto program = qtVersion->qmakeFilePath().fileName();
    return tr("<b>qmake:</b> %1 %2").arg(program, project()->projectFilePath().fileName());
  };
  setSummaryUpdater(updateSummary);

  connect(target(), &Target::kitChanged, this, updateSummary);
}

auto QMakeStep::qmakeBuildConfiguration() const -> QmakeBuildConfiguration*
{
  return qobject_cast<QmakeBuildConfiguration*>(buildConfiguration());
}

auto QMakeStep::qmakeBuildSystem() const -> QmakeBuildSystem*
{
  return qmakeBuildConfiguration()->qmakeBuildSystem();
}

///
/// Returns all arguments
/// That is: possbile subpath
/// spec
/// config arguemnts
/// moreArguments
/// user arguments
auto QMakeStep::allArguments(const QtVersion *v, ArgumentFlags flags) const -> QString
{
  QTC_ASSERT(v, return QString());
  auto bc = qmakeBuildConfiguration();
  QStringList arguments;
  if (bc->subNodeBuild())
    arguments << bc->subNodeBuild()->filePath().toUserOutput();
  else if (flags & ArgumentFlag::OmitProjectPath)
    arguments << project()->projectFilePath().fileName();
  else
    arguments << project()->projectFilePath().toUserOutput();

  if (v->qtVersion() < QtVersionNumber(5, 0, 0))
    arguments << "-r";
  auto userProvidedMkspec = false;
  for (ProcessArgs::ConstArgIterator ait(userArguments()); ait.next();) {
    if (ait.value() == "-spec") {
      if (ait.next()) {
        userProvidedMkspec = true;
        break;
      }
    }
  }
  const auto specArg = mkspec();
  if (!userProvidedMkspec && !specArg.isEmpty())
    arguments << "-spec" << QDir::toNativeSeparators(specArg);

  // Find out what flags we pass on to qmake
  arguments << bc->configCommandLineArguments();

  arguments << deducedArguments().toArguments();

  auto args = ProcessArgs::joinArgs(arguments);
  // User arguments
  ProcessArgs::addArgs(&args, userArguments());
  for (auto arg : qAsConst(m_extraArgs))
    ProcessArgs::addArgs(&args, arg);
  return (flags & ArgumentFlag::Expand) ? bc->macroExpander()->expand(args) : args;
}

auto QMakeStep::deducedArguments() const -> QMakeStepConfig
{
  auto kit = target()->kit();
  QMakeStepConfig config;
  Abi targetAbi;
  if (auto tc = ToolChainKitAspect::cxxToolChain(kit)) {
    targetAbi = tc->targetAbi();
    if (HostOsInfo::isWindowsHost() && tc->typeId() == ProjectExplorer::Constants::CLANG_TOOLCHAIN_TYPEID) {
      config.sysRoot = SysRootKitAspect::sysRoot(kit).toString();
      config.targetTriple = tc->originalTargetTriple();
    }
  }

  auto version = QtKitAspect::qtVersion(kit);

  config.osType = QMakeStepConfig::osTypeFor(targetAbi, version);
  config.separateDebugInfo = qmakeBuildConfiguration()->separateDebugInfo();
  config.linkQmlDebuggingQQ2 = qmakeBuildConfiguration()->qmlDebugging();
  config.useQtQuickCompiler = qmakeBuildConfiguration()->useQtQuickCompiler();

  return config;
}

auto QMakeStep::init() -> bool
{
  if (!AbstractProcessStep::init())
    return false;

  m_wasSuccess = true;
  auto qmakeBc = qmakeBuildConfiguration();
  const QtVersion *qtVersion = QtKitAspect::qtVersion(kit());

  if (!qtVersion) {
    emit addOutput(tr("No Qt version configured."), BuildStep::OutputFormat::ErrorMessage);
    return false;
  }

  FilePath workingDirectory;

  if (qmakeBc->subNodeBuild())
    workingDirectory = qmakeBc->qmakeBuildSystem()->buildDir(qmakeBc->subNodeBuild()->filePath());
  else
    workingDirectory = qmakeBc->buildDirectory();

  m_qmakeCommand = CommandLine{qtVersion->qmakeFilePath(), allArguments(qtVersion), CommandLine::Raw};
  m_runMakeQmake = (qtVersion->qtVersion() >= QtVersionNumber(5, 0, 0));

  // The Makefile is used by qmake and make on the build device, from that
  // perspective it is local.

  QString make;
  if (qmakeBc->subNodeBuild()) {
    auto pro = qmakeBc->subNodeBuild();
    if (pro && !pro->makefile().isEmpty())
      make = pro->makefile();
    else
      make = "Makefile";
  } else if (!qmakeBc->makefile().isEmpty()) {
    make = qmakeBc->makefile().path();
  } else {
    make = "Makefile";
  }

  auto makeFile = workingDirectory / make;

  if (m_runMakeQmake) {
    const auto make = makeCommand();
    if (make.isEmpty()) {
      emit addOutput(tr("Could not determine which \"make\" command to run. " "Check the \"make\" step in the build configuration."), BuildStep::OutputFormat::ErrorMessage);
      return false;
    }
    m_makeCommand = CommandLine{make, makeArguments(makeFile.path()), CommandLine::Raw};
  } else {
    m_makeCommand = {};
  }

  // Check whether we need to run qmake
  if (m_forced || QmakeSettings::alwaysRunQmake() || qmakeBc->compareToImportFrom(makeFile) != QmakeBuildConfiguration::MakefileMatches) {
    m_needToRunQMake = true;
  }
  m_forced = false;

  processParameters()->setWorkingDirectory(workingDirectory);

  auto node = static_cast<QmakeProFileNode*>(qmakeBc->project()->rootProjectNode());
  if (qmakeBc->subNodeBuild())
    node = qmakeBc->subNodeBuild();
  QTC_ASSERT(node, return false);
  auto proFile = node->filePath().toString();

  auto tasks = qtVersion->reportIssues(proFile, workingDirectory.toString());
  Utils::sort(tasks);

  if (!tasks.isEmpty()) {
    auto canContinue = true;
    for (const auto &t : qAsConst(tasks)) {
      emit addTask(t);
      if (t.type == Task::Error)
        canContinue = false;
    }
    if (!canContinue) {
      emitFaultyConfigurationMessage();
      return false;
    }
  }

  m_scriptTemplate = node->projectType() == ProjectType::ScriptTemplate;

  return true;
}

auto QMakeStep::setupOutputFormatter(OutputFormatter *formatter) -> void
{
  formatter->addLineParser(new QMakeParser);
  m_outputFormatter = formatter;
  AbstractProcessStep::setupOutputFormatter(formatter);
}

auto QMakeStep::doRun() -> void
{
  if (m_scriptTemplate) {
    emit finished(true);
    return;
  }

  if (!m_needToRunQMake) {
    emit addOutput(tr("Configuration unchanged, skipping qmake step."), BuildStep::OutputFormat::NormalMessage);
    emit finished(true);
    return;
  }

  m_needToRunQMake = false;

  m_nextState = State::RUN_QMAKE;
  runNextCommand();
}

auto QMakeStep::setForced(bool b) -> void
{
  m_forced = b;
}

auto QMakeStep::processStartupFailed() -> void
{
  m_needToRunQMake = true;
  AbstractProcessStep::processStartupFailed();
}

auto QMakeStep::processSucceeded(int exitCode, QProcess::ExitStatus status) -> bool
{
  auto result = AbstractProcessStep::processSucceeded(exitCode, status);
  if (!result)
    m_needToRunQMake = true;
  emit buildConfiguration()->buildDirectoryChanged();
  return result;
}

auto QMakeStep::doCancel() -> void
{
  AbstractProcessStep::doCancel();
}

auto QMakeStep::finish(bool success) -> void
{
  m_wasSuccess = success;
  runNextCommand();
}

auto QMakeStep::startOneCommand(const CommandLine &command) -> void
{
  auto pp = processParameters();
  pp->setCommandLine(command);

  AbstractProcessStep::doRun();
}

auto QMakeStep::runNextCommand() -> void
{
  if (isCanceled())
    m_wasSuccess = false;

  if (!m_wasSuccess)
    m_nextState = State::POST_PROCESS;

  emit progress(static_cast<int>(m_nextState) * 100 / static_cast<int>(State::POST_PROCESS), QString());

  switch (m_nextState) {
  case State::IDLE:
    return;
  case State::RUN_QMAKE:
    m_outputFormatter->setLineParsers({new QMakeParser});
    m_nextState = (m_runMakeQmake ? State::RUN_MAKE_QMAKE_ALL : State::POST_PROCESS);
    startOneCommand(m_qmakeCommand);
    return;
  case State::RUN_MAKE_QMAKE_ALL: {
    auto *parser = new GnuMakeParser;
    parser->addSearchDir(processParameters()->workingDirectory());
    m_outputFormatter->setLineParsers({parser});
    m_nextState = State::POST_PROCESS;
    startOneCommand(m_makeCommand);
  }
    return;
  case State::POST_PROCESS:
    m_nextState = State::IDLE;
    emit finished(m_wasSuccess);
    return;
  }
}

auto QMakeStep::setUserArguments(const QString &arguments) -> void
{
  m_userArgs->setArguments(arguments);
}

auto QMakeStep::extraArguments() const -> QStringList
{
  return m_extraArgs;
}

auto QMakeStep::setExtraArguments(const QStringList &args) -> void
{
  if (m_extraArgs != args) {
    m_extraArgs = args;
    emit qmakeBuildConfiguration()->qmakeBuildConfigurationChanged();
    qmakeBuildSystem()->scheduleUpdateAllNowOrLater();
  }
}

auto QMakeStep::extraParserArguments() const -> QStringList
{
  return m_extraParserArgs;
}

auto QMakeStep::setExtraParserArguments(const QStringList &args) -> void
{
  m_extraParserArgs = args;
}

auto QMakeStep::makeCommand() const -> FilePath
{
  if (auto ms = stepList()->firstOfType<MakeStep>())
    return ms->makeExecutable();
  return FilePath();
}

auto QMakeStep::makeArguments(const QString &makefile) const -> QString
{
  QString args;
  if (!makefile.isEmpty()) {
    ProcessArgs::addArg(&args, "-f");
    ProcessArgs::addArg(&args, makefile);
  }
  ProcessArgs::addArg(&args, "qmake_all");
  return args;
}

auto QMakeStep::effectiveQMakeCall() const -> QString
{
  auto qtVersion = QtKitAspect::qtVersion(kit());
  auto qmake = qtVersion ? qtVersion->qmakeFilePath() : FilePath();
  if (qmake.isEmpty())
    qmake = FilePath::fromString(tr("<no Qt version>"));
  auto make = makeCommand();
  if (make.isEmpty())
    make = FilePath::fromString(tr("<no Make step found>"));

  CommandLine cmd(qmake, {});

  auto result = qmake.toString();
  if (qtVersion) {
    auto qmakeBc = qmakeBuildConfiguration();
    const auto makefile = qmakeBc ? qmakeBc->makefile() : FilePath();
    result += ' ' + allArguments(qtVersion, ArgumentFlag::Expand);
    if (qtVersion->qtVersion() >= QtVersionNumber(5, 0, 0))
      result.append(QString(" && %1 %2").arg(make.path()).arg(makeArguments(makefile.path())));
  }
  return result;
}

auto QMakeStep::parserArguments() -> QStringList
{
  // NOTE: extra parser args placed before the other args intentionally
  auto result = m_extraParserArgs;
  auto qt = QtKitAspect::qtVersion(kit());
  QTC_ASSERT(qt, return QStringList());
  for (ProcessArgs::ConstArgIterator ait(allArguments(qt, ArgumentFlag::Expand)); ait.next();) {
    if (ait.isSimple())
      result << ait.value();
  }
  return result;
}

auto QMakeStep::userArguments() const -> QString
{
  return m_userArgs->arguments(macroExpander());
}

auto QMakeStep::mkspec() const -> QString
{
  auto additionalArguments = userArguments();
  ProcessArgs::addArgs(&additionalArguments, m_extraArgs);
  for (ProcessArgs::ArgIterator ait(&additionalArguments); ait.next();) {
    if (ait.value() == "-spec") {
      if (ait.next())
        return FilePath::fromUserInput(ait.value()).toString();
    }
  }

  return QmakeKitAspect::effectiveMkspec(target()->kit());
}

auto QMakeStep::toMap() const -> QVariantMap
{
  auto map(AbstractProcessStep::toMap());
  map.insert(QMAKE_FORCED_KEY, m_forced);
  map.insert(QMAKE_SELECTED_ABIS_KEY, m_selectedAbis);
  return map;
}

auto QMakeStep::fromMap(const QVariantMap &map) -> bool
{
  m_forced = map.value(QMAKE_FORCED_KEY, false).toBool();
  m_selectedAbis = map.value(QMAKE_SELECTED_ABIS_KEY).toStringList();

  // Backwards compatibility with < Creator 4.12.
  const auto separateDebugInfo = map.value("QtProjectManager.QMakeBuildStep.SeparateDebugInfo");
  if (separateDebugInfo.isValid())
    qmakeBuildConfiguration()->forceSeparateDebugInfo(separateDebugInfo.toBool());
  const auto qmlDebugging = map.value("QtProjectManager.QMakeBuildStep.LinkQmlDebuggingLibrary");
  if (qmlDebugging.isValid())
    qmakeBuildConfiguration()->forceQmlDebugging(qmlDebugging.toBool());
  const auto useQtQuickCompiler = map.value("QtProjectManager.QMakeBuildStep.UseQtQuickCompiler");
  if (useQtQuickCompiler.isValid())
    qmakeBuildConfiguration()->forceQtQuickCompiler(useQtQuickCompiler.toBool());

  return BuildStep::fromMap(map);
}

auto QMakeStep::createConfigWidget() -> QWidget*
{
  abisLabel = new QLabel(tr("ABIs:"));
  abisLabel->setAlignment(Qt::AlignLeading | Qt::AlignLeft | Qt::AlignTop);

  abisListWidget = new QListWidget;

  Layouting::Form builder;
  builder.addRow(m_buildType);
  builder.addRow(m_userArgs);
  builder.addRow(m_effectiveCall);
  builder.addRow({abisLabel, abisListWidget});
  auto widget = builder.emerge(false);

  qmakeBuildConfigChanged();

  emit updateSummary();
  updateAbiWidgets();
  updateEffectiveQMakeCall();

  connect(m_userArgs, &BaseAspect::changed, widget, [this] {
    updateAbiWidgets();
    updateEffectiveQMakeCall();

    emit qmakeBuildConfiguration()->qmakeBuildConfigurationChanged();
    qmakeBuildSystem()->scheduleUpdateAllNowOrLater();
  });

  connect(m_buildType, &BaseAspect::changed, widget, [this] { buildConfigurationSelected(); });

  connect(qmakeBuildConfiguration(), &QmakeBuildConfiguration::qmlDebuggingChanged, widget, [this] {
    linkQmlDebuggingLibraryChanged();
    askForRebuild(tr("QML Debugging"));
  });

  connect(project(), &Project::projectLanguagesUpdated, widget, [this] { linkQmlDebuggingLibraryChanged(); });
  connect(target(), &Target::parsingFinished, widget, [this] { updateEffectiveQMakeCall(); });
  connect(qmakeBuildConfiguration(), &QmakeBuildConfiguration::useQtQuickCompilerChanged, widget, [this] { useQtQuickCompilerChanged(); });
  connect(qmakeBuildConfiguration(), &QmakeBuildConfiguration::separateDebugInfoChanged, widget, [this] { separateDebugInfoChanged(); });
  connect(qmakeBuildConfiguration(), &QmakeBuildConfiguration::qmakeBuildConfigurationChanged, widget, [this] { qmakeBuildConfigChanged(); });
  connect(target(), &Target::kitChanged, widget, [this] { qtVersionChanged(); });

  connect(abisListWidget, &QListWidget::itemChanged, this, [this] {
    abisChanged();
    if (auto bc = qmakeBuildConfiguration())
      BuildManager::buildLists({bc->cleanSteps()});
  });

  VariableChooser::addSupportForChildWidgets(widget, macroExpander());

  return widget;
}

auto QMakeStep::qtVersionChanged() -> void
{
  updateAbiWidgets();
  updateEffectiveQMakeCall();
}

auto QMakeStep::qmakeBuildConfigChanged() -> void
{
  auto bc = qmakeBuildConfiguration();
  bool debug = bc->qmakeBuildConfiguration() & QtVersion::DebugBuild;
  m_ignoreChange = true;
  m_buildType->setValue(debug ? 0 : 1);
  m_ignoreChange = false;
  updateAbiWidgets();
  updateEffectiveQMakeCall();
}

auto QMakeStep::linkQmlDebuggingLibraryChanged() -> void
{
  updateAbiWidgets();
  updateEffectiveQMakeCall();
}

auto QMakeStep::useQtQuickCompilerChanged() -> void
{
  updateAbiWidgets();
  updateEffectiveQMakeCall();
  askForRebuild(tr("Qt Quick Compiler"));
}

auto QMakeStep::separateDebugInfoChanged() -> void
{
  updateAbiWidgets();
  updateEffectiveQMakeCall();
  askForRebuild(tr("Separate Debug Information"));
}

static auto isIos(const Kit *k) -> bool
{
  const auto deviceType = DeviceTypeKitAspect::deviceTypeId(k);
  return deviceType == Ios::Constants::IOS_DEVICE_TYPE || deviceType == Ios::Constants::IOS_SIMULATOR_TYPE;
}

auto QMakeStep::abisChanged() -> void
{
  m_selectedAbis.clear();
  for (auto i = 0; i < abisListWidget->count(); ++i) {
    auto item = abisListWidget->item(i);
    if (item->checkState() == Qt::CheckState::Checked)
      m_selectedAbis << item->text();
  }

  if (auto qtVersion = QtKitAspect::qtVersion(target()->kit())) {
    if (qtVersion->hasAbi(Abi::LinuxOS, Abi::AndroidLinuxFlavor)) {
      const QString prefix = QString("%1=").arg(Android::Constants::ANDROID_ABIS);
      auto args = m_extraArgs;
      for (auto it = args.begin(); it != args.end(); ++it) {
        if (it->startsWith(prefix)) {
          args.erase(it);
          break;
        }
      }
      if (!m_selectedAbis.isEmpty())
        args << prefix + '"' + m_selectedAbis.join(' ') + '"';
      setExtraArguments(args);
      buildSystem()->setProperty(Android::Constants::AndroidAbis, m_selectedAbis);
    } else if (qtVersion->hasAbi(Abi::DarwinOS) && !isIos(target()->kit())) {
      const QString prefix = "QMAKE_APPLE_DEVICE_ARCHS=";
      auto args = m_extraArgs;
      for (auto it = args.begin(); it != args.end(); ++it) {
        if (it->startsWith(prefix)) {
          args.erase(it);
          break;
        }
      }
      QStringList archs;
      for (const auto &selectedAbi : qAsConst(m_selectedAbis)) {
        const auto abi = Abi::abiFromTargetTriplet(selectedAbi);
        if (abi.architecture() == Abi::X86Architecture)
          archs << "x86_64";
        else if (abi.architecture() == Abi::ArmArchitecture)
          archs << "arm64";
      }
      if (!archs.isEmpty())
        args << prefix + '"' + archs.join(' ') + '"';
      setExtraArguments(args);
    }
  }

  updateAbiWidgets();
  updateEffectiveQMakeCall();
}

auto QMakeStep::buildConfigurationSelected() -> void
{
  if (m_ignoreChange)
    return;
  auto bc = qmakeBuildConfiguration();
  auto buildConfiguration = bc->qmakeBuildConfiguration();
  if (m_buildType->value() == 0) {
    // debug
    buildConfiguration = buildConfiguration | QtVersion::DebugBuild;
  } else {
    buildConfiguration = buildConfiguration & ~QtVersion::DebugBuild;
  }
  m_ignoreChange = true;
  bc->setQMakeBuildConfiguration(buildConfiguration);
  m_ignoreChange = false;

  updateAbiWidgets();
  updateEffectiveQMakeCall();
}

auto QMakeStep::askForRebuild(const QString &title) -> void
{
  auto *question = new QMessageBox(Core::ICore::dialogParent());
  question->setWindowTitle(title);
  question->setText(tr("The option will only take effect if the project is recompiled. Do you want to recompile now?"));
  question->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
  question->setModal(true);
  connect(question, &QDialog::finished, this, &QMakeStep::recompileMessageBoxFinished);
  question->show();
}

auto QMakeStep::updateAbiWidgets() -> void
{
  if (!abisLabel)
    return;

  auto qtVersion = QtKitAspect::qtVersion(target()->kit());
  if (!qtVersion)
    return;

  const auto abis = qtVersion->qtAbis();
  const auto enableAbisSelect = abis.size() > 1;
  abisLabel->setVisible(enableAbisSelect);
  abisListWidget->setVisible(enableAbisSelect);

  if (enableAbisSelect && abisListWidget->count() != abis.size()) {
    abisListWidget->clear();
    auto selectedAbis = m_selectedAbis;

    if (selectedAbis.isEmpty()) {
      if (qtVersion->hasAbi(Abi::LinuxOS, Abi::AndroidLinuxFlavor)) {
        // Prefer ARM for Android, prefer 32bit.
        for (const auto &abi : abis) {
          if (abi.param() == ProjectExplorer::Constants::ANDROID_ABI_ARMEABI_V7A)
            selectedAbis.append(abi.param());
        }
        if (selectedAbis.isEmpty()) {
          for (const auto &abi : abis) {
            if (abi.param() == ProjectExplorer::Constants::ANDROID_ABI_ARM64_V8A)
              selectedAbis.append(abi.param());
          }
        }
      } else if (qtVersion->hasAbi(Abi::DarwinOS) && !isIos(target()->kit()) && HostOsInfo::isRunningUnderRosetta()) {
        // Automatically select arm64 when running under Rosetta
        for (const auto &abi : abis) {
          if (abi.architecture() == Abi::ArmArchitecture)
            selectedAbis.append(abi.param());
        }
      }
    }

    for (const auto &abi : abis) {
      const auto param = abi.param();
      auto item = new QListWidgetItem{param, abisListWidget};
      item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
      item->setCheckState(selectedAbis.contains(param) ? Qt::Checked : Qt::Unchecked);
    }
    abisChanged();
  }
}

auto QMakeStep::updateEffectiveQMakeCall() -> void
{
  m_effectiveCall->setValue(effectiveQMakeCall());
}

auto QMakeStep::recompileMessageBoxFinished(int button) -> void
{
  if (button == QMessageBox::Yes) {
    if (auto bc = buildConfiguration())
      BuildManager::buildLists({bc->cleanSteps(), bc->buildSteps()});
  }
}

////
// QMakeStepFactory
////

QMakeStepFactory::QMakeStepFactory()
{
  registerStep<QMakeStep>(Constants::QMAKE_BS_ID);
  setSupportedConfiguration(Constants::QMAKE_BC_ID);
  setSupportedStepList(ProjectExplorer::Constants::BUILDSTEPS_BUILD);
  //: QMakeStep default display name
  setDisplayName(::QmakeProjectManager::QMakeStep::tr("qmake"));
  setFlags(BuildStepInfo::UniqueStep);
}

auto QMakeStepConfig::targetArchFor(const Abi &, const QtVersion *) -> QMakeStepConfig::TargetArchConfig
{
  return NoArch;
}

auto QMakeStepConfig::osTypeFor(const Abi &targetAbi, const QtVersion *version) -> QMakeStepConfig::OsType
{
  auto os = NoOsType;
  const char IOSQT[] = "Qt4ProjectManager.QtVersion.Ios";
  if (!version || version->type() != IOSQT)
    return os;
  if (targetAbi.os() == Abi::DarwinOS && targetAbi.binaryFormat() == Abi::MachOFormat) {
    if (targetAbi.architecture() == Abi::X86Architecture)
      os = IphoneSimulator;
    else if (targetAbi.architecture() == Abi::ArmArchitecture)
      os = IphoneOS;
  }
  return os;
}

auto QMakeStepConfig::toArguments() const -> QStringList
{
  QStringList arguments;

  // TODO: make that depend on the actual Qt version that is used
  if (osType == IphoneSimulator)
    arguments << "CONFIG+=iphonesimulator" << "CONFIG+=simulator" /*since Qt 5.7*/;
  else if (osType == IphoneOS)
    arguments << "CONFIG+=iphoneos" << "CONFIG+=device" /*since Qt 5.7*/;

  if (linkQmlDebuggingQQ2 == TriState::Enabled)
    arguments << "CONFIG+=qml_debug";
  else if (linkQmlDebuggingQQ2 == TriState::Disabled)
    arguments << "CONFIG-=qml_debug";

  if (useQtQuickCompiler == TriState::Enabled)
    arguments << "CONFIG+=qtquickcompiler";
  else if (useQtQuickCompiler == TriState::Disabled)
    arguments << "CONFIG-=qtquickcompiler";

  if (separateDebugInfo == TriState::Enabled)
    arguments << "CONFIG+=force_debug_info" << "CONFIG+=separate_debug_info";
  else if (separateDebugInfo == TriState::Disabled)
    arguments << "CONFIG-=separate_debug_info";

  if (!sysRoot.isEmpty()) {
    arguments << ("QMAKE_CFLAGS+=--sysroot=\"" + sysRoot + "\"");
    arguments << ("QMAKE_CXXFLAGS+=--sysroot=\"" + sysRoot + "\"");
    arguments << ("QMAKE_LFLAGS+=--sysroot=\"" + sysRoot + "\"");
    if (!targetTriple.isEmpty()) {
      arguments << ("QMAKE_CFLAGS+=--target=" + targetTriple);
      arguments << ("QMAKE_CXXFLAGS+=--target=" + targetTriple);
      arguments << ("QMAKE_LFLAGS+=--target=" + targetTriple);
    }
  }

  return arguments;
}

} // QmakeProjectManager
