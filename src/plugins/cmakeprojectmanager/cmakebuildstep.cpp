// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakebuildstep.hpp"

#include "cmakebuildconfiguration.hpp"
#include "cmakebuildsystem.hpp"
#include "cmakekitinformation.hpp"
#include "cmakeparser.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmaketool.hpp"

#include <core/core-item-view-find.hpp>
#include <projectexplorer/buildsteplist.hpp>
#include <projectexplorer/gnumakeparser.hpp>
#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/processparameters.hpp>
#include <projectexplorer/project.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/runconfiguration.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/xcodebuildparser.hpp>

#include <utils/algorithm.hpp>
#include <utils/layoutbuilder.hpp>

#include <QBoxLayout>
#include <QListWidget>
#include <QRegularExpression>
#include <QTreeView>

using namespace Orca::Plugin::Core;
using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

constexpr char BUILD_TARGETS_KEY[] = "CMakeProjectManager.MakeStep.BuildTargets";
constexpr char CMAKE_ARGUMENTS_KEY[] = "CMakeProjectManager.MakeStep.CMakeArguments";
constexpr char TOOL_ARGUMENTS_KEY[] = "CMakeProjectManager.MakeStep.AdditionalArguments";

// CmakeProgressParser

class CmakeProgressParser : public Utils::OutputLineParser {
  Q_OBJECT

signals:
  auto progress(int percentage) -> void;

private:
  auto handleLine(const QString &line, Utils::OutputFormat format) -> Result override
  {
    if (format != Utils::StdOutFormat)
      return Status::NotHandled;

    static const QRegularExpression percentProgress("^\\[\\s*(\\d*)%\\]");
    static const QRegularExpression ninjaProgress("^\\[\\s*(\\d*)/\\s*(\\d*)");

    auto match = percentProgress.match(line);
    if (match.hasMatch()) {
      auto ok = false;
      const auto percent = match.captured(1).toInt(&ok);
      if (ok) emit progress(percent);
      return Status::Done;
    }
    match = ninjaProgress.match(line);
    if (match.hasMatch()) {
      m_useNinja = true;
      auto ok = false;
      const auto done = match.captured(1).toInt(&ok);
      if (ok) {
        const auto all = match.captured(2).toInt(&ok);
        if (ok && all != 0) {
          const auto percent = static_cast<int>(100.0 * done / all);
          emit progress(percent);
        }
      }
      return Status::Done;
    }
    return Status::NotHandled;
  }

  auto hasDetectedRedirection() const -> bool override { return m_useNinja; }

  // TODO: Shouldn't we know the backend in advance? Then we could merge this class
  //       with CmakeParser.
  bool m_useNinja = false;
};

// CmakeTargetItem

CMakeTargetItem::CMakeTargetItem(const QString &target, CMakeBuildStep *step, bool special) : m_target(target), m_step(step), m_special(special) {}

auto CMakeTargetItem::data(int column, int role) const -> QVariant
{
  if (column == 0) {
    if (role == Qt::DisplayRole) {
      if (m_target.isEmpty())
        return CMakeBuildStep::tr("Current executable");
      return m_target;
    }

    if (role == Qt::ToolTipRole) {
      if (m_target.isEmpty()) {
        return CMakeBuildStep::tr("Build the executable used in the active run " "configuration. Currently: %1").arg(m_step->activeRunConfigTarget());
      }
      return CMakeBuildStep::tr("Target: %1").arg(m_target);
    }

    if (role == Qt::CheckStateRole)
      return m_step->buildsBuildTarget(m_target) ? Qt::Checked : Qt::Unchecked;

    if (role == Qt::FontRole) {
      if (m_special) {
        QFont italics;
        italics.setItalic(true);
        return italics;
      }
    }
  }

  return QVariant();
}

auto CMakeTargetItem::setData(int column, const QVariant &data, int role) -> bool
{
  if (column == 0 && role == Qt::CheckStateRole) {
    m_step->setBuildsBuildTarget(m_target, data.value<Qt::CheckState>() == Qt::Checked);
    return true;
  }
  return TreeItem::setData(column, data, role);
}

