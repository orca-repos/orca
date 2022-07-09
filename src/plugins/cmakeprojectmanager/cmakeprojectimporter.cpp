// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeprojectimporter.hpp"

#include "cmakebuildconfiguration.hpp"
#include "cmakebuildsystem.hpp"
#include "cmakekitinformation.hpp"
#include "cmakeprojectconstants.hpp"
#include "cmaketoolmanager.hpp"

#include <core/messagemanager.hpp>

#include <projectexplorer/buildinfo.hpp>
#include <projectexplorer/kitinformation.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/toolchainmanager.hpp>

#include <qtsupport/qtkitinformation.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <QDir>
#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace QtSupport;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

static Q_LOGGING_CATEGORY(cmInputLog, "qtc.cmake.import", QtWarningMsg);

struct DirectoryData {
  // Project Stuff:
  QByteArray cmakeBuildType;
  FilePath buildDirectory;
  FilePath cmakeHomeDirectory;

  // Kit Stuff
  FilePath cmakeBinary;
  QString generator;
  QString extraGenerator;
  QString platform;
  QString toolset;
  FilePath sysroot;
  QtProjectImporter::QtVersionData qt;
  QVector<ToolChainDescription> toolChains;
};

static auto scanDirectory(const FilePath &path, const QString &prefix) -> QStringList
{
  QStringList result;
  qCDebug(cmInputLog) << "Scanning for directories matching" << prefix << "in" << path;

  foreach(const FilePath &entry, path.dirEntries({{prefix + "*"}, QDir::Dirs | QDir::NoDotAndDotDot})) {
    QTC_ASSERT(entry.isDir(), continue);
    result.append(entry.toString());
  }
  return result;
}

static auto baseCMakeToolDisplayName(CMakeTool &tool) -> QString
{
  auto version = tool.version();
  return QString("CMake %1.%2.%3").arg(version.major).arg(version.minor).arg(version.patch);
}

static auto uniqueCMakeToolDisplayName(CMakeTool &tool) -> QString
{
  auto baseName = baseCMakeToolDisplayName(tool);

  QStringList existingNames;
  for (const CMakeTool *t : CMakeToolManager::cmakeTools())
    existingNames << t->displayName();
  return Utils::makeUniquelyNumbered(baseName, existingNames);
}

// CMakeProjectImporter

CMakeProjectImporter::CMakeProjectImporter(const FilePath &path) : QtProjectImporter(path)
{
  useTemporaryKitAspect(CMakeKitAspect::id(), [this](Kit *k, const QVariantList &vl) { cleanupTemporaryCMake(k, vl); }, [this](Kit *k, const QVariantList &vl) { persistTemporaryCMake(k, vl); });
}

auto CMakeProjectImporter::importCandidates() -> QStringList
{
  QStringList candidates;

  candidates << scanDirectory(projectFilePath().absolutePath(), "build");

  foreach(Kit *k, KitManager::kits()) {
    auto shadowBuildDirectory = CMakeBuildConfiguration::shadowBuildDirectory(projectFilePath(), k, QString(), BuildConfiguration::Unknown);
    candidates << scanDirectory(shadowBuildDirectory.absolutePath(), QString());
  }
  const auto finalists = Utils::filteredUnique(candidates);
  qCInfo(cmInputLog) << "import candidates:" << finalists;
  return finalists;
}

