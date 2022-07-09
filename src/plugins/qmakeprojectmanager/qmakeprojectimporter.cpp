// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakeprojectimporter.hpp"

#include "qmakebuildinfo.hpp"
#include "qmakekitinformation.hpp"
#include "qmakebuildconfiguration.hpp"
#include "qmakeproject.hpp"
#include "makefileparse.hpp"
#include "qmakestep.hpp"

#include <projectexplorer/buildinfo.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/kitmanager.hpp>
#include <projectexplorer/toolchain.hpp>
#include <projectexplorer/toolchainmanager.hpp>

#include <qtsupport/qtkitinformation.hpp>
#include <qtsupport/qtsupportconstants.hpp>
#include <qtsupport/qtversionfactory.hpp>
#include <qtsupport/qtversionmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QLoggingCategory>

#include <memory>

using namespace ProjectExplorer;
using namespace QmakeProjectManager;
using namespace QtSupport;
using namespace Utils;

namespace {

struct DirectoryData {
  QString makefile;
  Utils::FilePath buildDirectory;
  Utils::FilePath canonicalQmakeBinary;
  QtProjectImporter::QtVersionData qtVersionData;
  QString parsedSpec;
  QtVersion::QmakeBuildConfigs buildConfig;
  QString additionalArguments;
  QMakeStepConfig config;
  QMakeStepConfig::OsType osType;
};

} // namespace

