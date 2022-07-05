// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "filepath.h"
#include "hostosinfo.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMetaType>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamWriter> // Mac.

#ifdef QT_WIDGETS_LIB
#include <QFileDialog>
#endif

#include <functional>
#include <memory>

QT_BEGIN_NAMESPACE
class QDataStream;
class QTextStream;
class QWidget;

// for withNtfsPermissions
#ifdef Q_OS_WIN
extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
#endif
QT_END_NAMESPACE

namespace Utils {

class DeviceFileHooks {
public:
  std::function<bool(const FilePath &)> isExecutableFile;
  std::function<bool(const FilePath &)> isReadableFile;
  std::function<bool(const FilePath &)> isReadableDir;
  std::function<bool(const FilePath &)> isWritableDir;
  std::function<bool(const FilePath &)> isWritableFile;
  std::function<bool(const FilePath &)> isFile;
  std::function<bool(const FilePath &)> isDir;
  std::function<bool(const FilePath &)> ensureWritableDir;
  std::function<bool(const FilePath &)> ensureExistingFile;
  std::function<bool(const FilePath &)> createDir;
  std::function<bool(const FilePath &)> exists;
  std::function<bool(const FilePath &)> removeFile;
  std::function<bool(const FilePath &)> removeRecursively;
  std::function<bool(const FilePath &, const FilePath &)> copyFile;
  std::function<bool(const FilePath &, const FilePath &)> renameFile;
  std::function<FilePath(const FilePath &, const QList<FilePath> &)> searchInPath;
  std::function<FilePath(const FilePath &)> symLinkTarget;
  std::function<FilePath(const FilePath &)> mapToGlobalPath;
  std::function<QString(const FilePath &)> mapToDevicePath;
  std::function<void(const FilePath &, const std::function<bool(const FilePath &)> &, // Abort on 'false' return.
                     const FileFilter &)> iterateDirectory;
  std::function<QByteArray(const FilePath &, qint64, qint64)> fileContents;
  std::function<bool(const FilePath &, const QByteArray &)> writeFileContents;
  std::function<QDateTime(const FilePath &)> lastModified;
  std::function<QFile::Permissions(const FilePath &)> permissions;
  std::function<bool(const FilePath &, QFile::Permissions)> setPermissions;
  std::function<OsType(const FilePath &)> osType;
  std::function<Environment(const FilePath &)> environment;
  std::function<qint64(const FilePath &)> fileSize;
  std::function<qint64(const FilePath &)> bytesAvailable;

  template <class ...Args>
  using Continuation = std::function<void(Args ...)>;
  std::function<void(const Continuation<bool> &, const FilePath &, const FilePath &)> asyncCopyFile;
  std::function<void(const Continuation<const QByteArray&> &, const FilePath &, qint64, qint64)> asyncFileContents;
  std::function<void(const Continuation<bool> &, const FilePath &, const QByteArray &)> asyncWriteFileContents;
};

class ORCA_UTILS_EXPORT FileUtils {
public:
  #ifdef QT_GUI_LIB
  class ORCA_UTILS_EXPORT CopyAskingForOverwrite {
  public:
    CopyAskingForOverwrite(QWidget *dialogParent, const std::function<void(FilePath)> &postOperation = {});
    auto operator()(const FilePath &src, const FilePath &dest, QString *error) -> bool;
    auto files() const -> QList<FilePath>;

  private:
    QWidget *m_parent;
    FilePaths m_files;
    std::function<void(FilePath)> m_postOperation;
    bool m_overwriteAll = false;
    bool m_skipAll = false;
  };
  #endif // QT_GUI_LIB

  static auto copyRecursively(const FilePath &srcFilePath, const FilePath &tgtFilePath, QString *error = nullptr) -> bool;
  template <typename T>
  static auto copyRecursively(const FilePath &srcFilePath, const FilePath &tgtFilePath, QString *error, T &&copyHelper) -> bool;
  static auto copyIfDifferent(const FilePath &srcFilePath, const FilePath &tgtFilePath) -> bool;
  static auto fileSystemFriendlyName(const QString &name) -> QString;
  static auto indexOfQmakeUnfriendly(const QString &name, int startpos = 0) -> int;
  static auto qmakeFriendlyName(const QString &name) -> QString;
  static auto makeWritable(const FilePath &path) -> bool;
  static auto normalizedPathName(const QString &name) -> QString;
  static auto isRelativePath(const QString &fileName) -> bool;
  static auto isAbsolutePath(const QString &fileName) -> bool { return !isRelativePath(fileName); }
  static auto commonPath(const FilePath &oldCommonPath, const FilePath &fileName) -> FilePath;
  static auto fileId(const FilePath &fileName) -> QByteArray;
  static auto homePath() -> FilePath;
  static auto setDeviceFileHooks(const DeviceFileHooks &hooks) -> void;