static auto qmakeFromCMakeCache(const CMakeConfig &config) -> FilePath
{
  // Qt4 way to define things (more convenient for us, so try this first;-)
  const auto qmake = config.filePathValueOf("QT_QMAKE_EXECUTABLE");
  qCDebug(cmInputLog) << "QT_QMAKE_EXECUTABLE=" << qmake.toUserOutput();
  if (!qmake.isEmpty())
    return qmake;

  // Check Qt5 settings: oh, the horror!
  const auto qtCMakeDir = [config] {
    auto tmp = config.filePathValueOf("Qt5Core_DIR");
    if (tmp.isEmpty())
      tmp = config.filePathValueOf("Qt6Core_DIR");
    return tmp;
  }();
  qCDebug(cmInputLog) << "QtXCore_DIR=" << qtCMakeDir.toUserOutput();
  const auto canQtCMakeDir = FilePath::fromString(qtCMakeDir.toFileInfo().canonicalFilePath());
  qCInfo(cmInputLog) << "QtXCore_DIR (canonical)=" << canQtCMakeDir.toUserOutput();
  if (qtCMakeDir.isEmpty())
    return FilePath();
  const auto baseQtDir = canQtCMakeDir.parentDir().parentDir().parentDir(); // Up 3 levels...
  qCDebug(cmInputLog) << "BaseQtDir:" << baseQtDir.toUserOutput();

  // Run a CMake project that would do qmake probing
  TemporaryDirectory qtcQMakeProbeDir("qtc-cmake-qmake-probe-XXXXXXXX");

  QFile cmakeListTxt(qtcQMakeProbeDir.filePath("CMakeLists.txt").toString());
  if (!cmakeListTxt.open(QIODevice::WriteOnly)) {
    return FilePath();
    }
  // FIXME replace by raw string when gcc 8+ is minimum
  cmakeListTxt.write(QByteArray(
"cmake_minimum_required(VERSION 3.15)\n"
"\n"
"project(qmake-probe LANGUAGES NONE)\n"
"\n"
"# Bypass Qt6's usage of find_dependency, which would require compiler\n"
"# and source code probing, which slows things unnecessarily\n"
"file(WRITE \"${CMAKE_SOURCE_DIR}/CMakeFindDependencyMacro.cmake\"\n"
"[=["
"    macro(find_dependency dep)\n"
"    endmacro()\n"
"]=])\n"
"set(CMAKE_MODULE_PATH \"${CMAKE_SOURCE_DIR}\")\n"
"\n"
"find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)\n"
"find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)\n"
"\n"
"if (CMAKE_CROSSCOMPILING)\n"
"    find_program(qmake_binary\n"
"        NAMES qmake qmake.bat\n"
"        PATHS \"${Qt${QT_VERSION_MAJOR}_DIR}/../../../bin\"\n"
"        NO_DEFAULT_PATH)\n"
"    file(WRITE \"${CMAKE_SOURCE_DIR}/qmake-location.txt\" \"${qmake_binary}\")\n"
"else()\n"
"    file(GENERATE\n"
"         OUTPUT \"${CMAKE_SOURCE_DIR}/qmake-location.txt\"\n"
"         CONTENT \"$<TARGET_PROPERTY:Qt${QT_VERSION_MAJOR}::qmake,IMPORTED_LOCATION>\")\n"
"endif()\n"
));
  cmakeListTxt.close();

  QtcProcess cmake;
  cmake.setTimeoutS(5);
  cmake.setDisableUnixTerminal();
  auto env = Environment::systemEnvironment();
  env.setupEnglishOutput();
  cmake.setEnvironment(env);
  cmake.setTimeOutMessageBoxEnabled(false);

  auto cmakeGenerator = config.stringValueOf(QByteArray("CMAKE_GENERATOR"));
  auto cmakeExecutable = config.filePathValueOf(QByteArray("CMAKE_COMMAND"));
  auto cmakeMakeProgram = config.filePathValueOf(QByteArray("CMAKE_MAKE_PROGRAM"));
  auto toolchainFile = config.filePathValueOf(QByteArray("CMAKE_TOOLCHAIN_FILE"));
  auto hostPath = config.filePathValueOf(QByteArray("QT_HOST_PATH"));

  QStringList args;
  args.push_back("-S");
  args.push_back(qtcQMakeProbeDir.path().path());
  args.push_back("-B");
  args.push_back(qtcQMakeProbeDir.filePath("build").path());
  args.push_back("-G");
  args.push_back(cmakeGenerator);

  if (!cmakeMakeProgram.isEmpty()) {
    args.push_back(QStringLiteral("-DCMAKE_MAKE_PROGRAM=%1").arg(cmakeMakeProgram.toString()));
  }
  if (toolchainFile.isEmpty()) {
    args.push_back(QStringLiteral("-DCMAKE_PREFIX_PATH=%1").arg(baseQtDir.toString()));
  } else {
    args.push_back(QStringLiteral("-DCMAKE_FIND_ROOT_PATH=%1").arg(baseQtDir.toString()));
    args.push_back(QStringLiteral("-DCMAKE_TOOLCHAIN_FILE=%1").arg(toolchainFile.toString()));
  }
  if (!hostPath.isEmpty()) {
    args.push_back(QStringLiteral("-DQT_HOST_PATH=%1").arg(hostPath.toString()));
  }

  qCDebug(cmInputLog) << "CMake probing for qmake path: " << cmakeExecutable.toUserOutput() << args;
  cmake.setCommand({cmakeExecutable, args});
  cmake.runBlocking();

  QFile qmakeLocationTxt(qtcQMakeProbeDir.filePath("qmake-location.txt").path());
  if (!qmakeLocationTxt.open(QIODevice::ReadOnly)) {
    return FilePath();
  }
  auto qmakeLocation = FilePath::fromUtf8(qmakeLocationTxt.readLine().constData());
  qCDebug(cmInputLog) << "qmake location: " << qmakeLocation.toUserOutput();

  return qmakeLocation;
}

