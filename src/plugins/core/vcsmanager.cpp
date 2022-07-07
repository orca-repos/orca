// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "vcsmanager.hpp"
#include "iversioncontrol.hpp"
#include "icore.hpp"
#include "documentmanager.hpp"
#include "idocument.hpp"

#include <core/dialogs/addtovcsdialog.hpp>
#include <core/editormanager/editormanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/infobar.hpp>
#include <utils/optional.hpp>
#include <utils/qtcassert.hpp>

#include <QDir>
#include <QString>
#include <QList>
#include <QMap>
#include <QMessageBox>

using namespace Utils;

namespace Core {

#if defined(ORCA_BUILD_WITH_PLUGINS_TESTS)
const char TEST_PREFIX[] = "/8E3A9BA0-0B97-40DF-AEC1-2BDF9FC9EDBE/";
#endif

// ---- VCSManagerPrivate:
// Maintains a cache of top-level directory->version control.

class VcsManagerPrivate {
public:
  class VcsInfo {
  public:
    VcsInfo() = default;
    VcsInfo(IVersionControl *vc, QString tl) : version_control(vc), top_level(std::move(tl)) { }
    VcsInfo(const VcsInfo &other) = default;

    auto operator ==(const VcsInfo &other) const -> bool
    {
      return version_control == other.version_control && top_level == other.top_level;
    }

    IVersionControl *version_control = nullptr;
    QString top_level;
  };

  auto findInCache(const QString &dir) const -> optional<VcsInfo>
  {
    QTC_ASSERT(QDir(dir).isAbsolute(), return Utils::nullopt);
    QTC_ASSERT(!dir.endsWith(QLatin1Char('/')), return Utils::nullopt);
    QTC_ASSERT(QDir::fromNativeSeparators(dir) == dir, return Utils::nullopt);

    const auto it = m_cached_matches.constFind(dir);
    return it == m_cached_matches.constEnd() ? nullopt : make_optional(it.value());
  }

  auto clearCache() -> void
  {
    m_cached_matches.clear();
  }

  auto resetCache(const QString &dir) -> void
  {
    QTC_ASSERT(QDir(dir).isAbsolute(), return);
    QTC_ASSERT(!dir.endsWith(QLatin1Char('/')), return);
    QTC_ASSERT(QDir::fromNativeSeparators(dir) == dir, return);

    const QString dir_slash = dir + QLatin1Char('/');
    for(const auto &key: m_cached_matches.keys()) {
      if (key == dir || key.startsWith(dir_slash))
        m_cached_matches.remove(key);
    }
  }

  auto cache(IVersionControl *vc, const QString &top_level, const QString &dir) -> void
  {
    QTC_ASSERT(QDir(dir).isAbsolute(), return);
    QTC_ASSERT(!dir.endsWith(QLatin1Char('/')), return);
    QTC_ASSERT(QDir::fromNativeSeparators(dir) == dir, return);
    QTC_ASSERT(dir.startsWith(top_level + QLatin1Char('/')) || top_level == dir || top_level.isEmpty(), return);
    QTC_ASSERT((top_level.isEmpty() && !vc) || (!top_level.isEmpty() && vc), return);

    auto tmp_dir = dir;
    constexpr QChar slash = QLatin1Char('/');

    while (tmp_dir.count() >= top_level.count() && !tmp_dir.isEmpty()) {
      m_cached_matches.insert(tmp_dir, VcsInfo(vc, top_level));
      // if no vc was found, this might mean we're inside a repo internal directory (.git)
      // Cache only input directory, not parents
      if (!vc)
        break;
      if (const auto slash_pos = tmp_dir.lastIndexOf(slash); slash_pos >= 0)
        tmp_dir.truncate(slash_pos);
      else
        tmp_dir.clear();
    }
  }

