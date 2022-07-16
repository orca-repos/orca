// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/fileutils.hpp>
#include <utils/id.hpp>

#include <QObject>
#include <QString>

namespace Orca::Plugin::Core {

class IVersionControl;
class MainWindow;

/* VcsManager:
 * 1) Provides functionality for finding the IVersionControl * for a given
 *    filename (findVersionControlForDirectory). Note that the VcsManager assumes
 *    that if a IVersionControl * manages a directory, then it also manages
 *    all the files and all the subdirectories.
 *    It works by asking all IVersionControl * if they manage the file, and ask
 *    for the topmost directory it manages. This information is cached and
 *    VCSManager thus knows pretty fast which IVersionControl * is responsible.
 * 2) Passes on the changes from the version controls caused by updating or
 *    branching repositories and routes them to its signals (repositoryChanged,
 *    filesChanged). */

class CORE_EXPORT VcsManager : public QObject {
  Q_OBJECT

public:
  static auto instance() -> VcsManager*;
  static auto extensionsInitialized() -> void;
  static auto versionControls() -> QList<IVersionControl*>;
  static auto versionControl(Utils::Id id) -> IVersionControl*;
  static auto resetVersionControlForDirectory(const Utils::FilePath &input_directory) -> void;
  static auto findVersionControlForDirectory(const Utils::FilePath &directory, QString *top_level_directory = nullptr) -> IVersionControl*;
  static auto findTopLevelForDirectory(const Utils::FilePath &directory) -> Utils::FilePath;
  static auto repositories(const IVersionControl *) -> QStringList;

  // Shows a confirmation dialog, whether the files should also be deleted
  // from revision control. Calls vcsDelete on the files. Returns the list
  // of files that failed.
  static auto promptToDelete(const Utils::FilePaths &file_paths) -> Utils::FilePaths;
  static auto promptToDelete(IVersionControl *version_control, const Utils::FilePaths &file_paths) -> Utils::FilePaths;
  static auto promptToDelete(IVersionControl *version_control, const QString &file_name) -> bool;

  // Shows a confirmation dialog, whether the files in the list should be
  // added to revision control. Calls vcsAdd for each file.
  static auto promptToAdd(const Utils::FilePath &directory, const Utils::FilePaths &filePaths) -> void;
  static auto emitRepositoryChanged(const Utils::FilePath &repository) -> void;

  // Utility messages for adding files
  static auto msgAddToVcsTitle() -> QString;
  static auto msgPromptToAddToVcs(const QStringList &files, const IVersionControl *vc) -> QString;
  static auto msgAddToVcsFailedTitle() -> QString;
  static auto msgToAddToVcsFailed(const QStringList &files, const IVersionControl *vc) -> QString;

  /*!
   * Return a list of paths where tools that came with the VCS may be installed.
   * This is helpful on windows where e.g. git comes with a lot of nice unix tools.
   */
  static auto additionalToolsPath() -> Utils::FilePaths;
  static auto clearVersionControlCache() -> void;

signals:
  auto repositoryChanged(const Utils::FilePath &repository) -> void;
  auto configurationChanged(const IVersionControl *vcs) -> void;

private:
  explicit VcsManager(QObject *parent = nullptr);
  ~VcsManager() override;

  auto handleConfigurationChanges() -> void;
  static auto addVersionControl(IVersionControl *vc) -> void;

  friend class Orca::Plugin::Core::MainWindow;
  friend class Orca::Plugin::Core::IVersionControl;
};

} // namespace Orca::Plugin::Core
