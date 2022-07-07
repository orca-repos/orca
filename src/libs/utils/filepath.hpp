// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"
#include "porting.hpp"

#include "hostosinfo.hpp"

#include <QDir>
#include <QDirIterator>
#include <QMetaType>

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QDateTime;
class QDebug;
class QFileInfo;
class QUrl;
QT_END_NAMESPACE

class tst_fileutils; // This becomes a friend of Utils::FilePath for testing private methods.

namespace Utils {

class Environment;
class EnvironmentChange;

class ORCA_UTILS_EXPORT FileFilter {
public:
  FileFilter(const QStringList &nameFilters, QDir::Filters fileFilters = QDir::NoFilter, QDirIterator::IteratorFlags flags = QDirIterator::NoIteratorFlags);

  const QStringList nameFilters;
  const QDir::Filters fileFilters = QDir::NoFilter;
  const QDirIterator::IteratorFlags iteratorFlags = QDirIterator::NoIteratorFlags;
};

class ORCA_UTILS_EXPORT FilePath {
public:
  FilePath();

  template <size_t N>
  FilePath(const char (&literal)[N]) { setFromString(literal); }

  [[nodiscard]] static auto fromString(const QString &filepath) -> FilePath;
  [[nodiscard]] static auto fromStringWithExtension(const QString &filepath, const QString &defaultExtension) -> FilePath;
  [[nodiscard]] static auto fromUserInput(const QString &filepath) -> FilePath;
  [[nodiscard]] static auto fromUtf8(const char *filepath, int filepathSize = -1) -> FilePath;
  [[nodiscard]] static auto fromVariant(const QVariant &variant) -> FilePath;
  [[nodiscard]] static auto fromUrl(const QUrl &url) -> FilePath;
  auto toUserOutput() const -> QString;
  auto toString() const -> QString;
  auto toVariant() const -> QVariant;
  auto toUrl() const -> QUrl;
  auto scheme() const -> QString { return m_scheme; }
  auto setScheme(const QString &scheme) -> void;
  auto host() const -> QString { return m_host; }
  auto setHost(const QString &host) -> void;
  auto path() const -> QString { return m_data; }
  auto setPath(const QString &path) -> void { m_data = path; }
  auto fileName() const -> QString;
  auto fileNameWithPathComponents(int pathComponents) const -> QString;
  auto baseName() const -> QString;
  auto completeBaseName() const -> QString;
  auto suffix() const -> QString;
  auto completeSuffix() const -> QString;
  [[nodiscard]] auto pathAppended(const QString &str) const -> FilePath;
  [[nodiscard]] auto stringAppended(const QString &str) const -> FilePath;
  auto startsWith(const QString &s) const -> bool;
  auto endsWith(const QString &s) const -> bool;
  auto exists() const -> bool;
  auto parentDir() const -> FilePath;
  auto isChildOf(const FilePath &s) const -> bool;
  auto isWritableDir() const -> bool;
  auto isWritableFile() const -> bool;
  auto ensureWritableDir() const -> bool;
  auto ensureExistingFile() const -> bool;
  auto isExecutableFile() const -> bool;
  auto isReadableFile() const -> bool;
  auto isReadableDir() const -> bool;
  auto isRelativePath() const -> bool;
  auto isAbsolutePath() const -> bool { return !isRelativePath(); }
  auto isFile() const -> bool;
  auto isDir() const -> bool;
  auto isNewerThan(const QDateTime &timeStamp) const -> bool;
  auto lastModified() const -> QDateTime;
  auto permissions() const -> QFile::Permissions;
  auto setPermissions(QFile::Permissions permissions) const -> bool;
  auto osType() const -> OsType;
  auto removeFile() const -> bool;
  auto removeRecursively(QString *error = nullptr) const -> bool;
  auto copyFile(const FilePath &target) const -> bool;
  auto renameFile(const FilePath &target) const -> bool;
  auto fileSize() const -> qint64;
  auto bytesAvailable() const -> qint64;
  auto createDir() const -> bool;
  auto dirEntries(const FileFilter &filter, QDir::SortFlags sort = QDir::NoSort) const -> QList<FilePath>;
  auto dirEntries(QDir::Filters filters) const -> QList<FilePath>;
  auto fileContents(qint64 maxSize = -1, qint64 offset = 0) const -> QByteArray;
  auto writeFileContents(const QByteArray &data) const -> bool;
  auto operator==(const FilePath &other) const -> bool;
  auto operator!=(const FilePath &other) const -> bool;
  auto operator<(const FilePath &other) const -> bool;
  auto operator<=(const FilePath &other) const -> bool;
  auto operator>(const FilePath &other) const -> bool;
  auto operator>=(const FilePath &other) const -> bool;
  [[nodiscard]] auto operator+(const QString &s) const -> FilePath;
  [[nodiscard]] auto operator/(const QString &str) const -> FilePath;
  auto caseSensitivity() const -> Qt::CaseSensitivity;
  auto clear() -> void;
  auto isEmpty() const -> bool;
  auto hash(uint seed) const -> QHashValueType;
  [[nodiscard]] auto resolvePath(const FilePath &tail) const -> FilePath;
  [[nodiscard]] auto resolvePath(const QString &tail) const -> FilePath;
  [[nodiscard]] auto cleanPath() const -> FilePath;
  [[nodiscard]] auto canonicalPath() const -> FilePath;
  [[nodiscard]] auto symLinkTarget() const -> FilePath;
  [[nodiscard]] auto resolveSymlinks() const -> FilePath;
  [[nodiscard]] auto withExecutableSuffix() const -> FilePath;
  [[nodiscard]] auto relativeChildPath(const FilePath &parent) const -> FilePath;
  [[nodiscard]] auto relativePath(const FilePath &anchor) const -> FilePath;
  [[nodiscard]] auto searchInDirectories(const QList<FilePath> &dirs) const -> FilePath;
  [[nodiscard]] auto searchInPath(const QList<FilePath> &additionalDirs = {}) const -> FilePath;
  [[nodiscard]] auto deviceEnvironment() const -> Environment;
  [[nodiscard]] auto onDevice(const FilePath &deviceTemplate) const -> FilePath;
  [[nodiscard]] auto withNewPath(const QString &newPath) const -> FilePath;
  auto iterateDirectory(const std::function<bool(const FilePath &item)> &callBack, const FileFilter &filter) const -> void;
  // makes sure that capitalization of directories is canonical
  // on Windows and macOS. This is rarely needed.
  [[nodiscard]] auto normalizedPathName() const -> FilePath;
  auto nativePath() const -> QString;
  auto shortNativePath() const -> QString;
  auto startsWithDriveLetter() const -> bool;
  static auto formatFilePaths(const QList<FilePath> &files, const QString &separator) -> QString;
  static auto removeDuplicates(QList<FilePath> &files) -> void;
  static auto sort(QList<FilePath> &files) -> void;