namespace QmakeProjectManager {
namespace Internal {

const Utils::Id QT_IS_TEMPORARY("Qmake.TempQt");
constexpr char IOSQT[] = "Qt4ProjectManager.QtVersion.Ios"; // ugly

QmakeProjectImporter::QmakeProjectImporter(const FilePath &path) : QtProjectImporter(path) { }

auto QmakeProjectImporter::importCandidates() -> QStringList
{
  QStringList candidates;

  auto pfi = projectFilePath().toFileInfo();
  const auto prefix = pfi.baseName();
  candidates << pfi.absolutePath();

  foreach(Kit *k, KitManager::kits()) {
    const auto sbdir = QmakeBuildConfiguration::shadowBuildDirectory(projectFilePath(), k, QString(), BuildConfiguration::Unknown);

    const auto baseDir = sbdir.toFileInfo().absolutePath();

    foreach(const QString &dir, QDir(baseDir).entryList()) {
      const QString path = baseDir + QLatin1Char('/') + dir;
      if (dir.startsWith(prefix) && !candidates.contains(path))
        candidates << path;
    }
  }
  return candidates;
}

auto QmakeProjectImporter::examineDirectory(const FilePath &importPath, QString *warningMessage) const -> QList<void*>
{
  Q_UNUSED(warningMessage)
  QList<void*> result;
  const auto &logs = MakeFileParse::logging();

  auto makefiles = QDir(importPath.toString()).entryList(QStringList(QLatin1String("Makefile*")));
  qCDebug(logs) << "  Makefiles:" << makefiles;

  foreach(const QString &file, makefiles) {
    std::unique_ptr<DirectoryData> data(new DirectoryData);
    data->makefile = file;
    data->buildDirectory = importPath;

    qCDebug(logs) << "  Parsing makefile" << file;
    // find interesting makefiles
    const auto makefile = importPath / file;
    MakeFileParse parse(makefile, MakeFileParse::Mode::FilterKnownConfigValues);
    if (parse.makeFileState() != MakeFileParse::Okay) {
      qCDebug(logs) << "  Parsing the makefile failed" << makefile;
      continue;
    }
    if (parse.srcProFile() != projectFilePath()) {
      qCDebug(logs) << "  pro files doesn't match" << parse.srcProFile() << projectFilePath();
      continue;
    }

    data->canonicalQmakeBinary = parse.qmakePath().canonicalPath();
    if (data->canonicalQmakeBinary.isEmpty()) {
      qCDebug(logs) << "  " << parse.qmakePath() << "doesn't exist anymore";
      continue;
    }

    qCDebug(logs) << "  QMake:" << data->canonicalQmakeBinary;

    data->qtVersionData = QtProjectImporter::findOrCreateQtVersion(data->canonicalQmakeBinary);
    auto version = data->qtVersionData.qt;
    auto isTemporaryVersion = data->qtVersionData.isTemporary;

    QTC_ASSERT(version, continue);

    qCDebug(logs) << "  qt version:" << version->displayName() << " temporary:" << isTemporaryVersion;

    data->osType = parse.config().osType;

    qCDebug(logs) << "  osType:    " << data->osType;
    if (version->type() == QLatin1String(IOSQT) && data->osType == QMakeStepConfig::NoOsType) {
      data->osType = QMakeStepConfig::IphoneOS;
      qCDebug(logs) << "  IOS found without osType, adjusting osType" << data->osType;
    }

    // find qmake arguments and mkspec
    data->additionalArguments = parse.unparsedArguments();
    qCDebug(logs) << "  Unparsed arguments:" << data->additionalArguments;
    data->parsedSpec = QmakeBuildConfiguration::extractSpecFromArguments(&(data->additionalArguments), importPath, version);
    qCDebug(logs) << "  Extracted spec:" << data->parsedSpec;
    qCDebug(logs) << "  Arguments now:" << data->additionalArguments;

    const auto versionSpec = version->mkspec();
    if (data->parsedSpec.isEmpty() || data->parsedSpec == "default") {
      data->parsedSpec = versionSpec;
      qCDebug(logs) << "  No parsed spec or default spec => parsed spec now:" << data->parsedSpec;
    }
    data->buildConfig = parse.effectiveBuildConfig(data->qtVersionData.qt->defaultBuildConfig());
    data->config = parse.config();

    result.append(data.release());
  }
  return result;
}

auto QmakeProjectImporter::matchKit(void *directoryData, const Kit *k) const -> bool
{
  auto *data = static_cast<DirectoryData*>(directoryData);
  const auto &logs = MakeFileParse::logging();

  auto kitVersion = QtKitAspect::qtVersion(k);
  auto kitSpec = QmakeKitAspect::mkspec(k);
  auto tc = ToolChainKitAspect::cxxToolChain(k);
  if (kitSpec.isEmpty() && kitVersion)
    kitSpec = kitVersion->mkspecFor(tc);
  auto kitOsType = QMakeStepConfig::NoOsType;
  if (tc) {
    kitOsType = QMakeStepConfig::osTypeFor(tc->targetAbi(), kitVersion);
  }
  qCDebug(logs) << k->displayName() << "version:" << (kitVersion == data->qtVersionData.qt) << "spec:" << (kitSpec == data->parsedSpec) << "ostype:" << (kitOsType == data->osType);
  return kitVersion == data->qtVersionData.qt && kitSpec == data->parsedSpec && kitOsType == data->osType;
}

auto QmakeProjectImporter::createKit(void *directoryData) const -> Kit*
{
  auto *data = static_cast<DirectoryData*>(directoryData);
  return createTemporaryKit(data->qtVersionData, data->parsedSpec, data->osType);
}

auto QmakeProjectImporter::buildInfoList(void *directoryData) const -> const QList<BuildInfo>
{
  auto *data = static_cast<DirectoryData*>(directoryData);

  // create info:
  BuildInfo info;
  if (data->buildConfig & QtVersion::DebugBuild) {
    info.buildType = BuildConfiguration::Debug;
    info.displayName = QCoreApplication::translate("QmakeProjectManager::Internal::QmakeProjectImporter", "Debug");
  } else {
    info.buildType = BuildConfiguration::Release;
    info.displayName = QCoreApplication::translate("QmakeProjectManager::Internal::QmakeProjectImporter", "Release");
  }
  info.buildDirectory = data->buildDirectory;

  QmakeExtraBuildInfo extra;
  extra.additionalArguments = data->additionalArguments;
  extra.config = data->config;
  extra.makefile = data->makefile;
  info.extraInfo = QVariant::fromValue(extra);

  return {info};
}

auto QmakeProjectImporter::deleteDirectoryData(void *directoryData) const -> void
{
  delete static_cast<DirectoryData*>(directoryData);
}

static auto preferredToolChains(QtVersion *qtVersion, const QString &ms) -> const Toolchains
{
  const auto spec = ms.isEmpty() ? qtVersion->mkspec() : ms;

  const auto toolchains = ToolChainManager::toolchains();
  const auto qtAbis = qtVersion->qtAbis();
  const auto matcher = [&](const ToolChain *tc) {
    return qtAbis.contains(tc->targetAbi()) && tc->suggestedMkspecList().contains(spec);
  };
  const auto cxxToolchain = findOrDefault(toolchains, [matcher](const ToolChain *tc) {
    return tc->language() == ProjectExplorer::Constants::CXX_LANGUAGE_ID && matcher(tc);
  });
  const auto cToolchain = findOrDefault(toolchains, [matcher](const ToolChain *tc) {
    return tc->language() == ProjectExplorer::Constants::C_LANGUAGE_ID && matcher(tc);
  });
  Toolchains chosenToolchains;
  for (const auto tc : {cxxToolchain, cToolchain}) {
    if (tc)
      chosenToolchains << tc;
  };
  return chosenToolchains;
}

auto QmakeProjectImporter::createTemporaryKit(const QtProjectImporter::QtVersionData &data, const QString &parsedSpec, const QMakeStepConfig::OsType &osType) const -> Kit*
{
  Q_UNUSED(osType) // TODO use this to select the right toolchain?
  return QtProjectImporter::createTemporaryKit(data, [&data, parsedSpec](Kit *k) -> void {
    for (const auto tc : preferredToolChains(data.qt, parsedSpec))
      ToolChainKitAspect::setToolChain(k, tc);
    if (parsedSpec != data.qt->mkspec())
      QmakeKitAspect::setMkspec(k, parsedSpec, QmakeKitAspect::MkspecSource::Code);
  });
}

} // namespace Internal
} // namespace QmakeProjectManager