  QList<IVersionControl*> m_version_control_list;
  QMap<QString, VcsInfo> m_cached_matches;
  IVersionControl *m_unconfigured_vcs = nullptr;
  FilePaths m_cached_additional_tools_paths;
  bool m_cached_additional_tools_paths_dirty = true;
};

static VcsManagerPrivate *d = nullptr;
static VcsManager *m_instance = nullptr;

VcsManager::VcsManager(QObject *parent) : QObject(parent)
{
  m_instance = this;
  d = new VcsManagerPrivate;
}

VcsManager::~VcsManager()
{
  m_instance = nullptr;
  delete d;
}

auto VcsManager::addVersionControl(IVersionControl *vc) -> void
{
  QTC_ASSERT(!d->m_version_control_list.contains(vc), return);
  d->m_version_control_list.append(vc);
}

auto VcsManager::instance() -> VcsManager*
{
  return m_instance;
}

auto VcsManager::extensionsInitialized() -> void
{
  // Change signal connections
  for(const auto& version_control: versionControls()) {
    connect(version_control, &IVersionControl::filesChanged, DocumentManager::instance(), [](const QStringList &fileNames) {
      DocumentManager::notifyFilesChangedInternally(transform(fileNames, &FilePath::fromString));
    });
    connect(version_control, &IVersionControl::repositoryChanged, m_instance, &VcsManager::repositoryChanged);
    connect(version_control, &IVersionControl::configurationChanged, m_instance, &VcsManager::handleConfigurationChanges);
  }
}

auto VcsManager::versionControls() -> QList<IVersionControl*>
{
  return d->m_version_control_list;
}

auto VcsManager::versionControl(const Id id) -> IVersionControl*
{
  return findOrDefault(versionControls(), equal(&IVersionControl::id, id));
}

static auto absoluteWithNoTrailingSlash(const QString &directory) -> QString
{
  auto res = QDir(directory).absolutePath();

  if (res.endsWith(QLatin1Char('/')))
    res.chop(1);

  return res;
}

auto VcsManager::resetVersionControlForDirectory(const FilePath &input_directory) -> void
{
  if (input_directory.isEmpty())
    return;

  const auto directory = absoluteWithNoTrailingSlash(input_directory.toString());
  d->resetCache(directory);

  emit m_instance->repositoryChanged(FilePath::fromString(directory));
}

auto VcsManager::findVersionControlForDirectory(const FilePath &input_directory, QString *top_level_directory) -> IVersionControl*
{
  using StringVersionControlPair = QPair<QString, IVersionControl*>;
  using StringVersionControlPairs = QList<StringVersionControlPair>;

  if (input_directory.isEmpty()) {
    if (top_level_directory)
      top_level_directory->clear();
    return nullptr;
  }

  // Make sure we an absolute path:
  auto directory = absoluteWithNoTrailingSlash(input_directory.toString());

  #ifdef ORCA_BUILD_WITH_PLUGINS_TESTS
    if (directory[0].isLetter() && directory.indexOf(QLatin1Char(':') + QLatin1String(TEST_PREFIX)) == 1)
        directory = directory.mid(2);
  #endif

  if (auto cached_data = d->findInCache(directory)) {
    if (top_level_directory)
      *top_level_directory = cached_data->top_level;
    return cached_data->version_control;
  }

  // Nothing: ask the IVersionControls directly.
  StringVersionControlPairs all_that_can_manage;

  for(auto &version_control: versionControls()) {
    FilePath top_level;
    if (version_control->managesDirectory(FilePath::fromString(directory), &top_level))
      all_that_can_manage.push_back(StringVersionControlPair(top_level.toString(), version_control));
  }

  // To properly find a nested repository (say, git checkout inside SVN),
  // we need to select the version control with the longest toplevel pathname.
  Utils::sort(all_that_can_manage, [](const StringVersionControlPair &l, const StringVersionControlPair &r) {
    return l.first.size() > r.first.size();
  });

  if (all_that_can_manage.isEmpty()) {
    d->cache(nullptr, QString(), directory); // register that nothing was found!
    // report result;
    if (top_level_directory)
      top_level_directory->clear();
    return nullptr;
  }

  // Register Vcs(s) with the cache
  auto tmp_dir = absoluteWithNoTrailingSlash(directory);

  #if defined ORCA_BUILD_WITH_PLUGINS_TESTS
    // Force caching of test directories (even though they do not exist):
    if (directory.startsWith(QLatin1String(TEST_PREFIX)))
        tmpDir = directory;
  #endif

  // directory might refer to a historical directory which doesn't exist.
  // In this case, don't cache it.
  if (!tmp_dir.isEmpty()) {
    constexpr QChar slash = QLatin1Char('/');
    const auto cend = all_that_can_manage.constEnd();
    for (auto i = all_that_can_manage.constBegin(); i != cend; ++i) {
      // If topLevel was already cached for another VC, skip this one
      if (tmp_dir.count() < i->first.count())
        continue;
      d->cache(i->second, i->first, tmp_dir);
      tmp_dir = i->first;
      if (const auto slash_pos = tmp_dir.lastIndexOf(slash); slash_pos >= 0)
        tmp_dir.truncate(slash_pos);
    }
  }

  // return result
  if (top_level_directory)
    *top_level_directory = all_that_can_manage.first().first;

  auto version_control = all_that_can_manage.first().second;

  if (const auto is_vcs_configured = version_control->isConfigured(); !is_vcs_configured || d->m_unconfigured_vcs) {
    Id vcs_warning("VcsNotConfiguredWarning");
    auto cur_document = EditorManager::currentDocument();
    if (is_vcs_configured) {
      if (cur_document && d->m_unconfigured_vcs == version_control) {
        cur_document->infoBar()->removeInfo(vcs_warning);
        d->m_unconfigured_vcs = nullptr;
      }
      return version_control;
    }
    if (auto info_bar = cur_document ? cur_document->infoBar() : nullptr; info_bar && info_bar->canInfoBeAdded(vcs_warning)) {
      InfoBarEntry info(vcs_warning, tr("%1 repository was detected but %1 is not configured.").arg(version_control->displayName()), InfoBarEntry::GlobalSuppression::Enabled);
      d->m_unconfigured_vcs = version_control;
      info.addCustomButton(ICore::msgShowOptionsDialog(), []() {
        QTC_ASSERT(d->m_unconfigured_vcs, return);
        ICore::showOptionsDialog(d->m_unconfigured_vcs->id());
      });
      info_bar->addInfo(info);
    }
    return nullptr;
  }
  return version_control;
}

auto VcsManager::findTopLevelForDirectory(const FilePath &directory) -> FilePath
{
  QString result;
  findVersionControlForDirectory(directory, &result);
  return FilePath::fromString(result);
}

auto VcsManager::repositories(const IVersionControl *vc) -> QStringList
{
  QStringList result;

  for (auto it = d->m_cached_matches.constBegin(); it != d->m_cached_matches.constEnd(); ++it) {
    if (it.value().version_control == vc)
      result.append(it.value().top_level);
  }

  return result;
}

auto VcsManager::promptToDelete(IVersionControl *version_control, const QString &file_name) -> bool
{
  return promptToDelete(version_control, {FilePath::fromString(file_name)}).isEmpty();
}

auto VcsManager::promptToDelete(const FilePaths &file_paths) -> FilePaths
{
  // Categorize files by their parent directory, so we won't call
  // findVersionControlForDirectory() more often than necessary.
  QMap<FilePath, FilePaths> files_by_parent_dir;

  for (const auto &fp : file_paths)
    files_by_parent_dir[fp.absolutePath()].append(fp);

  // Categorize by version control system.
  QHash<IVersionControl*, FilePaths> files_by_version_control;

  for (auto it = files_by_parent_dir.cbegin(); it != files_by_parent_dir.cend(); ++it) {
    if (const auto vc = findVersionControlForDirectory(it.key()))
      files_by_version_control[vc] << it.value();
  }

  // Remove the files.
  FilePaths failed_files;

  for (auto it = files_by_version_control.cbegin(); it != files_by_version_control.cend(); ++it)
    failed_files << promptToDelete(it.key(), it.value());

  return failed_files;
}

auto VcsManager::promptToDelete(IVersionControl *version_control, const FilePaths &file_paths) -> FilePaths
{
  QTC_ASSERT(version_control, return {});

  if (!version_control->supportsOperation(IVersionControl::DeleteOperation))
    return {};

  const QString file_list_for_ui = "<ul><li>" + transform(file_paths, [](const FilePath &fp) {
    return fp.toUserOutput();
  }).join("</li><li>") + "</li></ul>";

  const auto title = tr("Version Control");
  const auto msg = tr("Remove the following files from the version control system (%2)?" "%1Note: This might remove the local file.").arg(file_list_for_ui, version_control->displayName());

  if (const auto button = QMessageBox::question(ICore::dialogParent(), title, msg, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes); button != QMessageBox::Yes)
    return {};

  FilePaths failed_files;

  for (const auto &fp : file_paths) {
    if (!version_control->vcsDelete(fp))
      failed_files << fp;
  }

  return failed_files;
}

auto VcsManager::msgAddToVcsTitle() -> QString
{
  return tr("Add to Version Control");
}

auto VcsManager::msgPromptToAddToVcs(const QStringList &files, const IVersionControl *vc) -> QString
{
  return files.size() == 1 ? tr("Add the file\n%1\nto version control (%2)?").arg(files.front(), vc->displayName()) : tr("Add the files\n%1\nto version control (%2)?").arg(files.join(QString(QLatin1Char('\n'))), vc->displayName());
}

auto VcsManager::msgAddToVcsFailedTitle() -> QString
{
  return tr("Adding to Version Control Failed");
}

auto VcsManager::msgToAddToVcsFailed(const QStringList &files, const IVersionControl *vc) -> QString
{
  return files.size() == 1 ? tr("Could not add the file\n%1\nto version control (%2)\n").arg(files.front(), vc->displayName()) : tr("Could not add the following files to version control (%1)\n%2").arg(vc->displayName(), files.join(QString(QLatin1Char('\n'))));
}

auto VcsManager::additionalToolsPath() -> FilePaths
{
  if (d->m_cached_additional_tools_paths_dirty) {
    d->m_cached_additional_tools_paths.clear();
    for (const auto vc : versionControls())
      d->m_cached_additional_tools_paths.append(vc->additionalToolsPath());
    d->m_cached_additional_tools_paths_dirty = false;
  }

  return d->m_cached_additional_tools_paths;
}

auto VcsManager::promptToAdd(const FilePath &directory, const FilePaths &filePaths) -> void
{
  const auto vc = findVersionControlForDirectory(directory);

  if (!vc || !vc->supportsOperation(IVersionControl::AddOperation))
    return;

  const auto unmanaged_files = vc->unmanagedFiles(filePaths);

  if (unmanaged_files.isEmpty())
    return;

  if (Internal::AddToVcsDialog dlg(ICore::dialogParent(), msgAddToVcsTitle(), unmanaged_files, vc->displayName()); dlg.exec() == QDialog::Accepted) {
    QStringList not_added_to_vc;
    for (const auto &file : unmanaged_files) {
      if (!vc->vcsAdd(directory.resolvePath(file)))
        not_added_to_vc << file.toUserOutput();
    }
    if (!not_added_to_vc.isEmpty()) {
      QMessageBox::warning(ICore::dialogParent(), msgAddToVcsFailedTitle(), msgToAddToVcsFailed(not_added_to_vc, vc));
    }
  }
}

auto VcsManager::emitRepositoryChanged(const FilePath &repository) -> void
{
  emit m_instance->repositoryChanged(repository);
}

auto VcsManager::clearVersionControlCache() -> void
{
  auto repo_list = d->m_cached_matches.keys();
  d->clearCache();
  for(const auto &repo: repo_list) emit m_instance->repositoryChanged(FilePath::fromString(repo));
}

auto VcsManager::handleConfigurationChanges() -> void
{
  d->m_cached_additional_tools_paths_dirty = true;
  if (const auto vcs = qobject_cast<IVersionControl*>(sender()))
    emit configurationChanged(vcs);
}

} // namespace Core

