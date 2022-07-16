// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fileapireader.hpp"

#include "fileapidataextractor.hpp"
#include "fileapiparser.hpp"

#include <core/core-message-manager.hpp>

#include <projectexplorer/projectexplorer.hpp>

#include <utils/algorithm.hpp>
#include <utils/runextensions.hpp>

#include <QLoggingCategory>

using namespace ProjectExplorer;
using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

static Q_LOGGING_CATEGORY(cmakeFileApiMode, "qtc.cmake.fileApiMode", QtWarningMsg);

using namespace FileApiDetails;

// --------------------------------------------------------------------
// FileApiReader:
// --------------------------------------------------------------------

FileApiReader::FileApiReader() : m_lastReplyTimestamp()
{
  QObject::connect(&m_watcher, &FileSystemWatcher::directoryChanged, this, &FileApiReader::replyDirectoryHasChanged);
}

FileApiReader::~FileApiReader()
{
  stop();
  resetData();
}

auto FileApiReader::setParameters(const BuildDirParameters &p) -> void
{
  qCDebug(cmakeFileApiMode) << "\n\n\n\n\n=============================================================\n";

  // Update:
  m_parameters = p;
  qCDebug(cmakeFileApiMode) << "Work directory:" << m_parameters.buildDirectory.toUserOutput();

  // Reset watcher:
  m_watcher.clear();

  FileApiParser::setupCMakeFileApi(m_parameters.buildDirectory, m_watcher);

  resetData();
}

auto FileApiReader::resetData() -> void
{
  m_cmakeFiles.clear();
  if (!m_parameters.sourceDirectory.isEmpty()) {
    CMakeFileInfo cmakeListsTxt;
    cmakeListsTxt.path = m_parameters.sourceDirectory.pathAppended("CMakeLists.txt");
    cmakeListsTxt.isCMakeListsDotTxt = true;
    m_cmakeFiles.insert(cmakeListsTxt);
  }

  m_cache.clear();
  m_buildTargets.clear();
  m_projectParts.clear();
  m_rootProjectNode.reset();
}

auto FileApiReader::parse(bool forceCMakeRun, bool forceInitialConfiguration, bool forceExtraConfiguration) -> void
{
  qCDebug(cmakeFileApiMode) << "Parse called with arguments: ForceCMakeRun:" << forceCMakeRun << " - forceConfiguration:" << forceInitialConfiguration << " - forceExtraConfiguration:" << forceExtraConfiguration;
  startState();

  const auto args = (forceInitialConfiguration ? m_parameters.initialCMakeArguments : QStringList()) + (forceExtraConfiguration ? (m_parameters.configurationChangesArguments + m_parameters.additionalCMakeArguments) : QStringList());
  qCDebug(cmakeFileApiMode) << "Parameters request these CMake arguments:" << args;

  const auto replyFile = FileApiParser::scanForCMakeReplyFile(m_parameters.buildDirectory);
  // Only need to update when one of the following conditions is met:
  //  * The user forces the cmake run,
  //  * The user provided arguments,
  //  * There is no reply file,
  //  * One of the cmakefiles is newer than the replyFile and the user asked
  //    for creator to run CMake as needed,
  //  * A query file is newer than the reply file
  const auto hasArguments = !args.isEmpty();
  const auto replyFileMissing = !replyFile.exists();
  const auto cmakeFilesChanged = m_parameters.cmakeTool() && m_parameters.cmakeTool()->isAutoRun() && anyOf(m_cmakeFiles, [&replyFile](const CMakeFileInfo &info) {
    return !info.isGenerated && info.path.lastModified() > replyFile.lastModified();
  });
  const auto queryFileChanged = anyOf(FileApiParser::cmakeQueryFilePaths(m_parameters.buildDirectory), [&replyFile](const FilePath &qf) {
    return qf.lastModified() > replyFile.lastModified();
  });

  const auto mustUpdate = forceCMakeRun || hasArguments || replyFileMissing || cmakeFilesChanged || queryFileChanged;
  qCDebug(cmakeFileApiMode) << QString("Do I need to run CMake? %1 " "(force: %2 | args: %3 | missing reply: %4 | " "cmakeFilesChanged: %5 | " "queryFileChanged: %6)").arg(mustUpdate).arg(forceCMakeRun).arg(hasArguments).arg(replyFileMissing).arg(cmakeFilesChanged).arg(queryFileChanged);

  if (mustUpdate) {
    qCDebug(cmakeFileApiMode) << QString("FileApiReader: Starting CMake with \"%1\".").arg(args.join("\", \""));
    startCMakeState(args);
  } else {
    endState(replyFile, false);
  }
}