static auto extractToolChainsFromCache(const CMakeConfig &config) -> QVector<ToolChainDescription>
{
  QVector<ToolChainDescription> result;
  auto haveCCxxCompiler = false;
  for (const auto &i : config) {
    if (!i.key.startsWith("CMAKE_") || !i.key.endsWith("_COMPILER"))
      continue;
    const auto language = i.key.mid(6, i.key.count() - 6 - 9); // skip "CMAKE_" and "_COMPILER"
    Id languageId;
    if (language == "CXX") {
      haveCCxxCompiler = true;
      languageId = ProjectExplorer::Constants::CXX_LANGUAGE_ID;
    } else if (language == "C") {
      haveCCxxCompiler = true;
      languageId = ProjectExplorer::Constants::C_LANGUAGE_ID;
    } else
      languageId = Id::fromName(language);
    result.append({FilePath::fromUtf8(i.value), languageId});
  }

  if (!haveCCxxCompiler) {
    const auto generator = config.valueOf("CMAKE_GENERATOR");
    QString cCompilerName;
    QString cxxCompilerName;
    if (generator.contains("Visual Studio")) {
      cCompilerName = "cl.exe";
      cxxCompilerName = "cl.exe";
    } else if (generator.contains("Xcode")) {
      cCompilerName = "clang";
      cxxCompilerName = "clang++";
    }

    if (!cCompilerName.isEmpty() && !cxxCompilerName.isEmpty()) {
      const auto linker = config.filePathValueOf("CMAKE_LINKER");
      if (!linker.isEmpty()) {
        const auto compilerPath = linker.parentDir();
        result.append({compilerPath.pathAppended(cCompilerName), ProjectExplorer::Constants::C_LANGUAGE_ID});
        result.append({compilerPath.pathAppended(cxxCompilerName), ProjectExplorer::Constants::CXX_LANGUAGE_ID});
      }
    }
  }

  return result;
}