#if defined(ORCA_BUILD_WITH_PLUGINS_TESTS)

#include <QtTest>

#include "coreplugin.hpp"

#include <extensionsystem/pluginmanager.hpp>

namespace Core {namespace Internal {

  const char ID_VCS_A[] = "A";
  const char ID_VCS_B[] = "B";

  using FileHash = QHash<FilePath, FilePath>;

  static FileHash makeHash(const QStringList &list)
  {
    FileHash result;
    for (const QString &i : list) {
      QStringList parts = i.split(QLatin1Char(':'));
      QTC_ASSERT(parts.count() == 2, continue);
      result.insert(FilePath::fromString(QString::fromLatin1(TEST_PREFIX) + parts.at(0)), FilePath::fromString(QString::fromLatin1(TEST_PREFIX) + parts.at(1)));
    }
    return result;
  }

  static QString makeString(const QString &s)
  {
    if (s.isEmpty())
      return QString();
    return QString::fromLatin1(TEST_PREFIX) + s;
  }

  void CorePlugin::testVcsManager_data()
  {
    // avoid conflicts with real files and directories:

    QTest::addColumn<QStringList>("dirsVcsA"); // <directory>:<toplevel>
    QTest::addColumn<QStringList>("dirsVcsB"); // <directory>:<toplevel>
    // <directory>:<toplevel>:<vcsid>:<- from cache, * from VCS>
    QTest::addColumn<QStringList>("results");

    QTest::newRow("A and B next to each other") << QStringList({"a:a", "a/1:a", "a/2:a", "a/2/5:a", "a/2/5/6:a"}) << QStringList({"b:b", "b/3:b", "b/4:b"}) << QStringList({
      ":::-",
      // empty directory to look up
      "c:::*",
      // Neither in A nor B
      "a:a:A:*",
      // in A
      "b:b:B:*",
      // in B
      "b/3:b:B:*",
      // in B
      "b/4:b:B:*",
      // in B
      "a/1:a:A:*",
      // in A
      "a/2:a:A:*",
      // in A
      ":::-",
      // empty directory to look up
      "a/2/5/6:a:A:*",
      // in A
      "a/2/5:a:A:-",
      // in A (cached from before!)
      // repeat: These need to come from the cache now:
      "c:::-",
      // Neither in A nor B
      "a:a:A:-",
      // in A
      "b:b:B:-",
      // in B
      "b/3:b:B:-",
      // in B
      "b/4:b:B:-",
      // in B
      "a/1:a:A:-",
      // in A
      "a/2:a:A:-",
      // in A
      "a/2/5/6:a:A:-",
      // in A
      "a/2/5:a:A:-" // in A
    });
    QTest::newRow("B in A") << QStringList({"a:a", "a/1:a", "a/2:a", "a/2/5:a", "a/2/5/6:a"}) << QStringList({"a/1/b:a/1/b", "a/1/b/3:a/1/b", "a/1/b/4:a/1/b", "a/1/b/3/5:a/1/b", "a/1/b/3/5/6:a/1/b"}) << QStringList({
      "a:a:A:*",
      // in A
      "c:::*",
      // Neither in A nor B
      "a/3:::*",
      // Neither in A nor B
      "a/1/b/x:::*",
      // Neither in A nor B
      "a/1/b:a/1/b:B:*",
      // in B
      "a/1:a:A:*",
      // in A
      "a/1/b/../../2:a:A:*" // in A
    });
    QTest::newRow("A and B") // first one wins...
      << QStringList({"a:a", "a/1:a", "a/2:a"}) << QStringList({"a:a", "a/1:a", "a/2:a"}) << QStringList({"a/2:a:A:*"});
  }