  #ifdef QT_WIDGETS_LIB
  static auto setDialogParentGetter(const std::function<QWidget *()> &getter) -> void;
  static auto getOpenFilePath(QWidget *parent, const QString &caption, const FilePath &dir = {}, const QString &filter = {}, QString *selectedFilter = nullptr, QFileDialog::Options options = {}) -> FilePath;
  static auto getSaveFilePath(QWidget *parent, const QString &caption, const FilePath &dir = {}, const QString &filter = {}, QString *selectedFilter = nullptr, QFileDialog::Options options = {}) -> FilePath;
  static auto getExistingDirectory(QWidget *parent, const QString &caption, const FilePath &dir = {}, QFileDialog::Options options = QFileDialog::ShowDirsOnly) -> FilePath;
  static auto getOpenFilePaths(QWidget *parent, const QString &caption, const FilePath &dir = {}, const QString &filter = {}, QString *selectedFilter = nullptr, QFileDialog::Options options = {}) -> FilePaths;
  #endif
};

template <typename T>
auto FileUtils::copyRecursively(const FilePath &srcFilePath, const FilePath &tgtFilePath, QString *error, T &&copyHelper) -> bool
{
  if (srcFilePath.isDir()) {
    if (!tgtFilePath.exists()) {
      if (!tgtFilePath.ensureWritableDir()) {
        if (error) {
          *error = QCoreApplication::translate("Utils::FileUtils", "Failed to create directory \"%1\".").arg(tgtFilePath.toUserOutput());
        }
        return false;
      }
    }
    const QDir sourceDir(srcFilePath.toString());
    const QStringList fileNames = sourceDir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QString &fileName : fileNames) {
      const FilePath newSrcFilePath = srcFilePath / fileName;
      const FilePath newTgtFilePath = tgtFilePath / fileName;
      if (!copyRecursively(newSrcFilePath, newTgtFilePath, error, copyHelper))
        return false;
    }
  } else {
    if (!copyHelper(srcFilePath, tgtFilePath, error))
      return false;
  }
  return true;
}

// for actually finding out if e.g. directories are writable on Windows
#ifdef Q_OS_WIN

template <typename T>
auto withNtfsPermissions(const std::function<T()> &task) -> T
{
  qt_ntfs_permission_lookup++;
  T result = task();
  qt_ntfs_permission_lookup--;
  return result;
}

template <>
ORCA_UTILS_EXPORT auto withNtfsPermissions(const std::function<void()> &task) -> void;

#else // Q_OS_WIN

template <typename T>
T withNtfsPermissions(const std::function<T()> &task)
{
    return task();
}

#endif // Q_OS_WIN

class ORCA_UTILS_EXPORT FileReader {
  Q_DECLARE_TR_FUNCTIONS(Utils::FileUtils) // sic!

public:
  static auto fetchQrc(const QString &fileName) -> QByteArray;                                 // Only for internal resources
  auto fetch(const FilePath &filePath, QIODevice::OpenMode mode = QIODevice::NotOpen) -> bool; // QIODevice::ReadOnly is implicit
  auto fetch(const FilePath &filePath, QIODevice::OpenMode mode, QString *errorString) -> bool;
  auto fetch(const FilePath &filePath, QString *errorString) -> bool { return fetch(filePath, QIODevice::NotOpen, errorString); }
  #ifdef QT_GUI_LIB
  auto fetch(const FilePath &filePath, QIODevice::OpenMode mode, QWidget *parent) -> bool;
  auto fetch(const FilePath &filePath, QWidget *parent) -> bool { return fetch(filePath, QIODevice::NotOpen, parent); }
  #endif // QT_GUI_LIB
  auto data() const -> const QByteArray& { return m_data; }
  auto errorString() const -> const QString& { return m_errorString; }
private:
  QByteArray m_data;
  QString m_errorString;
};

class ORCA_UTILS_EXPORT FileSaverBase {
  Q_DECLARE_TR_FUNCTIONS(Utils::FileUtils) // sic!

public:
  FileSaverBase();
  virtual ~FileSaverBase();

  auto filePath() const -> FilePath { return m_filePath; }
  auto hasError() const -> bool { return m_hasError; }
  auto errorString() const -> QString { return m_errorString; }
  virtual auto finalize() -> bool;
  auto finalize(QString *errStr) -> bool;
  #ifdef QT_GUI_LIB
  auto finalize(QWidget *parent) -> bool;
  #endif

  auto write(const char *data, int len) -> bool;
  auto write(const QByteArray &bytes) -> bool;
  auto setResult(QTextStream *stream) -> bool;
  auto setResult(QDataStream *stream) -> bool;
  auto setResult(QXmlStreamWriter *stream) -> bool;
  auto setResult(bool ok) -> bool;

  auto file() -> QFile* { return m_file.get(); }

protected:
  std::unique_ptr<QFile> m_file;
  FilePath m_filePath;
  QString m_errorString;
  bool m_hasError = false;

private:
  Q_DISABLE_COPY(FileSaverBase)
};

class ORCA_UTILS_EXPORT FileSaver : public FileSaverBase {
  Q_DECLARE_TR_FUNCTIONS(Utils::FileUtils) // sic!

public:
  // QIODevice::WriteOnly is implicit
  explicit FileSaver(const FilePath &filePath, QIODevice::OpenMode mode = QIODevice::NotOpen);

  auto finalize() -> bool override;
  using FileSaverBase::finalize;

private:
  bool m_isSafe = false;
};

class ORCA_UTILS_EXPORT TempFileSaver : public FileSaverBase {
  Q_DECLARE_TR_FUNCTIONS(Utils::FileUtils) // sic!

public:
  explicit TempFileSaver(const QString &templ = QString());
  ~TempFileSaver() override;

  auto setAutoRemove(bool on) -> void { m_autoRemove = on; }

private:
  bool m_autoRemove = true;
};

ORCA_UTILS_EXPORT auto operator<<(QTextStream &s, const FilePath &fn) -> QTextStream&;

} // namespace Utils

