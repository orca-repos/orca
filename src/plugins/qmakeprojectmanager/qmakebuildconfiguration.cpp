// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakebuildconfiguration.hpp"

#include "qmakebuildinfo.hpp"
#include "qmakekitinformation.hpp"
#include "qmakeproject.hpp"
#include "qmakeprojectmanagerconstants.hpp"
#include "qmakenodes.hpp"
#include "qmakesettings.hpp"
#include "qmakestep.hpp"
#include "makefileparse.hpp"
#include "qmakebuildconfiguration.hpp"

#include <constants/android/androidconstants.hpp>

#include <core/core-document-manager.hpp>
#include <core/core-interface.hpp>

#include <projectexplorer/buildaspects.hpp>
#include <projectexplorer/buildinfo.hpp>
#include <projectexplorer/buildmanager.hpp>
#include <projectexplorer/buildpropertiessettings.hpp>
#include <projectexplorer/buildsteplist.hpp>
#include <projectexplorer/kit.hpp>
#include <projectexplorer/makestep.hpp>
#include <projectexplorer/projectexplorer.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/target.hpp>
#include <projectexplorer/toolchain.hpp>

#include <qtsupport/qtbuildaspects.hpp>
#include <qtsupport/qtkitinformation.hpp>
#include <qtsupport/qtversionmanager.hpp>

#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>
#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QInputDialog>
#include <QLoggingCategory>

#include <limits>

using namespace ProjectExplorer;
using namespace QtSupport;
using namespace Utils;
using namespace QmakeProjectManager::Internal;