  void CorePlugin::testVcsManager()
  {
    // setup:
    QList<IVersionControl*> orig = Core::d->m_versionControlList;
    TestVersionControl *vcsA(new TestVersionControl(ID_VCS_A, QLatin1String("A")));
    TestVersionControl *vcsB(new TestVersionControl(ID_VCS_B, QLatin1String("B")));

    Core::d->m_versionControlList = {vcsA, vcsB};

    // test:
    QFETCH(QStringList, dirsVcsA);
    QFETCH(QStringList, dirsVcsB);
    QFETCH(QStringList, results);

    vcsA->setManagedDirectories(makeHash(dirsVcsA));
    vcsB->setManagedDirectories(makeHash(dirsVcsB));

    QString realTopLevel = QLatin1String("ABC"); // Make sure this gets cleared if needed.

    // From VCSes:
    int expectedCount = 0;
    foreach(const QString &result, results) {
      // qDebug() << "Expecting:" << result;

      QStringList split = result.split(QLatin1Char(':'));
      QCOMPARE(split.count(), 4);
      QVERIFY(split.at(3) == QLatin1String("*") || split.at(3) == QLatin1String("-"));

      const QString directory = split.at(0);
      const QString topLevel = split.at(1);
      const QString vcsId = split.at(2);
      bool fromCache = split.at(3) == QLatin1String("-");

      if (!fromCache && !directory.isEmpty())
        ++expectedCount;

      IVersionControl *vcs;
      vcs = VcsManager::findVersionControlForDirectory(FilePath::fromString(makeString(directory)), &realTopLevel);
      QCOMPARE(realTopLevel, makeString(topLevel));
      if (vcs)
        QCOMPARE(vcs->id().toString(), vcsId);
      else
        QCOMPARE(QString(), vcsId);
      QCOMPARE(vcsA->dirCount(), expectedCount);
      QCOMPARE(vcsA->fileCount(), 0);
      QCOMPARE(vcsB->dirCount(), expectedCount);
      QCOMPARE(vcsB->fileCount(), 0);
    }

    // teardown:
    qDeleteAll(Core::d->m_versionControlList);
    Core::d->m_versionControlList = orig;
  }

} // namespace Internal
} // namespace Core

#endif
