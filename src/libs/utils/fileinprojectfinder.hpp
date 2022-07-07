// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/utils_global.hpp>
#include <utils/fileutils.hpp>

#include <QHash>
#include <QSharedPointer>
#include <QStringList>

QT_FORWARD_DECLARE_CLASS(QUrl)

namespace Utils {
class QrcParser;

class ORCA_UTILS_EXPORT FileInProjectFinder
{
public:

    using FileHandler = std::function<void(const QString &, int)>;
    using DirectoryHandler = std::function<void(const QStringList &, int)>;

    FileInProjectFinder();
    ~FileInProjectFinder();

    auto setProjectDirectory(const FilePath &absoluteProjectPath) -> void;
    auto projectDirectory() const -> FilePath;
    auto setProjectFiles(const FilePaths &projectFiles) -> void;
    auto setSysroot(const FilePath &sysroot) -> void;
    auto addMappedPath(const FilePath &localFilePath, const QString &remoteFilePath) -> void;
    auto findFile(const QUrl &fileUrl, bool *success = nullptr) const -> FilePaths;
    auto findFileOrDirectory(const QString &originalPath, FileHandler fileHandler = nullptr, DirectoryHandler directoryHandler = nullptr) const -> bool;
    auto searchDirectories() const -> FilePaths;
    auto setAdditionalSearchDirectories(const FilePaths &searchDirectories) -> void;

private:
    struct PathMappingNode {
      ~PathMappingNode();
      FilePath localPath;
      QHash<QString, PathMappingNode*> children;
    };

    struct CacheEntry {
      QStringList paths;
      int matchLength = 0;
    };

    class QrcUrlFinder {
    public:
      auto find(const QUrl &fileUrl) const -> FilePaths;
      auto setProjectFiles(const FilePaths &projectFiles) -> void;

    private:
      FilePaths m_allQrcFiles;
      mutable QHash<QUrl, FilePaths> m_fileCache;
      mutable QHash<FilePath, QSharedPointer<QrcParser>> m_parserCache;
    };

    auto findInSearchPaths(const QString &filePath, FileHandler fileHandler, DirectoryHandler directoryHandler) const -> CacheEntry;
    static auto findInSearchPath(const QString &searchPath, const QString &filePath, FileHandler fileHandler, DirectoryHandler directoryHandler) -> CacheEntry;
    auto filesWithSameFileName(const QString &fileName) const -> QStringList;
    auto pathSegmentsWithSameName(const QString &path) const -> QStringList;
    auto handleSuccess(const QString &originalPath, const QStringList &found, int confidence, const char *where) const -> bool;
    static auto commonPostFixLength(const QString &candidatePath, const QString &filePathToFind) -> int;
    static auto bestMatches(const QStringList &filePaths, const QString &filePathToFind) -> QStringList;

    FilePath m_projectDir;
    FilePath m_sysroot;
    FilePaths m_projectFiles;
    FilePaths m_searchDirectories;
    PathMappingNode m_pathMapRoot;
    mutable QHash<QString, CacheEntry> m_cache;
    QrcUrlFinder m_qrcUrlFinder;
};

ORCA_UTILS_EXPORT auto chooseFileFromList(const FilePaths &candidates) -> FilePath;

} // namespace Utils