auto CMakeProjectImporter::examineDirectory(const FilePath &importPath, QString *warningMessage) const -> QList<void*>
{
  qCInfo(cmInputLog) << "Examining directory:" << importPath.toUserOutput();
  const auto cacheFile = importPath.pathAppended("CMakeCache.txt");

  if (!cacheFile.exists()) {
    qCDebug(cmInputLog) << cacheFile.toUserOutput() << "does not exist, returning.";
    return {};
  }

  QString errorMessage;
  const auto config = CMakeBuildSystem::parseCMakeCacheDotTxt(cacheFile, &errorMessage);
  if (config.isEmpty() || !errorMessage.isEmpty()) {
    qCDebug(cmInputLog) << "Failed to read configuration from" << cacheFile << errorMessage;
    return {};
  }

  QByteArrayList buildConfigurationTypes = {config.valueOf("CMAKE_BUILD_TYPE")};
  if (buildConfigurationTypes.front().isEmpty()) {
    auto buildConfigurationTypesString = config.valueOf("CMAKE_CONFIGURATION_TYPES");
    if (!buildConfigurationTypesString.isEmpty())
      buildConfigurationTypes = buildConfigurationTypesString.split(';');
  }

  QList<void*> result;
  for (auto const &buildType : qAsConst(buildConfigurationTypes)) {
    auto data = std::make_unique<DirectoryData>();

    data->cmakeHomeDirectory = FilePath::fromUserInput(config.stringValueOf("CMAKE_HOME_DIRECTORY")).canonicalPath();
    const auto canonicalProjectDirectory = projectDirectory().canonicalPath();
    if (data->cmakeHomeDirectory != canonicalProjectDirectory) {
      *warningMessage = tr("Unexpected source directory \"%1\", expected \"%2\". " "This can be correct in some situations, for example when " "importing a standalone Qt test, but usually this is an error. " "Import the build anyway?").arg(data->cmakeHomeDirectory.toUserOutput(), canonicalProjectDirectory.toUserOutput());
    }

    data->buildDirectory = importPath;
    data->cmakeBuildType = buildType;

    data->cmakeBinary = config.filePathValueOf("CMAKE_COMMAND");
    data->generator = config.stringValueOf("CMAKE_GENERATOR");
    data->extraGenerator = config.stringValueOf("CMAKE_EXTRA_GENERATOR");
    data->platform = config.stringValueOf("CMAKE_GENERATOR_PLATFORM");
    data->toolset = config.stringValueOf("CMAKE_GENERATOR_TOOLSET");
    data->sysroot = config.filePathValueOf("CMAKE_SYSROOT");

    // Qt:
    const auto qmake = qmakeFromCMakeCache(config);
    if (!qmake.isEmpty())
      data->qt = findOrCreateQtVersion(qmake);

    // ToolChains:
    data->toolChains = extractToolChainsFromCache(config);

    qCInfo(cmInputLog) << "Offering to import" << importPath.toUserOutput();
    result.push_back(static_cast<void*>(data.release()));
  }
  return result;
}

auto CMakeProjectImporter::matchKit(void *directoryData, const Kit *k) const -> bool
{
  const DirectoryData *data = static_cast<DirectoryData*>(directoryData);

  auto cm = CMakeKitAspect::cmakeTool(k);
  if (!cm || cm->cmakeExecutable() != data->cmakeBinary)
    return false;

  if (CMakeGeneratorKitAspect::generator(k) != data->generator || CMakeGeneratorKitAspect::extraGenerator(k) != data->extraGenerator || CMakeGeneratorKitAspect::platform(k) != data->platform || CMakeGeneratorKitAspect::toolset(k) != data->toolset)
    return false;

  if (SysRootKitAspect::sysRoot(k) != data->sysroot)
    return false;

  if (data->qt.qt && QtSupport::QtKitAspect::qtVersionId(k) != data->qt.qt->uniqueId())
    return false;

  const auto allLanguages = ToolChainManager::allLanguages();
  for (const auto &tcd : data->toolChains) {
    if (!Utils::contains(allLanguages, [&tcd](const Id &language) { return language == tcd.language; }))
      continue;
    auto tc = ToolChainKitAspect::toolChain(k, tcd.language);
    if (!tc || !Utils::Environment::systemEnvironment().isSameExecutable(tc->compilerCommand().toString(), tcd.compilerPath.toString())) {
      return false;
    }
  }

  qCDebug(cmInputLog) << k->displayName() << "matches directoryData for" << data->buildDirectory.toUserOutput();
  return true;
}

auto CMakeProjectImporter::createKit(void *directoryData) const -> Kit*
{
  const DirectoryData *data = static_cast<DirectoryData*>(directoryData);

  return QtProjectImporter::createTemporaryKit(data->qt, [&data, this](Kit *k) {
    const auto cmtd = findOrCreateCMakeTool(data->cmakeBinary);
    QTC_ASSERT(cmtd.cmakeTool, return);
    if (cmtd.isTemporary)
      addTemporaryData(CMakeKitAspect::id(), cmtd.cmakeTool->id().toSetting(), k);
    CMakeKitAspect::setCMakeTool(k, cmtd.cmakeTool->id());

    CMakeGeneratorKitAspect::setGenerator(k, data->generator);
    CMakeGeneratorKitAspect::setExtraGenerator(k, data->extraGenerator);
    CMakeGeneratorKitAspect::setPlatform(k, data->platform);
    CMakeGeneratorKitAspect::setToolset(k, data->toolset);

    SysRootKitAspect::setSysRoot(k, data->sysroot);

    for (const auto &cmtcd : data->toolChains) {
      const auto tcd = findOrCreateToolChains(cmtcd);
      QTC_ASSERT(!tcd.tcs.isEmpty(), continue);

      if (tcd.areTemporary) {
        for (auto tc : tcd.tcs)
          addTemporaryData(ToolChainKitAspect::id(), tc->id(), k);
      }

      ToolChainKitAspect::setToolChain(k, tcd.tcs.at(0));
    }

    qCInfo(cmInputLog) << "Temporary Kit created.";
  });
}

