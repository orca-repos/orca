// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmakebuildtarget.hpp"
#include "cmakeprocess.hpp"
#include "cmakeprojectnodes.hpp"
#include "fileapidataextractor.hpp"

#include <projectexplorer/rawprojectpart.hpp>
#include <projectexplorer/treescanner.hpp>

#include <utils/filesystemwatcher.hpp>
#include <utils/optional.hpp>

#include <QFuture>
#include <QObject>
#include <QDateTime>

#include <memory>

namespace ProjectExplorer {
class ProjectNode;
}

namespace CMakeProjectManager {
namespace Internal {

class FileApiQtcData;

class FileApiReader final : public QObject {
  Q_OBJECT

public:
  FileApiReader();
  ~FileApiReader();

  auto setParameters(const BuildDirParameters &p) -> void;
  auto resetData() -> void;
  auto parse(bool forceCMakeRun, bool forceInitialConfiguration, bool forceExtraConfiguration) -> void;
  auto stop() -> void;
  auto stopCMakeRun() -> void;
  auto isParsing() const -> bool;
  auto projectFilesToWatch() const -> QSet<Utils::FilePath>;
  auto takeBuildTargets(QString &errorMessage) -> QList<CMakeBuildTarget>;
  auto takeParsedConfiguration(QString &errorMessage) -> CMakeConfig;
  auto ctestPath() const -> QString;
  auto createRawProjectParts(QString &errorMessage) -> ProjectExplorer::RawProjectParts;
  auto isMultiConfig() const -> bool;
  auto usesAllCapsTargets() const -> bool;
  auto lastCMakeExitCode() const -> int;
  auto rootProjectNode() -> std::unique_ptr<CMakeProjectNode>;
  auto topCmakeFile() const -> Utils::FilePath;

signals:
  auto configurationStarted() const -> void;
  auto dataAvailable(bool restoredFromBackup) const -> void;
  auto dirty() const -> void;
  auto errorOccurred(const QString &message) const -> void;

private:
  auto startState() -> void;
  auto endState(const Utils::FilePath &replyFilePath, bool restoredFromBackup) -> void;
  auto startCMakeState(const QStringList &configurationArguments) -> void;
  auto cmakeFinishedState() -> void;
  auto replyDirectoryHasChanged(const QString &directory) const -> void;
  auto makeBackupConfiguration(bool store) -> void;
  auto writeConfigurationIntoBuildDirectory(const QStringList &configuration) -> void;

  std::unique_ptr<CMakeProcess> m_cmakeProcess;
  // cmake data:
  CMakeConfig m_cache;
  QSet<CMakeFileInfo> m_cmakeFiles;
  QList<CMakeBuildTarget> m_buildTargets;
  ProjectExplorer::RawProjectParts m_projectParts;
  std::unique_ptr<CMakeProjectNode> m_rootProjectNode;
  QString m_ctestPath;
  bool m_isMultiConfig = false;
  bool m_usesAllCapsTargets = false;
  int m_lastCMakeExitCode = 0;
  Utils::optional<QFuture<std::shared_ptr<FileApiQtcData>>> m_future;
  // Update related:
  bool m_isParsing = false;
  BuildDirParameters m_parameters;
  // Notification on changes outside of creator:
  Utils::FileSystemWatcher m_watcher;
  QDateTime m_lastReplyTimestamp;
};

} // namespace Internal
} // namespace CMakeProjectManager