  // Asynchronous interface
  template <class ...Args>
  using Continuation = std::function<void(Args ...)>;
  auto asyncCopyFile(const Continuation<bool> &cont, const FilePath &target) const -> void;
  auto asyncFileContents(const Continuation<const QByteArray&> &cont, qint64 maxSize = -1, qint64 offset = 0) const -> void;
  auto asyncWriteFileContents(const Continuation<bool> &cont, const QByteArray &data) const -> void;

  // Prefer not to use
  // Using needsDevice() in "user" code is likely to result in code that
  // makes a local/remote distinction which should be avoided in general.
  // There are usually other means available. E.g. distinguishing based
  // on FilePath::osType().
  auto needsDevice() const -> bool;

  // Deprecated.
  [[nodiscard]] static auto fromFileInfo(const QFileInfo &info) -> FilePath; // Avoid.
  [[nodiscard]] auto toFileInfo() const -> QFileInfo;                        // Avoid.
  [[nodiscard]] auto toDir() const -> QDir;                                  // Avoid.
  [[nodiscard]] auto absolutePath() const -> FilePath;                       // Avoid. Use resolvePath(...)[.parent()] with proper base.
  [[nodiscard]] auto absoluteFilePath() const -> FilePath;                   // Avoid. Use resolvePath(...) with proper base.

private:
  friend class ::tst_fileutils;
  static auto calcRelativePath(const QString &absolutePath, const QString &absoluteAnchorPath) -> QString;
  auto setFromString(const QString &filepath) -> void;
  [[nodiscard]] auto mapToDevicePath() const -> QString;

  QString m_scheme;
  QString m_host; // May contain raw slashes.
  QString m_data;
};

using FilePaths = QList<FilePath>;

inline auto qHash(const Utils::FilePath &a, uint seed = 0) -> QHashValueType
{
  return a.hash(seed);
}

} // namespace Utils

QT_BEGIN_NAMESPACE ORCA_UTILS_EXPORT auto operator<<(QDebug dbg, const Utils::FilePath &c) -> QDebug;
QT_END_NAMESPACE Q_DECLARE_METATYPE(Utils::FilePath)

namespace std {
template <>
struct ORCA_UTILS_EXPORT hash<Utils::FilePath> {
  using argument_type = Utils::FilePath;
  using result_type = size_t;
  auto operator()(const argument_type &fn) const -> result_type;
};
} // namespace std
