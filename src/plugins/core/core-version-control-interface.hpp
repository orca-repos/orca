// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/filepath.hpp>
#include <utils/id.hpp>

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QString>

QT_FORWARD_DECLARE_CLASS(QMenu);

namespace Orca::Plugin::Core {

class ShellCommand;

class CORE_EXPORT IVersionControl : public QObject {
  Q_OBJECT

public:
  enum SettingsFlag {
    AutoOpen = 0x1
  };

  Q_DECLARE_FLAGS(SettingsFlags, SettingsFlag)

  enum Operation {
    AddOperation,
    DeleteOperation,
    MoveOperation,
    CreateRepositoryOperation,
    SnapshotOperations,
    AnnotateOperation,
    InitialCheckoutOperation
  };

  Q_ENUM(SettingsFlag)
  Q_ENUM(Operation)

  enum OpenSupportMode {
    NoOpen,
    /*!< Files can be edited without noticing the VCS */
    OpenOptional,
    /*!< Files can be opened by the VCS, or hijacked */
    OpenMandatory /*!< Files must always be opened by the VCS */
  };

  class CORE_EXPORT TopicCache {
  public:
    virtual ~TopicCache();
    auto topic(const Utils::FilePath &top_level) -> QString;

  protected:
    virtual auto trackFile(const Utils::FilePath &repository) -> Utils::FilePath = 0;
    virtual auto refreshTopic(const Utils::FilePath &repository) -> QString = 0;

  private:
    class TopicData {
    public:
      QDateTime time_stamp;
      QString topic;
    };

    QHash<Utils::FilePath, TopicData> m_cache;
  };

  IVersionControl();
  ~IVersionControl() override;

  virtual auto displayName() const -> QString = 0;
  virtual auto id() const -> Utils::Id = 0;

  /*!
   * \brief isVcsFileOrDirectory
   * \param file_path
   * \return True if filePath is a file or directory that is maintained by the
   * version control system.
   *
   * It will return true only for exact matches of the name, not for e.g. files in a
   * directory owned by the version control system (e.g. .git/control).
   *
   * This method needs to be thread safe!
   */
  virtual auto isVcsFileOrDirectory(const Utils::FilePath &file_path) const -> bool = 0;

  /*!
   * Returns whether files in this directory should be managed with this
   * version control.
   * If \a topLevel is non-null, it should return the topmost directory,
   * for which this IVersionControl should be used. The VcsManager assumes
   * that all files in the returned directory are managed by the same IVersionControl.
   */

  virtual auto managesDirectory(const Utils::FilePath &file_path, Utils::FilePath *top_level = nullptr) const -> bool = 0;

  /*!
   * Returns whether \a relativeFilePath is managed by this version control.
   *
   * \a workingDirectory is assumed to be part of a valid repository (not necessarily its
   * top level). \a fileName is expected to be relative to workingDirectory.
   */
  virtual auto managesFile(const Utils::FilePath &working_directory, const QString &file_name) const -> bool = 0;

  /*!
   * Returns the subset of \a filePaths that is not managed by this version control.
   *
   * The \a filePaths are expected to be absolute paths.
   */
  virtual auto unmanagedFiles(const Utils::FilePaths &file_paths) const -> Utils::FilePaths;

  /*!
   * Returns true is the VCS is configured to run.
   */
  virtual auto isConfigured() const -> bool = 0;
  /*!
   * Called to query whether a VCS supports the respective operations.
   *
   * Return false if the VCS is not configured yet.
   */
  virtual auto supportsOperation(Operation operation) const -> bool = 0;

  /*!
   * Returns the open support mode for \a filePath.
   */
  virtual auto openSupportMode(const Utils::FilePath &file_path) const -> OpenSupportMode;

  /*!
   * Called prior to save, if the file is read only. Should be implemented if
   * the scc requires a operation before editing the file, e.g. 'p4 edit'
   *
   * \note The EditorManager calls this for the editors.
   */
  virtual auto vcsOpen(const Utils::FilePath &file_path) -> bool = 0;

  /*!
   * Returns settings.
   */

  virtual auto settingsFlags() const -> SettingsFlags { return {}; }

  /*!
   * Called after a file has been added to a project If the version control
   * needs to know which files it needs to track you should reimplement this
   * function, e.g. 'p4 add', 'cvs add', 'svn add'.
   *
   * \note This function should be called from IProject subclasses after
   *       files are added to the project.
   */
  virtual auto vcsAdd(const Utils::FilePath &file_path) -> bool = 0;

  /*!
   * Called after a file has been removed from the project (if the user
   * wants), e.g. 'p4 delete', 'svn delete'.
   */
  virtual auto vcsDelete(const Utils::FilePath &file_path) -> bool = 0;