auto CMakeProjectImporter::buildInfoList(void *directoryData) const -> const QList<BuildInfo>
{
  auto data = static_cast<const DirectoryData*>(directoryData);

  // create info:
  auto info = CMakeBuildConfigurationFactory::createBuildInfo(CMakeBuildConfigurationFactory::buildTypeFromByteArray(data->cmakeBuildType));
  info.buildDirectory = data->buildDirectory;
  info.displayName = info.typeName;

  QVariantMap config;
  config.insert(Constants::CMAKE_HOME_DIR, data->cmakeHomeDirectory.toString());
  info.extraInfo = config;

  qCDebug(cmInputLog) << "BuildInfo configured.";
  return {info};
}

auto CMakeProjectImporter::findOrCreateCMakeTool(const FilePath &cmakeToolPath) const -> CMakeProjectImporter::CMakeToolData
{
  CMakeToolData result;
  result.cmakeTool = CMakeToolManager::findByCommand(cmakeToolPath);
  if (!result.cmakeTool) {
    qCDebug(cmInputLog) << "Creating temporary CMakeTool for" << cmakeToolPath.toUserOutput();

    UpdateGuard guard(*this);

    auto newTool = std::make_unique<CMakeTool>(CMakeTool::ManualDetection, CMakeTool::createId());
    newTool->setFilePath(cmakeToolPath);
    newTool->setDisplayName(uniqueCMakeToolDisplayName(*newTool));

    result.cmakeTool = newTool.get();
    result.isTemporary = true;
    CMakeToolManager::registerCMakeTool(std::move(newTool));
  }
  return result;
}

auto CMakeProjectImporter::deleteDirectoryData(void *directoryData) const -> void
{
  delete static_cast<DirectoryData*>(directoryData);
}

auto CMakeProjectImporter::cleanupTemporaryCMake(Kit *k, const QVariantList &vl) -> void
{
  if (vl.isEmpty())
    return; // No temporary CMake
  QTC_ASSERT(vl.count() == 1, return);
  CMakeKitAspect::setCMakeTool(k, Id()); // Always mark Kit as not using this Qt
  CMakeToolManager::deregisterCMakeTool(Id::fromSetting(vl.at(0)));
  qCDebug(cmInputLog) << "Temporary CMake tool cleaned up.";
}

auto CMakeProjectImporter::persistTemporaryCMake(Kit *k, const QVariantList &vl) -> void
{
  if (vl.isEmpty())
    return; // No temporary CMake
  QTC_ASSERT(vl.count() == 1, return);
  const auto data = vl.at(0);
  auto tmpCmake = CMakeToolManager::findById(Id::fromSetting(data));
  auto actualCmake = CMakeKitAspect::cmakeTool(k);

  // User changed Kit away from temporary CMake that was set up:
  if (tmpCmake && actualCmake != tmpCmake)
    CMakeToolManager::deregisterCMakeTool(tmpCmake->id());

  qCDebug(cmInputLog) << "Temporary CMake tool made persistent.";
}

} // namespace Internal
} // namespace CMakeProjectManager

#ifdef WITH_TESTS

#include "cmakeprojectplugin.hpp"

#include <QTest>

