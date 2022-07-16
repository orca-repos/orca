// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakemakestep.hpp"

#include "qmakeparser.hpp"
#include "qmakeproject.hpp"
#include "qmakenodes.hpp"
#include "qmakebuildconfiguration.hpp"
#include "qmakeprojectmanagerconstants.hpp"
#include "qmakesettings.hpp"
#include "qmakestep.hpp"

#include <projectexplorer/target.hpp>
#include <projectexplorer/toolchain.hpp>
#include <projectexplorer/buildsteplist.hpp>
#include <projectexplorer/gnumakeparser.hpp>
#include <projectexplorer/processparameters.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/xcodebuildparser.hpp>

#include <utils/qtcprocess.hpp>
#include <utils/variablechooser.hpp>

#include <QDir>
#include <QFileInfo>

using namespace ProjectExplorer;
using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

class QmakeMakeStep : public MakeStep {
  Q_DECLARE_TR_FUNCTIONS(QmakeProjectManager::QmakeMakeStep)

public:
  QmakeMakeStep(BuildStepList *bsl, Id id);

private:
  auto finish(bool success) -> void override;
  auto init() -> bool override;
  auto setupOutputFormatter(OutputFormatter *formatter) -> void override;
  auto doRun() -> void override;
  auto displayArguments() const -> QStringList override;

  bool m_scriptTarget = false;
  FilePath m_makeFileToCheck;
  bool m_unalignedBuildDir;
  bool m_ignoredNonTopLevelBuild = false;
};

QmakeMakeStep::QmakeMakeStep(BuildStepList *bsl, Id id) : MakeStep(bsl, id)
{
  if (bsl->id() == ProjectExplorer::Constants::BUILDSTEPS_CLEAN) {
    setIgnoreReturnValue(true);
    setUserArguments("clean");
  }
  supportDisablingForSubdirs();
}

auto QmakeMakeStep::init() -> bool
{
  // Note: This skips the Makestep::init() level.
  if (!AbstractProcessStep::init())
    return false;

  const auto bc = static_cast<QmakeBuildConfiguration*>(buildConfiguration());

  const auto unmodifiedMake = effectiveMakeCommand(Execution);
  const auto makeExecutable = unmodifiedMake.executable();
  if (makeExecutable.isEmpty()) emit addTask(makeCommandMissingTask());

  if (!bc || makeExecutable.isEmpty()) {
    emitFaultyConfigurationMessage();
    return false;
  }

  // Ignore all but the first make step for a non-top-level build. See QTCREATORBUG-15794.
  m_ignoredNonTopLevelBuild = (bc->fileNodeBuild() || bc->subNodeBuild()) && !enabledForSubDirs();

  auto pp = processParameters();
  pp->setMacroExpander(bc->macroExpander());

  FilePath workingDirectory;
  if (bc->subNodeBuild())
    workingDirectory = bc->qmakeBuildSystem()->buildDir(bc->subNodeBuild()->filePath());
  else
    workingDirectory = bc->buildDirectory();
  pp->setWorkingDirectory(workingDirectory);

  CommandLine makeCmd(makeExecutable);

  auto subProFile = bc->subNodeBuild();
  if (subProFile) {
    auto makefile = subProFile->makefile();
    if (makefile.isEmpty())
      makefile = "Makefile";
    // Use Makefile.Debug and Makefile.Release
    // for file builds, since the rules for that are
    // only in those files.
    if (subProFile->isDebugAndRelease() && bc->fileNodeBuild()) {
      if (buildType() == QmakeBuildConfiguration::Debug)
        makefile += ".Debug";
      else
        makefile += ".Release";
    }

    if (makefile != "Makefile")
      makeCmd.addArgs({"-f", makefile});

    m_makeFileToCheck = workingDirectory / makefile;
  } else {
    auto makefile = bc->makefile();
    if (!makefile.isEmpty()) {
      makeCmd.addArgs({"-f", makefile.path()});
      m_makeFileToCheck = workingDirectory / makefile.path();
    } else {
      m_makeFileToCheck = workingDirectory / "Makefile";
    }
  }

  makeCmd.addArgs(unmodifiedMake.arguments(), CommandLine::Raw);

  if (bc->fileNodeBuild() && subProFile) {
    auto objectsDir = subProFile->objectsDirectory();
    if (objectsDir.isEmpty()) {
      objectsDir = bc->qmakeBuildSystem()->buildDir(subProFile->filePath()).toString();
      if (subProFile->isDebugAndRelease()) {
        if (bc->buildType() == QmakeBuildConfiguration::Debug)
          objectsDir += "/debug";
        else
          objectsDir += "/release";
      }
    }

    if (subProFile->isObjectParallelToSource()) {
      const auto sourceFileDir = bc->fileNodeBuild()->filePath().parentDir();
      const auto proFileDir = subProFile->proFile()->sourceDir().canonicalPath();
      if (!objectsDir.endsWith('/'))
        objectsDir += QLatin1Char('/');
      objectsDir += sourceFileDir.relativeChildPath(proFileDir).toString();
      objectsDir = QDir::cleanPath(objectsDir);
    }

    auto relObjectsDir = QDir(pp->workingDirectory().toString()).relativeFilePath(objectsDir);
    if (relObjectsDir == ".")
      relObjectsDir.clear();
    if (!relObjectsDir.isEmpty())
      relObjectsDir += '/';
    QString objectFile = relObjectsDir + bc->fileNodeBuild()->filePath().baseName() + subProFile->objectExtension();
    makeCmd.addArg(objectFile);
  }

  pp->setEnvironment(makeEnvironment());
  pp->setCommandLine(makeCmd);

  auto rootNode = dynamic_cast<QmakeProFileNode*>(project()->rootProjectNode());
  QTC_ASSERT(rootNode, return false);
  m_scriptTarget = rootNode->projectType() == ProjectType::ScriptTemplate;
  m_unalignedBuildDir = !bc->isBuildDirAtSafeLocation();

  // A user doing "make clean" indicates they want a proper rebuild, so make sure to really
  // execute qmake on the next build.
  if (stepList()->id() == ProjectExplorer::Constants::BUILDSTEPS_CLEAN) {
    const auto qmakeStep = bc->qmakeStep();
    if (qmakeStep)
      qmakeStep->setForced(true);
  }

  return true;
}