auto FileApiReader::stop() -> void
{
  if (m_cmakeProcess)
    disconnect(m_cmakeProcess.get(), nullptr, this, nullptr);
  m_cmakeProcess.reset();

  if (m_future) {
    m_future->cancel();
    m_future->waitForFinished();
  }
  m_future = {};
  m_isParsing = false;
}

auto FileApiReader::stopCMakeRun() -> void
{
  if (m_cmakeProcess)
    m_cmakeProcess->terminate();
}

auto FileApiReader::isParsing() const -> bool
{
  return m_isParsing;
}

auto FileApiReader::projectFilesToWatch() const -> QSet<FilePath>
{
  return Utils::transform(Utils::filtered(m_cmakeFiles, [](const CMakeFileInfo &info) { return !info.isGenerated; }), [](const CMakeFileInfo &info) { return info.path; });
}

auto FileApiReader::takeBuildTargets(QString &errorMessage) -> QList<CMakeBuildTarget>
{
  Q_UNUSED(errorMessage)

  return std::exchange(m_buildTargets, {});
}

auto FileApiReader::takeParsedConfiguration(QString &errorMessage) -> CMakeConfig
{
  if (m_lastCMakeExitCode != 0)
    errorMessage = tr("CMake returned error code: %1").arg(m_lastCMakeExitCode);

  return std::exchange(m_cache, {});
}

auto FileApiReader::ctestPath() const -> QString
{
  // if we failed to run cmake we should not offer ctest information either
  return m_lastCMakeExitCode == 0 ? m_ctestPath : QString();
}

auto FileApiReader::isMultiConfig() const -> bool
{
  return m_isMultiConfig;
}

auto FileApiReader::usesAllCapsTargets() const -> bool
{
  return m_usesAllCapsTargets;
}

auto FileApiReader::createRawProjectParts(QString &errorMessage) -> RawProjectParts
{
  Q_UNUSED(errorMessage)

  return std::exchange(m_projectParts, {});
}

auto FileApiReader::startState() -> void
{
  qCDebug(cmakeFileApiMode) << "FileApiReader: START STATE.";
  QTC_ASSERT(!m_isParsing, return);
  QTC_ASSERT(!m_future.has_value(), return);

  m_isParsing = true;

  qCDebug(cmakeFileApiMode) << "FileApiReader: CONFIGURATION STARTED SIGNAL";
  emit configurationStarted();
}

auto FileApiReader::endState(const FilePath &replyFilePath, bool restoredFromBackup) -> void
{
  qCDebug(cmakeFileApiMode) << "FileApiReader: END STATE.";
  QTC_ASSERT(m_isParsing, return);
  QTC_ASSERT(!m_future.has_value(), return);

  const auto sourceDirectory = m_parameters.sourceDirectory;
  const auto buildDirectory = m_parameters.buildDirectory;
  const auto cmakeBuildType = m_parameters.cmakeBuildType == "Build" ? "" : m_parameters.cmakeBuildType;

  QTC_CHECK(!replyFilePath.needsDevice());
  m_lastReplyTimestamp = replyFilePath.lastModified();

  m_future = runAsync(ProjectExplorerPlugin::sharedThreadPool(), [replyFilePath, sourceDirectory, buildDirectory, cmakeBuildType](QFutureInterface<std::shared_ptr<FileApiQtcData>> &fi) {
    auto result = std::make_shared<FileApiQtcData>();
    auto data = FileApiParser::parseData(fi, replyFilePath, cmakeBuildType, result->errorMessage);
    if (result->errorMessage.isEmpty())
      *result = extractData(data, sourceDirectory, buildDirectory);
    else
      qWarning() << result->errorMessage;

    fi.reportResult(result);
  });
  onResultReady(m_future.value(), this, [this, sourceDirectory, buildDirectory, restoredFromBackup](const std::shared_ptr<FileApiQtcData> &value) {
    m_isParsing = false;
    m_cache = std::move(value->cache);
    m_cmakeFiles = std::move(value->cmakeFiles);
    m_buildTargets = std::move(value->buildTargets);
    m_projectParts = std::move(value->projectParts);
    m_rootProjectNode = std::move(value->rootProjectNode);
    m_ctestPath = std::move(value->ctestPath);
    m_isMultiConfig = std::move(value->isMultiConfig);
    m_usesAllCapsTargets = std::move(value->usesAllCapsTargets);

    if (value->errorMessage.isEmpty()) {
      emit this->dataAvailable(restoredFromBackup);
    } else {
      emit this->errorOccurred(value->errorMessage);
    }
    m_future = {};
  });
}