namespace CMakeProjectManager {
namespace Internal {

void CMakeProjectPlugin::testCMakeProjectImporterQt_data()
{
    QTest::addColumn<QStringList>("cache");
    QTest::addColumn<QString>("expectedQmake");

    QTest::newRow("Empty input")
            << QStringList() << QString();

    QTest::newRow("Qt4")
            << QStringList({QString::fromLatin1("QT_QMAKE_EXECUTABLE=/usr/bin/xxx/qmake")})
            << "/usr/bin/xxx/qmake";

    // Everything else will require Qt installations!
}

void CMakeProjectPlugin::testCMakeProjectImporterQt()
{
    QFETCH(QStringList, cache);
    QFETCH(QString, expectedQmake);

    CMakeConfig config;
    foreach (const QString &c, cache) {
        const int pos = c.indexOf('=');
        Q_ASSERT(pos > 0);
        const QString key = c.left(pos);
        const QString value = c.mid(pos + 1);
        config.append(CMakeConfigItem(key.toUtf8(), value.toUtf8()));
    }

    FilePath realQmake = qmakeFromCMakeCache(config);
    QCOMPARE(realQmake.toString(), expectedQmake);
}
void CMakeProjectPlugin::testCMakeProjectImporterToolChain_data()
{
    QTest::addColumn<QStringList>("cache");
    QTest::addColumn<QByteArrayList>("expectedLanguages");
    QTest::addColumn<QStringList>("expectedToolChains");

    QTest::newRow("Empty input")
            << QStringList() << QByteArrayList() << QStringList();

    QTest::newRow("Unrelated input")
            << QStringList("CMAKE_SOMETHING_ELSE=/tmp") << QByteArrayList() << QStringList();
    QTest::newRow("CXX compiler")
            << QStringList({"CMAKE_CXX_COMPILER=/usr/bin/g++"})
            << QByteArrayList({"Cxx"})
            << QStringList({"/usr/bin/g++"});
    QTest::newRow("CXX compiler, C compiler")
            << QStringList({"CMAKE_CXX_COMPILER=/usr/bin/g++", "CMAKE_C_COMPILER=/usr/bin/clang"})
            << QByteArrayList({"Cxx", "C"})
            << QStringList({"/usr/bin/g++", "/usr/bin/clang"});
    QTest::newRow("CXX compiler, C compiler, strange compiler")
            << QStringList({"CMAKE_CXX_COMPILER=/usr/bin/g++",
                             "CMAKE_C_COMPILER=/usr/bin/clang",
                             "CMAKE_STRANGE_LANGUAGE_COMPILER=/tmp/strange/compiler"})
            << QByteArrayList({"Cxx", "C", "STRANGE_LANGUAGE"})
            << QStringList({"/usr/bin/g++", "/usr/bin/clang", "/tmp/strange/compiler"});
    QTest::newRow("CXX compiler, C compiler, strange compiler (with junk)")
            << QStringList({"FOO=test",
                             "CMAKE_CXX_COMPILER=/usr/bin/g++",
                             "CMAKE_BUILD_TYPE=debug",
                             "CMAKE_C_COMPILER=/usr/bin/clang",
                             "SOMETHING_COMPILER=/usr/bin/something",
                             "CMAKE_STRANGE_LANGUAGE_COMPILER=/tmp/strange/compiler",
                             "BAR=more test"})
            << QByteArrayList({"Cxx", "C", "STRANGE_LANGUAGE"})
            << QStringList({"/usr/bin/g++", "/usr/bin/clang", "/tmp/strange/compiler"});
}

void CMakeProjectPlugin::testCMakeProjectImporterToolChain()
{
    QFETCH(QStringList, cache);
    QFETCH(QByteArrayList, expectedLanguages);
    QFETCH(QStringList, expectedToolChains);

    QCOMPARE(expectedLanguages.count(), expectedToolChains.count());

    CMakeConfig config;
    foreach (const QString &c, cache) {
        const int pos = c.indexOf('=');
        Q_ASSERT(pos > 0);
        const QString key = c.left(pos);
        const QString value = c.mid(pos + 1);
        config.append(CMakeConfigItem(key.toUtf8(), value.toUtf8()));
    }

    const QVector<ToolChainDescription> tcs = extractToolChainsFromCache(config);
    QCOMPARE(tcs.count(), expectedLanguages.count());
    for (int i = 0; i < tcs.count(); ++i) {
        QCOMPARE(tcs.at(i).language, expectedLanguages.at(i));
        QCOMPARE(tcs.at(i).compilerPath.toString(), expectedToolChains.at(i));
    }
}

} // namespace Internal
} // namespace CMakeProjectManager

#endif