auto QmakeMakeStep::setupOutputFormatter(OutputFormatter *formatter) -> void
{
  formatter->addLineParser(new GnuMakeParser());
  auto tc = ToolChainKitAspect::cxxToolChain(kit());
  OutputTaskParser *xcodeBuildParser = nullptr;
  if (tc && tc->targetAbi().os() == Abi::DarwinOS) {
    xcodeBuildParser = new XcodebuildParser;
    formatter->addLineParser(xcodeBuildParser);
  }
  auto additionalParsers = kit()->createOutputParsers();

  // make may cause qmake to be run, add last to make sure it has a low priority.
  additionalParsers << new QMakeParser;

  if (xcodeBuildParser) {
    for (const auto p : qAsConst(additionalParsers))
      p->setRedirectionDetector(xcodeBuildParser);
  }
  formatter->addLineParsers(additionalParsers);
  formatter->addSearchDir(processParameters()->effectiveWorkingDirectory());

  AbstractProcessStep::setupOutputFormatter(formatter);
}

auto QmakeMakeStep::doRun() -> void
{
  if (m_scriptTarget || m_ignoredNonTopLevelBuild) {
    emit finished(true);
    return;
  }

  if (!m_makeFileToCheck.exists()) {
    if (!ignoreReturnValue()) emit addOutput(tr("Cannot find Makefile. Check your build settings."), BuildStep::OutputFormat::NormalMessage);
    const auto success = ignoreReturnValue();
    emit finished(success);
    return;
  }

  AbstractProcessStep::doRun();
}

auto QmakeMakeStep::finish(bool success) -> void
{
  if (!success && !isCanceled() && m_unalignedBuildDir && QmakeSettings::warnAgainstUnalignedBuildDir()) {
    const auto msg = tr("The build directory is not at the same level as the source " "directory, which could be the reason for the build failure.");
    emit addTask(BuildSystemTask(Task::Warning, msg));
  }
  MakeStep::finish(success);
}

auto QmakeMakeStep::displayArguments() const -> QStringList
{
  const auto bc = static_cast<QmakeBuildConfiguration*>(buildConfiguration());
  if (bc && !bc->makefile().isEmpty())
    return {"-f", bc->makefile().path()};
  return {};
}

///
// QmakeMakeStepFactory
///

QmakeMakeStepFactory::QmakeMakeStepFactory()
{
  registerStep<QmakeMakeStep>(Constants::MAKESTEP_BS_ID);
  setSupportedProjectType(Constants::QMAKEPROJECT_ID);
  setDisplayName(MakeStep::defaultDisplayName());
}

} // Internal
} // QmakeProjectManager