  /*!
   * Called to rename a file, should do the actual on disk renaming
   * (e.g. git mv, svn move, p4 move)
   */
  virtual auto vcsMove(const Utils::FilePath &from, const Utils::FilePath &to) -> bool = 0;

  /*!
   * Called to initialize the version control system in a directory.
   */
  virtual auto vcsCreateRepository(const Utils::FilePath &directory) -> bool = 0;

  /*!
   * Topic (e.g. name of the current branch)
   */
  virtual auto vcsTopic(const Utils::FilePath &top_level) -> QString;

  /*!
   * Display annotation for a file and scroll to line
   */
  virtual auto vcsAnnotate(const Utils::FilePath &file, int line) -> void = 0;

  /*!
   * Display text for Open operation
   */
  virtual auto vcsOpenText() const -> QString;

  /*!
   * Display text for Make Writable
   */
  virtual auto vcsMakeWritableText() const -> QString;

  /*!
   * Display details of reference
   */
  virtual auto vcsDescribe(const Utils::FilePath &working_directory, const QString &reference) -> void = 0;

  /*!
   * Return a list of paths where tools that came with the VCS may be installed.
   * This is helpful on windows where e.g. git comes with a lot of nice unix tools.
   */
  virtual auto additionalToolsPath() const -> Utils::FilePaths;

  /*!
   * Return a ShellCommand capable of checking out \a url into \a baseDirectory, where
   * a new subdirectory with \a localName will be created.
   *
   * \a extraArgs are passed on to the command being run.
   */
  virtual auto createInitialCheckoutCommand(const QString &url, const Utils::FilePath &base_directory, const QString &local_name, const QStringList &extra_args) -> ShellCommand*;
  virtual auto fillLinkContextMenu(QMenu *menu, const Utils::FilePath &working_directory, const QString &reference) -> void;
  virtual auto handleLink(const Utils::FilePath &working_directory, const QString &reference) -> bool;

  class CORE_EXPORT RepoUrl {
  public:
    explicit RepoUrl(const QString &location);

    QString protocol;
    QString user_name;
    QString host;
    QString path;
    quint16 port = 0;
    bool is_valid = false;
  };

  virtual auto getRepoUrl(const QString &location) -> RepoUrl;
  auto setTopicCache(TopicCache *topic_cache) -> void;

signals:
  auto repositoryChanged(const Utils::FilePath &repository) -> void;
  auto filesChanged(const QStringList &files) -> void;
  auto configurationChanged() -> void;

private:
  TopicCache *m_topic_cache = nullptr;
};

} // namespace Orca::Plugin::Core

Q_DECLARE_OPERATORS_FOR_FLAGS(Orca::Plugin::Core::IVersionControl::SettingsFlags)

#if defined(ORCA_BUILD_WITH_PLUGINS_TESTS)

#include <QSet>

namespace Orca::Plugin::Core {

class CORE_EXPORT TestVersionControl : public IVersionControl {
  Q_OBJECT

public:
  TestVersionControl(Utils::Id id, const QString &name) : m_id(id), m_displayName(name) { }
  ~TestVersionControl() override;

  bool isVcsFileOrDirectory(const Utils::FilePath &filePath) const final
  {
    Q_UNUSED(filePath)
    return false;
  }

  void setManagedDirectories(const QHash<Utils::FilePath, Utils::FilePath> &dirs);
  void setManagedFiles(const QSet<Utils::FilePath> &files);
  int dirCount() const { return m_dirCount; }
  int fileCount() const { return m_fileCount; }

  // IVersionControl interface
  QString displayName() const override { return m_displayName; }
  Utils::Id id() const override { return m_id; }
  bool managesDirectory(const Utils::FilePath &filePath, Utils::FilePath *topLevel) const override;
  bool managesFile(const Utils::FilePath &workingDirectory, const QString &fileName) const override;
  bool isConfigured() const override { return true; }
  bool supportsOperation(Operation) const override { return false; }
  bool vcsOpen(const Utils::FilePath &) override { return false; }
  bool vcsAdd(const Utils::FilePath &) override { return false; }
  bool vcsDelete(const Utils::FilePath &) override { return false; }
  bool vcsMove(const Utils::FilePath &, const Utils::FilePath &) override { return false; }
  bool vcsCreateRepository(const Utils::FilePath &) override { return false; }
  void vcsAnnotate(const Utils::FilePath &, int) override {}
  void vcsDescribe(const Utils::FilePath &, const QString &) override {}

private:
  Utils::Id m_id;
  QString m_displayName;
  QHash<Utils::FilePath, Utils::FilePath> m_managedDirs;
  QSet<Utils::FilePath> m_managedFiles;
  mutable int m_dirCount = 0;
  mutable int m_fileCount = 0;
};

} // namespace Orca::Plugin::Core

#endif