auto FileApiReader::makeBackupConfiguration(bool store) -> void
{
  auto reply = m_parameters.buildDirectory.pathAppended(".cmake/api/v1/reply");
  auto replyPrev = m_parameters.buildDirectory.pathAppended(".cmake/api/v1/reply.prev");
  if (!store)
    std::swap(reply, replyPrev);

  if (reply.exists()) {
    if (replyPrev.exists())
      replyPrev.removeRecursively();
    QTC_CHECK(!replyPrev.exists());
    if (!reply.renameFile(replyPrev))
      Orca::Plugin::Core::MessageManager::writeFlashing(tr("Failed to rename %1 to %2.").arg(reply.toString(), replyPrev.toString()));
  }

  auto cmakeCacheTxt = m_parameters.buildDirectory.pathAppended("CMakeCache.txt");
  auto cmakeCacheTxtPrev = m_parameters.buildDirectory.pathAppended("CMakeCache.txt.prev");
  if (!store)
    std::swap(cmakeCacheTxt, cmakeCacheTxtPrev);

  if (cmakeCacheTxt.exists())
    if (!FileUtils::copyIfDifferent(cmakeCacheTxt, cmakeCacheTxtPrev))
      Orca::Plugin::Core::MessageManager::writeFlashing(tr("Failed to copy %1 to %2.").arg(cmakeCacheTxt.toString(), cmakeCacheTxtPrev.toString()));
}

auto FileApiReader::writeConfigurationIntoBuildDirectory(const QStringList &configurationArguments) -> void
{
  const auto buildDir = m_parameters.buildDirectory;
  QTC_CHECK(buildDir.ensureWritableDir());

  QByteArray contents;
  QStringList unknownOptions;
  contents.append("# This file is managed by Qt Creator, do not edit!\n\n");
  contents.append(transform(CMakeConfig::fromArguments(configurationArguments, unknownOptions).toList(), [](const CMakeConfigItem &item) { return item.toCMakeSetLine(nullptr); }).join('\n').toUtf8());

  const auto settingsFile = buildDir / "qtcsettings.cmake";
  QTC_CHECK(settingsFile.writeFileContents(contents));
}

auto FileApiReader::rootProjectNode() -> std::unique_ptr<CMakeProjectNode>
{
  return std::exchange(m_rootProjectNode, {});
}

auto FileApiReader::topCmakeFile() const -> FilePath
{
  return m_cmakeFiles.size() == 1 ? (*m_cmakeFiles.begin()).path : FilePath{};
}

auto FileApiReader::lastCMakeExitCode() const -> int
{
  return m_lastCMakeExitCode;
}

auto FileApiReader::startCMakeState(const QStringList &configurationArguments) -> void
{
  qCDebug(cmakeFileApiMode) << "FileApiReader: START CMAKE STATE.";
  QTC_ASSERT(!m_cmakeProcess, return);

  m_cmakeProcess = std::make_unique<CMakeProcess>();

  connect(m_cmakeProcess.get(), &CMakeProcess::finished, this, &FileApiReader::cmakeFinishedState);

  qCDebug(cmakeFileApiMode) << ">>>>>> Running cmake with arguments:" << configurationArguments;
  // Reset watcher:
  m_watcher.removeFiles(m_watcher.files());
  m_watcher.removeDirectories(m_watcher.directories());

  makeBackupConfiguration(true);
  writeConfigurationIntoBuildDirectory(configurationArguments);
  m_cmakeProcess->run(m_parameters, configurationArguments);
}

auto FileApiReader::cmakeFinishedState() -> void
{
  qCDebug(cmakeFileApiMode) << "FileApiReader: CMAKE FINISHED STATE.";

  m_lastCMakeExitCode = m_cmakeProcess->lastExitCode();
  m_cmakeProcess.release()->deleteLater();

  if (m_lastCMakeExitCode != 0)
    makeBackupConfiguration(false);

  FileApiParser::setupCMakeFileApi(m_parameters.buildDirectory, m_watcher);

  endState(FileApiParser::scanForCMakeReplyFile(m_parameters.buildDirectory), m_lastCMakeExitCode != 0);
}

auto FileApiReader::replyDirectoryHasChanged(const QString &directory) const -> void
{
  if (m_isParsing)
    return; // This has been triggered by ourselves, ignore.

  const auto reply = FileApiParser::scanForCMakeReplyFile(m_parameters.buildDirectory);
  const auto dir = reply.absolutePath();
  if (dir.isEmpty())
    return; // CMake started to fill the result dir, but has not written a result file yet
  QTC_CHECK(!dir.needsDevice());
  QTC_ASSERT(dir.path() == directory, return);

  if (m_lastReplyTimestamp.isValid() && reply.lastModified() > m_lastReplyTimestamp) emit dirty();
}

} // namespace Internal
} // namespace CMakeProjectManager