auto CMakeTargetItem::flags(int) const -> Qt::ItemFlags
{
  return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// CMakeBuildStep

CMakeBuildStep::CMakeBuildStep(BuildStepList *bsl, Utils::Id id) : AbstractProcessStep(bsl, id)
{
  m_cmakeArguments = addAspect<StringAspect>();
  m_cmakeArguments->setSettingsKey(CMAKE_ARGUMENTS_KEY);
  m_cmakeArguments->setLabelText(tr("CMake arguments:"));
  m_cmakeArguments->setDisplayStyle(StringAspect::LineEditDisplay);

  m_toolArguments = addAspect<StringAspect>();
  m_toolArguments->setSettingsKey(TOOL_ARGUMENTS_KEY);
  m_toolArguments->setLabelText(tr("Tool arguments:"));
  m_toolArguments->setDisplayStyle(StringAspect::LineEditDisplay);

  m_buildTargetModel.setHeader({tr("Target")});

  setBuildTargets({defaultBuildTarget()});
  auto *bs = qobject_cast<CMakeBuildSystem*>(buildSystem());
  if (bs && !bs->buildTargets().isEmpty())
    recreateBuildTargetsModel();

  setLowPriority();

  setCommandLineProvider([this] { return cmakeCommand(); });

  setEnvironmentModifier([](Environment &env) {
    const QString ninjaProgressString = "[%f/%t "; // ninja: [33/100
    env.setupEnglishOutput();
    if (!env.expandedValueForKey("NINJA_STATUS").startsWith(ninjaProgressString))
      env.set("NINJA_STATUS", ninjaProgressString + "%o/sec] ");
  });

  connect(target(), &Target::parsingFinished, this, [this](bool success) {
    if (success) // Do not change when parsing failed.
      recreateBuildTargetsModel();
  });

  connect(target(), &Target::activeRunConfigurationChanged, this, &CMakeBuildStep::updateBuildTargetsModel);
}

auto CMakeBuildStep::toMap() const -> QVariantMap
{
  auto map(AbstractProcessStep::toMap());
  map.insert(BUILD_TARGETS_KEY, m_buildTargets);
  return map;
}

auto CMakeBuildStep::fromMap(const QVariantMap &map) -> bool
{
  setBuildTargets(map.value(BUILD_TARGETS_KEY).toStringList());
  return BuildStep::fromMap(map);
}

auto CMakeBuildStep::init() -> bool
{
  if (!AbstractProcessStep::init())
    return false;

  auto bc = buildConfiguration();
  QTC_ASSERT(bc, return false);

  if (!bc->isEnabled()) {
    emit addTask(BuildSystemTask(Task::Error, tr("The build configuration is currently disabled.")));
    emitFaultyConfigurationMessage();
    return false;
  }

  auto tool = CMakeKitAspect::cmakeTool(kit());
  if (!tool || !tool->isValid()) {
    emit addTask(BuildSystemTask(Task::Error, tr("A CMake tool must be set up for building. " "Configure a CMake tool in the kit options.")));
    emitFaultyConfigurationMessage();
    return false;
  }

  if (m_buildTargets.contains(QString())) {
    auto rc = target()->activeRunConfiguration();
    if (!rc || rc->buildKey().isEmpty()) {
      emit addTask(BuildSystemTask(Task::Error, QCoreApplication::translate("ProjectExplorer::Task", "You asked to build the current Run Configuration's build target only, " "but it is not associated with a build target. " "Update the Make Step in your build settings.")));
      emitFaultyConfigurationMessage();
      return false;
    }
  }

  // Warn if doing out-of-source builds with a CMakeCache.txt is the source directory
  const auto projectDirectory = bc->target()->project()->projectDirectory();
  if (bc->buildDirectory() != projectDirectory) {
    if (projectDirectory.pathAppended("CMakeCache.txt").exists()) {
      emit addTask(BuildSystemTask(Task::Warning, tr("There is a CMakeCache.txt file in \"%1\", which suggest an " "in-source build was done before. You are now building in \"%2\", " "and the CMakeCache.txt file might confuse CMake.").arg(projectDirectory.toUserOutput(), bc->buildDirectory().toUserOutput())));
    }
  }

  setIgnoreReturnValue(m_buildTargets == QStringList(CMakeBuildStep::cleanTarget()));

  return true;
}

auto CMakeBuildStep::setupOutputFormatter(Utils::OutputFormatter *formatter) -> void
{
  auto cmakeParser = new CMakeParser;
  const auto progressParser = new CmakeProgressParser;
  connect(progressParser, &CmakeProgressParser::progress, this, [this](int percent) {
    emit progress(percent, {});
  });
  formatter->addLineParser(progressParser);
  cmakeParser->setSourceDirectory(project()->projectDirectory().toString());
  formatter->addLineParsers({cmakeParser, new GnuMakeParser});
  auto tc = ToolChainKitAspect::cxxToolChain(kit());
  OutputTaskParser *xcodeBuildParser = nullptr;
  if (tc && tc->targetAbi().os() == Abi::DarwinOS) {
    xcodeBuildParser = new XcodebuildParser;
    formatter->addLineParser(xcodeBuildParser);
    progressParser->setRedirectionDetector(xcodeBuildParser);
  }
  const auto additionalParsers = kit()->createOutputParsers();
  for (const auto p : additionalParsers)
    p->setRedirectionDetector(progressParser);
  formatter->addLineParsers(additionalParsers);
  formatter->addSearchDir(processParameters()->effectiveWorkingDirectory());
  AbstractProcessStep::setupOutputFormatter(formatter);
}

auto CMakeBuildStep::doRun() -> void
{
  // Make sure CMake state was written to disk before trying to build:
  m_waiting = false;
  auto bs = static_cast<CMakeBuildSystem*>(buildSystem());
  if (bs->persistCMakeState()) {
    emit addOutput(tr("Persisting CMake state..."), BuildStep::OutputFormat::NormalMessage);
    m_waiting = true;
  } else if (buildSystem()->isWaitingForParse()) {
    emit addOutput(tr("Running CMake in preparation to build..."), BuildStep::OutputFormat::NormalMessage);
    m_waiting = true;
  }

  if (m_waiting) {
    m_runTrigger = connect(target(), &Target::parsingFinished, this, [this](bool success) { handleProjectWasParsed(success); });
  } else {
    runImpl();
  }
}

auto CMakeBuildStep::runImpl() -> void
{
  // Do the actual build:
  AbstractProcessStep::doRun();
}

auto CMakeBuildStep::handleProjectWasParsed(bool success) -> void
{
  m_waiting = false;
  disconnect(m_runTrigger);
  if (isCanceled()) {
    emit finished(false);
  } else if (success) {
    runImpl();
  } else {
    AbstractProcessStep::stdError(tr("Project did not parse successfully, cannot build."));
    emit finished(false);
  }
}

auto CMakeBuildStep::defaultBuildTarget() const -> QString
{
  const BuildStepList *const bsl = stepList();
  QTC_ASSERT(bsl, return {});
  const auto parentId = bsl->id();
  if (parentId == ProjectExplorer::Constants::BUILDSTEPS_CLEAN)
    return cleanTarget();
  if (parentId == ProjectExplorer::Constants::BUILDSTEPS_DEPLOY)
    return installTarget();
  return allTarget();
}

auto CMakeBuildStep::buildTargets() const -> QStringList
{
  return m_buildTargets;
}

auto CMakeBuildStep::buildsBuildTarget(const QString &target) const -> bool
{
  return m_buildTargets.contains(target);
}

auto CMakeBuildStep::setBuildsBuildTarget(const QString &target, bool on) -> void
{
  auto targets = m_buildTargets;
  if (on && !m_buildTargets.contains(target))
    targets.append(target);
  if (!on)
    targets.removeAll(target);
  setBuildTargets(targets);
}

auto CMakeBuildStep::setBuildTargets(const QStringList &buildTargets) -> void
{
  if (buildTargets.isEmpty())
    m_buildTargets = QStringList(defaultBuildTarget());
  else
    m_buildTargets = buildTargets;
  updateBuildTargetsModel();
}

auto CMakeBuildStep::cmakeCommand() const -> CommandLine
{
  CommandLine cmd;
  if (auto tool = CMakeKitAspect::cmakeTool(kit()))
    cmd.setExecutable(tool->cmakeExecutable());

  FilePath buildDirectory = ".";
  if (buildConfiguration())
    buildDirectory = buildConfiguration()->buildDirectory();

  cmd.addArgs({"--build", buildDirectory.onDevice(cmd.executable()).path()});

  cmd.addArg("--target");
  cmd.addArgs(Utils::transform(m_buildTargets, [this](const QString &s) {
    if (s.isEmpty()) {
      if (auto rc = target()->activeRunConfiguration())
        return rc->buildKey();
    }
    return s;
  }));

  auto bs = qobject_cast<CMakeBuildSystem*>(buildSystem());
  auto bc = qobject_cast<CMakeBuildConfiguration*>(buildConfiguration());
  if (bc && bs && bs->isMultiConfig()) {
    cmd.addArg("--config");
    cmd.addArg(bc->cmakeBuildType());
  }

  if (!m_cmakeArguments->value().isEmpty())
    cmd.addArgs(m_cmakeArguments->value(), CommandLine::Raw);

  if (!m_toolArguments->value().isEmpty()) {
    cmd.addArg("--");
    cmd.addArgs(m_toolArguments->value(), CommandLine::Raw);
  }

  return cmd;
}

auto CMakeBuildStep::cleanTarget() const -> QString
{
  return QString("clean");
}

auto CMakeBuildStep::allTarget() const -> QString
{
  return m_allTarget;
}

auto CMakeBuildStep::installTarget() const -> QString
{
  return m_installTarget;
}

auto CMakeBuildStep::specialTargets(bool allCapsTargets) -> QStringList
{
  if (!allCapsTargets)
    return {"all", "clean", "install", "install/strip", "package", "test"};
  else
    return {"ALL_BUILD", "clean", "INSTALL", "PACKAGE", "RUN_TESTS"};
}

auto CMakeBuildStep::activeRunConfigTarget() const -> QString
{
  auto rc = target()->activeRunConfiguration();
  return rc ? rc->buildKey() : QString();
}

auto CMakeBuildStep::createConfigWidget() -> QWidget*
{
  auto updateDetails = [this] {
    ProcessParameters param;
    setupProcessParameters(&param);
    param.setCommandLine(cmakeCommand());
    setSummaryText(param.summary(displayName()));
  };

  setDisplayName(tr("Build", "ConfigWidget display name."));

  auto buildTargetsView = new QTreeView;
  buildTargetsView->setMinimumHeight(200);
  buildTargetsView->setModel(&m_buildTargetModel);
  buildTargetsView->setRootIsDecorated(false);
  buildTargetsView->setHeaderHidden(true);

  auto frame = ItemViewFind::createSearchableWrapper(buildTargetsView, ItemViewFind::LightColored);

  Layouting::Form builder;
  builder.addRow(m_cmakeArguments);
  builder.addRow(m_toolArguments);
  builder.addRow({new QLabel(tr("Targets:")), frame});
  auto widget = builder.emerge();

  updateDetails();

  connect(m_cmakeArguments, &StringAspect::changed, this, updateDetails);
  connect(m_toolArguments, &StringAspect::changed, this, updateDetails);

  connect(ProjectExplorerPlugin::instance(), &ProjectExplorerPlugin::settingsChanged, this, updateDetails);

  connect(buildConfiguration(), &BuildConfiguration::environmentChanged, this, updateDetails);

  connect(this, &CMakeBuildStep::buildTargetsChanged, widget, updateDetails);

  return widget;
}

auto CMakeBuildStep::recreateBuildTargetsModel() -> void
{
  auto addItem = [this](const QString &target, bool special = false) {
    auto item = new CMakeTargetItem(target, this, special);
    m_buildTargetModel.rootItem()->appendChild(item);
  };

  m_buildTargetModel.clear();

  auto bs = qobject_cast<CMakeBuildSystem*>(buildSystem());
  auto targetList = bs ? bs->buildTargetTitles() : QStringList();

  auto usesAllCapsTargets = bs ? bs->usesAllCapsTargets() : false;
  if (usesAllCapsTargets) {
    m_allTarget = "ALL_BUILD";
    m_installTarget = "INSTALL";

    int idx = m_buildTargets.indexOf(QString("all"));
    if (idx != -1)
      m_buildTargets[idx] = QString("ALL_BUILD");
    idx = m_buildTargets.indexOf(QString("install"));
    if (idx != -1)
      m_buildTargets[idx] = QString("INSTALL");
  }
  targetList.removeDuplicates();

  addItem(QString(), true);

  // Remove the targets that do not exist in the build system
  // This can result when selected targets get renamed
  if (!targetList.empty()) {
    Utils::erase(m_buildTargets, [targetList](const QString &bt) {
      return !bt.isEmpty() /* "current executable" */ && !targetList.contains(bt);
    });
    if (m_buildTargets.empty())
      m_buildTargets.push_back(m_allTarget);
  }

  for (const auto &buildTarget : qAsConst(targetList))
    addItem(buildTarget, specialTargets(usesAllCapsTargets).contains(buildTarget));

  updateBuildTargetsModel();
}

auto CMakeBuildStep::updateBuildTargetsModel() -> void
{
  emit m_buildTargetModel.layoutChanged();
  emit buildTargetsChanged();
}

auto CMakeBuildStep::processFinished(int exitCode, QProcess::ExitStatus status) -> void
{
  AbstractProcessStep::processFinished(exitCode, status);
  emit progress(100, QString());
}

// CMakeBuildStepFactory

CMakeBuildStepFactory::CMakeBuildStepFactory()
{
  registerStep<CMakeBuildStep>(Constants::CMAKE_BUILD_STEP_ID);
  setDisplayName(CMakeBuildStep::tr("CMake Build", "Display name for CMakeProjectManager::CMakeBuildStep id."));
  setSupportedProjectType(Constants::CMAKE_PROJECT_ID);
}

} // Internal
} // CMakeProjectManager

#include <cmakebuildstep.moc>