namespace QmakeProjectManager {

class RunSystemAspect : public TriStateAspect {
  Q_OBJECT

public:
  RunSystemAspect() : TriStateAspect(tr("Run"), tr("Ignore"), tr("Use global setting"))
  {
    setSettingsKey("RunSystemFunction");
    setDisplayName(tr("qmake system() behavior when parsing:"));
  }
};

QmakeExtraBuildInfo::QmakeExtraBuildInfo()
{
  const auto &settings = ProjectExplorerPlugin::buildPropertiesSettings();
  config.separateDebugInfo = settings.separateDebugInfo.value();
  config.linkQmlDebuggingQQ2 = settings.qmlDebugging.value();
  config.useQtQuickCompiler = settings.qtQuickCompiler.value();
}

// --------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------

auto QmakeBuildConfiguration::shadowBuildDirectory(const FilePath &proFilePath, const Kit *k, const QString &suffix, BuildConfiguration::BuildType buildType) -> FilePath
{
  if (proFilePath.isEmpty())
    return {};

  const auto projectName = proFilePath.completeBaseName();
  return BuildConfiguration::buildDirectoryFromTemplate(Project::projectDirectory(proFilePath), proFilePath, projectName, k, suffix, buildType);
}

const char BUILD_CONFIGURATION_KEY[] = "Qt4ProjectManager.Qt4BuildConfiguration.BuildConfiguration";

QmakeBuildConfiguration::QmakeBuildConfiguration(Target *target, Utils::Id id) : BuildConfiguration(target, id)
{
  setConfigWidgetDisplayName(tr("General"));
  setConfigWidgetHasFrame(true);

  m_buildSystem = new QmakeBuildSystem(this);

  appendInitialBuildStep(Constants::QMAKE_BS_ID);
  appendInitialBuildStep(Constants::MAKESTEP_BS_ID);
  appendInitialCleanStep(Constants::MAKESTEP_BS_ID);

  setInitializer([this, target](const BuildInfo &info) {
    auto qmakeStep = buildSteps()->firstOfType<QMakeStep>();
    QTC_ASSERT(qmakeStep, return);

    const auto qmakeExtra = info.extraInfo.value<QmakeExtraBuildInfo>();
    auto version = QtKitAspect::qtVersion(target->kit());

    auto config = version->defaultBuildConfig();
    if (info.buildType == BuildConfiguration::Debug)
      config |= QtVersion::DebugBuild;
    else
      config &= ~QtVersion::DebugBuild;

    auto additionalArguments = qmakeExtra.additionalArguments;
    if (!additionalArguments.isEmpty())
      qmakeStep->setUserArguments(additionalArguments);

    aspect<SeparateDebugInfoAspect>()->setValue(qmakeExtra.config.separateDebugInfo);
    aspect<QmlDebuggingAspect>()->setValue(qmakeExtra.config.linkQmlDebuggingQQ2);
    aspect<QtQuickCompilerAspect>()->setValue(qmakeExtra.config.useQtQuickCompiler);

    setQMakeBuildConfiguration(config);

    auto directory = info.buildDirectory;
    if (directory.isEmpty()) {
      directory = shadowBuildDirectory(target->project()->projectFilePath(), target->kit(), info.displayName, info.buildType);
    }

    setBuildDirectory(directory);

    if (DeviceTypeKitAspect::deviceTypeId(target->kit()) == Android::Constants::ANDROID_DEVICE_TYPE) {
      buildSteps()->appendStep(Android::Constants::ANDROID_PACKAGE_INSTALL_STEP_ID);
      buildSteps()->appendStep(Android::Constants::ANDROID_BUILD_APK_ID);
    }

    updateCacheAndEmitEnvironmentChanged();
  });

  connect(target, &Target::kitChanged, this, &QmakeBuildConfiguration::kitChanged);
  auto expander = macroExpander();
  expander->registerVariable("Qmake:Makefile", "Qmake makefile", [this]() -> QString {
    const auto file = makefile();
    if (!file.isEmpty())
      return file.path();
    return QLatin1String("Makefile");
  });

  buildDirectoryAspect()->allowInSourceBuilds(target->project()->projectDirectory());
  connect(this, &BuildConfiguration::buildDirectoryChanged, this, &QmakeBuildConfiguration::updateProblemLabel);
  connect(this, &QmakeBuildConfiguration::qmakeBuildConfigurationChanged, this, &QmakeBuildConfiguration::updateProblemLabel);
  connect(&QmakeSettings::instance(), &QmakeSettings::settingsChanged, this, &QmakeBuildConfiguration::updateProblemLabel);
  connect(target, &Target::parsingFinished, this, &QmakeBuildConfiguration::updateProblemLabel);
  connect(target, &Target::kitChanged, this, &QmakeBuildConfiguration::updateProblemLabel);

  const auto separateDebugInfoAspect = addAspect<SeparateDebugInfoAspect>();
  connect(separateDebugInfoAspect, &SeparateDebugInfoAspect::changed, this, [this] {
    emit separateDebugInfoChanged();
    emit qmakeBuildConfigurationChanged();
    qmakeBuildSystem()->scheduleUpdateAllNowOrLater();
  });

  const auto qmlDebuggingAspect = addAspect<QmlDebuggingAspect>();
  qmlDebuggingAspect->setKit(target->kit());
  connect(qmlDebuggingAspect, &QmlDebuggingAspect::changed, this, [this] {
    emit qmlDebuggingChanged();
    emit qmakeBuildConfigurationChanged();
    qmakeBuildSystem()->scheduleUpdateAllNowOrLater();
  });

  const auto qtQuickCompilerAspect = addAspect<QtQuickCompilerAspect>();
  qtQuickCompilerAspect->setKit(target->kit());
  connect(qtQuickCompilerAspect, &QtQuickCompilerAspect::changed, this, [this] {
    emit useQtQuickCompilerChanged();
    emit qmakeBuildConfigurationChanged();
    qmakeBuildSystem()->scheduleUpdateAllNowOrLater();
  });

  addAspect<RunSystemAspect>();
}

QmakeBuildConfiguration::~QmakeBuildConfiguration()
{
  delete m_buildSystem;
}

auto QmakeBuildConfiguration::toMap() const -> QVariantMap
{
  auto map(BuildConfiguration::toMap());
  map.insert(QLatin1String(BUILD_CONFIGURATION_KEY), int(m_qmakeBuildConfiguration));
  return map;
}

auto QmakeBuildConfiguration::fromMap(const QVariantMap &map) -> bool
{
  if (!BuildConfiguration::fromMap(map))
    return false;

  m_qmakeBuildConfiguration = QtVersion::QmakeBuildConfigs(map.value(QLatin1String(BUILD_CONFIGURATION_KEY)).toInt());

  m_lastKitState = LastKitState(kit());
  return true;
}

auto QmakeBuildConfiguration::kitChanged() -> void
{
  auto newState = LastKitState(kit());
  if (newState != m_lastKitState) {
    // This only checks if the ids have changed!
    // For that reason the QmakeBuildConfiguration is also connected
    // to the toolchain and qtversion managers
    m_buildSystem->scheduleUpdateAllNowOrLater();
    m_lastKitState = newState;
  }
}

auto QmakeBuildConfiguration::updateProblemLabel() -> void
{
  const auto k = kit();
  const auto proFileName = project()->projectFilePath().toString();

  // Check for Qt version:
  auto version = QtSupport::QtKitAspect::qtVersion(k);
  if (!version) {
    buildDirectoryAspect()->setProblem(tr("This kit cannot build this project since it " "does not define a Qt version."));
    return;
  }

  const auto bs = qmakeBuildSystem();
  if (auto rootProFile = bs->rootProFile()) {
    if (rootProFile->parseInProgress() || !rootProFile->validParse()) {
      buildDirectoryAspect()->setProblem({});
      return;
    }
  }

  auto targetMismatch = false;
  auto incompatibleBuild = false;
  auto allGood = false;
  // we only show if we actually have a qmake and makestep
  QString errorString;
  if (qmakeStep() && makeStep()) {
    const auto makeFile = this->makefile().isEmpty() ? "Makefile" : makefile().path();
    switch (compareToImportFrom(buildDirectory() / makeFile, &errorString)) {
    case QmakeBuildConfiguration::MakefileMatches:
      allGood = true;
      break;
    case QmakeBuildConfiguration::MakefileMissing:
      allGood = true;
      break;
    case QmakeBuildConfiguration::MakefileIncompatible:
      incompatibleBuild = true;
      break;
    case QmakeBuildConfiguration::MakefileForWrongProject:
      targetMismatch = true;
      break;
    }
  }

  const auto unalignedBuildDir = QmakeSettings::warnAgainstUnalignedBuildDir() && !isBuildDirAtSafeLocation();
  if (unalignedBuildDir)
    allGood = false;

  if (allGood) {
    Tasks issues;
    issues = version->reportIssues(proFileName, buildDirectory().toString());
    Utils::sort(issues);

    if (!issues.isEmpty()) {
      QString text = QLatin1String("<nobr>");
      foreach(const ProjectExplorer::Task &task, issues) {
        QString type;
        switch (task.type) {
        case ProjectExplorer::Task::Error:
          type = tr("Error:");
          type += QLatin1Char(' ');
          break;
        case ProjectExplorer::Task::Warning:
          type = tr("Warning:");
          type += QLatin1Char(' ');
          break;
        case ProjectExplorer::Task::Unknown: default:
          break;
        }
        if (!text.endsWith(QLatin1String("br>")))
          text.append(QLatin1String("<br>"));
        text.append(type + task.description());
      }
      buildDirectoryAspect()->setProblem(text);
      return;
    }
  } else if (targetMismatch) {
    buildDirectoryAspect()->setProblem(tr("The build directory contains a build for " "a different project, which will be overwritten."));
    return;
  } else if (incompatibleBuild) {
    buildDirectoryAspect()->setProblem(tr("%1 The build will be overwritten.", "%1 error message").arg(errorString));
    return;
  } else if (unalignedBuildDir) {
    buildDirectoryAspect()->setProblem(unalignedBuildDirWarning());
    return;
  }

  buildDirectoryAspect()->setProblem({});
}

auto QmakeBuildConfiguration::buildSystem() const -> BuildSystem*
{
  return m_buildSystem;
}

/// If only a sub tree should be build this function returns which sub node
/// should be build
/// \see QMakeBuildConfiguration::setSubNodeBuild
auto QmakeBuildConfiguration::subNodeBuild() const -> QmakeProFileNode*
{
  return m_subNodeBuild;
}

/// A sub node build on builds a sub node of the project
/// That is triggered by a right click in the project explorer tree
/// The sub node to be build is set via this function immediately before
/// calling BuildManager::buildProject( BuildConfiguration * )
/// and reset immediately afterwards
/// That is m_subNodesBuild is set only temporarly
auto QmakeBuildConfiguration::setSubNodeBuild(QmakeProFileNode *node) -> void
{
  m_subNodeBuild = node;
}

auto QmakeBuildConfiguration::fileNodeBuild() const -> FileNode*
{
  return m_fileNodeBuild;
}

auto QmakeBuildConfiguration::setFileNodeBuild(FileNode *node) -> void
{
  m_fileNodeBuild = node;
}

auto QmakeBuildConfiguration::makefile() const -> FilePath
{
  return FilePath::fromString(m_buildSystem->rootProFile()->singleVariableValue(Variable::Makefile));
}

auto QmakeBuildConfiguration::qmakeBuildConfiguration() const -> QtVersion::QmakeBuildConfigs
{
  return m_qmakeBuildConfiguration;
}

auto QmakeBuildConfiguration::setQMakeBuildConfiguration(QtVersion::QmakeBuildConfigs config) -> void
{
  if (m_qmakeBuildConfiguration == config)
    return;
  m_qmakeBuildConfiguration = config;

  emit qmakeBuildConfigurationChanged();
  m_buildSystem->scheduleUpdateAllNowOrLater();
  emit buildTypeChanged();
}

auto QmakeBuildConfiguration::unalignedBuildDirWarning() -> QString
{
  return tr("The build directory should be at the same level as the source directory.");
}

auto QmakeBuildConfiguration::isBuildDirAtSafeLocation(const QString &sourceDir, const QString &buildDir) -> bool
{
  return buildDir.count('/') == sourceDir.count('/');
}

auto QmakeBuildConfiguration::isBuildDirAtSafeLocation() const -> bool
{
  return isBuildDirAtSafeLocation(project()->projectDirectory().toString(), buildDirectory().toString());
}

auto QmakeBuildConfiguration::separateDebugInfo() const -> TriState
{
  return aspect<SeparateDebugInfoAspect>()->value();
}

auto QmakeBuildConfiguration::forceSeparateDebugInfo(bool sepDebugInfo) -> void
{
  aspect<SeparateDebugInfoAspect>()->setValue(sepDebugInfo ? TriState::Enabled : TriState::Disabled);
}

auto QmakeBuildConfiguration::qmlDebugging() const -> TriState
{
  return aspect<QmlDebuggingAspect>()->value();
}

auto QmakeBuildConfiguration::forceQmlDebugging(bool enable) -> void
{
  aspect<QmlDebuggingAspect>()->setValue(enable ? TriState::Enabled : TriState::Disabled);
}

auto QmakeBuildConfiguration::useQtQuickCompiler() const -> TriState
{
  return aspect<QtQuickCompilerAspect>()->value();
}

auto QmakeBuildConfiguration::forceQtQuickCompiler(bool enable) -> void
{
  aspect<QtQuickCompilerAspect>()->setValue(enable ? TriState::Enabled : TriState::Disabled);
}

auto QmakeBuildConfiguration::runSystemFunction() const -> bool
{
  const auto runSystem = aspect<RunSystemAspect>()->value();
  if (runSystem == TriState::Enabled)
    return true;
  if (runSystem == TriState::Disabled)
    return false;
  return QmakeSettings::runSystemFunction();
}

auto QmakeBuildConfiguration::configCommandLineArguments() const -> QStringList
{
  QStringList result;
  auto version = QtKitAspect::qtVersion(kit());
  auto defaultBuildConfiguration = version ? version->defaultBuildConfig() : QtVersion::QmakeBuildConfigs(QtVersion::DebugBuild | QtVersion::BuildAll);
  auto userBuildConfiguration = m_qmakeBuildConfiguration;
  if ((defaultBuildConfiguration & QtVersion::BuildAll) && !(userBuildConfiguration & QtVersion::BuildAll))
    result << QLatin1String("CONFIG-=debug_and_release");

  if (!(defaultBuildConfiguration & QtVersion::BuildAll) && (userBuildConfiguration & QtVersion::BuildAll))
    result << QLatin1String("CONFIG+=debug_and_release");
  if ((defaultBuildConfiguration & QtVersion::DebugBuild) && !(userBuildConfiguration & QtVersion::DebugBuild))
    result << QLatin1String("CONFIG+=release");
  if (!(defaultBuildConfiguration & QtVersion::DebugBuild) && (userBuildConfiguration & QtVersion::DebugBuild))
    result << QLatin1String("CONFIG+=debug");
  return result;
}

auto QmakeBuildConfiguration::qmakeStep() const -> QMakeStep*
{
  QMakeStep *qs = nullptr;
  auto bsl = buildSteps();
  for (auto i = 0; i < bsl->count(); ++i)
    if ((qs = qobject_cast<QMakeStep*>(bsl->at(i))) != nullptr)
      return qs;
  return nullptr;
}

auto QmakeBuildConfiguration::makeStep() const -> MakeStep*
{
  MakeStep *ms = nullptr;
  auto bsl = buildSteps();
  for (auto i = 0; i < bsl->count(); ++i)
    if ((ms = qobject_cast<MakeStep*>(bsl->at(i))) != nullptr)
      return ms;
  return nullptr;
}

auto QmakeBuildConfiguration::qmakeBuildSystem() const -> QmakeBuildSystem*
{
  return m_buildSystem;
}

// Returns true if both are equal.
auto QmakeBuildConfiguration::compareToImportFrom(const FilePath &makefile, QString *errorString) -> QmakeBuildConfiguration::MakefileState
{
  const auto &logs = MakeFileParse::logging();
  qCDebug(logs) << "QMakeBuildConfiguration::compareToImport";

  auto qs = qmakeStep();
  MakeFileParse parse(makefile, MakeFileParse::Mode::DoNotFilterKnownConfigValues);

  if (parse.makeFileState() == MakeFileParse::MakefileMissing) {
    qCDebug(logs) << "**Makefile missing";
    return MakefileMissing;
  }
  if (parse.makeFileState() == MakeFileParse::CouldNotParse) {
    qCDebug(logs) << "**Makefile incompatible";
    if (errorString)
      *errorString = tr("Could not parse Makefile.");
    return MakefileIncompatible;
  }

  if (!qs) {
    qCDebug(logs) << "**No qmake step";
    return MakefileMissing;
  }

  auto version = QtKitAspect::qtVersion(kit());
  if (!version) {
    qCDebug(logs) << "**No qt version in kit";
    return MakefileForWrongProject;
  }

  const auto projectPath = m_subNodeBuild ? m_subNodeBuild->filePath() : qs->project()->projectFilePath();
  if (parse.srcProFile() != projectPath) {
    qCDebug(logs) << "**Different profile used to generate the Makefile:" << parse.srcProFile() << " expected profile:" << projectPath;
    if (errorString)
      *errorString = tr("The Makefile is for a different project.");
    return MakefileIncompatible;
  }

  if (version->qmakeFilePath() != parse.qmakePath()) {
    qCDebug(logs) << "**Different Qt versions, buildconfiguration:" << version->qmakeFilePath() << " Makefile:" << parse.qmakePath();
    return MakefileForWrongProject;
  }

  // same qtversion
  auto buildConfig = parse.effectiveBuildConfig(version->defaultBuildConfig());
  if (qmakeBuildConfiguration() != buildConfig) {
    qCDebug(logs) << "**Different qmake buildconfigurations buildconfiguration:" << qmakeBuildConfiguration() << " Makefile:" << buildConfig;
    if (errorString)
      *errorString = tr("The build type has changed.");
    return MakefileIncompatible;
  }

  // The qmake Build Configuration are the same,
  // now compare arguments lists
  // we have to compare without the spec/platform cmd argument
  // and compare that on its own
  auto workingDirectory = makefile.parentDir();
  QStringList actualArgs;
  auto allArgs = macroExpander()->expandProcessArgs(qs->allArguments(QtKitAspect::qtVersion(target()->kit()), QMakeStep::ArgumentFlag::Expand));
  // This copies the settings from allArgs to actualArgs (minus some we
  // are not interested in), splitting them up into individual strings:
  extractSpecFromArguments(&allArgs, workingDirectory, version, &actualArgs);
  actualArgs.removeFirst(); // Project file.
  const auto actualSpec = qs->mkspec();

  auto qmakeArgs = parse.unparsedArguments();
  QStringList parsedArgs;
  auto parsedSpec = extractSpecFromArguments(&qmakeArgs, workingDirectory, version, &parsedArgs);

  qCDebug(logs) << "  Actual args:" << actualArgs;
  qCDebug(logs) << "  Parsed args:" << parsedArgs;
  qCDebug(logs) << "  Actual spec:" << actualSpec;
  qCDebug(logs) << "  Parsed spec:" << parsedSpec;
  qCDebug(logs) << "  Actual config:" << qs->deducedArguments();
  qCDebug(logs) << "  Parsed config:" << parse.config();

  // Comparing the sorted list is obviously wrong
  // Though haven written a more complete version
  // that managed had around 200 lines and yet faild
  // to be actually foolproof at all, I think it's
  // not feasible without actually taking the qmake
  // command line parsing code

  // Things, sorting gets wrong:
  // parameters to positional parameters matter
  //  e.g. -o -spec is different from -spec -o
  //       -o 1 -spec 2 is diffrent from -spec 1 -o 2
  // variable assignment order matters
  // variable assignment vs -after
  // -norecursive vs. recursive
  actualArgs.sort();
  parsedArgs.sort();
  if (actualArgs != parsedArgs) {
    qCDebug(logs) << "**Mismatched args";
    if (errorString)
      *errorString = tr("The qmake arguments have changed.");
    return MakefileIncompatible;
  }

  if (parse.config() != qs->deducedArguments()) {
    qCDebug(logs) << "**Mismatched config";
    if (errorString)
      *errorString = tr("The qmake arguments have changed.");
    return MakefileIncompatible;
  }

  // Specs match exactly
  if (actualSpec == parsedSpec) {
    qCDebug(logs) << "**Matched specs (1)";
    return MakefileMatches;
  }
  // Actual spec is the default one
  //                    qDebug() << "AS vs VS" << actualSpec << version->mkspec();
  if ((actualSpec == version->mkspec() || actualSpec == "default") && (parsedSpec == version->mkspec() || parsedSpec == "default" || parsedSpec.isEmpty())) {
    qCDebug(logs) << "**Matched specs (2)";
    return MakefileMatches;
  }

  qCDebug(logs) << "**Incompatible specs";
  if (errorString)
    *errorString = tr("The mkspec has changed.");
  return MakefileIncompatible;
}

auto QmakeBuildConfiguration::extractSpecFromArguments(QString *args, const FilePath &directory, const QtVersion *version, QStringList *outArgs) -> QString
{
  FilePath parsedSpec;

  auto ignoreNext = false;
  auto nextIsSpec = false;
  for (ProcessArgs::ArgIterator ait(args); ait.next();) {
    if (ignoreNext) {
      ignoreNext = false;
      ait.deleteArg();
    } else if (nextIsSpec) {
      nextIsSpec = false;
      parsedSpec = FilePath::fromUserInput(ait.value());
      ait.deleteArg();
    } else if (ait.value() == QLatin1String("-spec") || ait.value() == QLatin1String("-platform")) {
      nextIsSpec = true;
      ait.deleteArg();
    } else if (ait.value() == QLatin1String("-cache")) {
      // We ignore -cache, because qmake contained a bug that it didn't
      // mention the -cache in the Makefile.
      // That means changing the -cache option in the additional arguments
      // does not automatically rerun qmake. Alas, we could try more
      // intelligent matching for -cache, but i guess people rarely
      // do use that.
      ignoreNext = true;
      ait.deleteArg();
    } else if (outArgs && ait.isSimple()) {
      outArgs->append(ait.value());
    }
  }

  if (parsedSpec.isEmpty())
    return {};

  auto baseMkspecDir = FilePath::fromUserInput(version->hostDataPath().toString() + "/mkspecs");
  baseMkspecDir = FilePath::fromString(baseMkspecDir.toFileInfo().canonicalFilePath());

  // if the path is relative it can be
  // relative to the working directory (as found in the Makefiles)
  // or relatively to the mkspec directory
  // if it is the former we need to get the canonical form
  // for the other one we don't need to do anything
  if (parsedSpec.toFileInfo().isRelative()) {
    if (QFileInfo::exists(directory.path() + QLatin1Char('/') + parsedSpec.toString()))
      parsedSpec = FilePath::fromUserInput(directory.path() + QLatin1Char('/') + parsedSpec.toString());
    else
      parsedSpec = FilePath::fromUserInput(baseMkspecDir.toString() + QLatin1Char('/') + parsedSpec.toString());
  }

  auto f2 = parsedSpec.toFileInfo();
  while (f2.isSymLink()) {
    parsedSpec = FilePath::fromString(f2.symLinkTarget());
    f2.setFile(parsedSpec.toString());
  }

  if (parsedSpec.isChildOf(baseMkspecDir)) {
    parsedSpec = parsedSpec.relativeChildPath(baseMkspecDir);
  } else {
    auto sourceMkSpecPath = FilePath::fromString(version->sourcePath().toString() + QLatin1String("/mkspecs"));
    if (parsedSpec.isChildOf(sourceMkSpecPath))
      parsedSpec = parsedSpec.relativeChildPath(sourceMkSpecPath);
  }
  return parsedSpec.toString();
}

/*!
  \class QmakeBuildConfigurationFactory
*/

static auto createBuildInfo(const Kit *k, const FilePath &projectPath, BuildConfiguration::BuildType type) -> BuildInfo
{
  const auto &settings = ProjectExplorerPlugin::buildPropertiesSettings();
  auto version = QtKitAspect::qtVersion(k);
  QmakeExtraBuildInfo extraInfo;
  BuildInfo info;
  QString suffix;

  if (type == BuildConfiguration::Release) {
    //: The name of the release build configuration created by default for a qmake project.
    info.displayName = BuildConfiguration::tr("Release");
    //: Non-ASCII characters in directory suffix may cause build issues.
    suffix = QmakeBuildConfiguration::tr("Release", "Shadow build directory suffix");
    if (settings.qtQuickCompiler.value() == TriState::Default) {
      if (version && version->isQtQuickCompilerSupported())
        extraInfo.config.useQtQuickCompiler = TriState::Enabled;
    }
  } else {
    if (type == BuildConfiguration::Debug) {
      //: The name of the debug build configuration created by default for a qmake project.
      info.displayName = BuildConfiguration::tr("Debug");
      //: Non-ASCII characters in directory suffix may cause build issues.
      suffix = QmakeBuildConfiguration::tr("Debug", "Shadow build directory suffix");
    } else if (type == BuildConfiguration::Profile) {
      //: The name of the profile build configuration created by default for a qmake project.
      info.displayName = BuildConfiguration::tr("Profile");
      //: Non-ASCII characters in directory suffix may cause build issues.
      suffix = QmakeBuildConfiguration::tr("Profile", "Shadow build directory suffix");
      if (settings.separateDebugInfo.value() == TriState::Default)
        extraInfo.config.separateDebugInfo = TriState::Enabled;

      if (settings.qtQuickCompiler.value() == TriState::Default) {
        if (version && version->isQtQuickCompilerSupported())
          extraInfo.config.useQtQuickCompiler = TriState::Enabled;
      }
    }
    if (settings.qmlDebugging.value() == TriState::Default) {
      if (version && version->isQmlDebuggingSupported())
        extraInfo.config.linkQmlDebuggingQQ2 = TriState::Enabled;
    }
  }
  info.typeName = info.displayName;
  // Leave info.buildDirectory unset;

  // check if this project is in the source directory:
  if (version && version->isInQtSourceDirectory(projectPath)) {
    // assemble build directory
    auto projectDirectory = projectPath.toFileInfo().absolutePath();
    auto qtSourceDir = QDir(version->sourcePath().toString());
    auto relativeProjectPath = qtSourceDir.relativeFilePath(projectDirectory);
    auto qtBuildDir = version->prefix().toString();
    auto absoluteBuildPath = QDir::cleanPath(qtBuildDir + QLatin1Char('/') + relativeProjectPath);

    info.buildDirectory = FilePath::fromString(absoluteBuildPath);
  } else {
    info.buildDirectory = QmakeBuildConfiguration::shadowBuildDirectory(projectPath, k, suffix, type);
  }
  info.buildType = type;
  info.extraInfo = QVariant::fromValue(extraInfo);
  return info;
}

QmakeBuildConfigurationFactory::QmakeBuildConfigurationFactory()
{
  registerBuildConfiguration<QmakeBuildConfiguration>(Constants::QMAKE_BC_ID);
  setSupportedProjectType(Constants::QMAKEPROJECT_ID);
  setSupportedProjectMimeTypeName(Constants::PROFILE_MIMETYPE);
  setIssueReporter([](Kit *k, const QString &projectPath, const QString &buildDir) {
    auto version = QtSupport::QtKitAspect::qtVersion(k);
    Tasks issues;
    if (version)
      issues << version->reportIssues(projectPath, buildDir);
    if (QmakeSettings::warnAgainstUnalignedBuildDir() && !QmakeBuildConfiguration::isBuildDirAtSafeLocation(QFileInfo(projectPath).absoluteDir().path(), QDir(buildDir).absolutePath())) {
      issues.append(BuildSystemTask(Task::Warning, QmakeBuildConfiguration::unalignedBuildDirWarning()));
    }
    return issues;
  });

  setBuildGenerator([](const Kit *k, const FilePath &projectPath, bool forSetup) {
    QList<BuildInfo> result;

    auto qtVersion = QtKitAspect::qtVersion(k);

    if (forSetup && (!qtVersion || !qtVersion->isValid()))
      return result;

    const auto addBuild = [&](BuildConfiguration::BuildType buildType) {
      auto info = createBuildInfo(k, projectPath, buildType);
      if (!forSetup) {
        info.displayName.clear();    // ask for a name
        info.buildDirectory.clear(); // This depends on the displayName
      }
      result << info;
    };

    addBuild(BuildConfiguration::Debug);
    addBuild(BuildConfiguration::Release);
    if (qtVersion && qtVersion->qtVersion().majorVersion > 4)
      addBuild(BuildConfiguration::Profile);

    return result;
  });
}

auto QmakeBuildConfiguration::buildType() const -> BuildConfiguration::BuildType
{
  if (qmakeBuildConfiguration() & QtVersion::DebugBuild)
    return Debug;
  if (separateDebugInfo() == TriState::Enabled)
    return Profile;
  return Release;
}

auto QmakeBuildConfiguration::addToEnvironment(Environment &env) const -> void
{
  QtSupport::QtKitAspect::addHostBinariesToPath(kit(), env);
}

QmakeBuildConfiguration::LastKitState::LastKitState() = default;

QmakeBuildConfiguration::LastKitState::LastKitState(Kit *k) : m_qtVersion(QtKitAspect::qtVersionId(k)), m_sysroot(SysRootKitAspect::sysRoot(k).toString()), m_mkspec(QmakeKitAspect::mkspec(k))
{
  auto tc = ToolChainKitAspect::cxxToolChain(k);
  m_toolchain = tc ? tc->id() : QByteArray();
}

auto QmakeBuildConfiguration::LastKitState::operator ==(const LastKitState &other) const -> bool
{
  return m_qtVersion == other.m_qtVersion && m_toolchain == other.m_toolchain && m_sysroot == other.m_sysroot && m_mkspec == other.m_mkspec;
}

auto QmakeBuildConfiguration::LastKitState::operator !=(const LastKitState &other) const -> bool
{
  return !operator ==(other);
}

auto QmakeBuildConfiguration::regenerateBuildFiles(Node *node) -> bool
{
  auto qs = qmakeStep();
  if (!qs)
    return false;

  qs->setForced(true);

  BuildManager::buildList(cleanSteps());
  BuildManager::appendStep(qs, BuildManager::displayNameForStepId(ProjectExplorer::Constants::BUILDSTEPS_CLEAN));

  QmakeProFileNode *proFile = nullptr;
  if (node && node != project()->rootProjectNode())
    proFile = dynamic_cast<QmakeProFileNode*>(node);

  setSubNodeBuild(proFile);

  return true;
}

auto QmakeBuildConfiguration::restrictNextBuild(const RunConfiguration *rc) -> void
{
  if (!rc) {
    setSubNodeBuild(nullptr);
    return;
  }
  const auto productNode = dynamic_cast<QmakeProFileNode*>(rc->productNode());
  QTC_ASSERT(productNode, return);
  setSubNodeBuild(productNode);
}

} // namespace QmakeProjectManager

#include <qmakebuildconfiguration.moc>
