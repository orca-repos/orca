// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fileutils.hpp"
#include "savefile.hpp"

#include "algorithm.hpp"
#include "commandline.hpp"
#include "environment.hpp"
#include "hostosinfo.hpp"
#include "qtcassert.hpp"

#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QOperatingSystemVersion>
#include <QTimer>
#include <QUrl>
#include <qplatformdefs.h>

#ifdef QT_GUI_LIB
#include <QMessageBox>
#endif

#ifdef Q_OS_WIN
#ifdef ORCA_PCH_H
#define CALLBACK WINAPI
#endif
#include <qt_windows.h>
#include <shlobj.h>
#endif

#ifdef Q_OS_OSX
#include "fileutils_mac.hpp"
#endif

namespace Utils {

// FileReader

auto FileReader::fetchQrc(const QString &fileName) -> QByteArray
{
  QTC_ASSERT(fileName.startsWith(':'), return QByteArray());
  QFile file(fileName);
  bool ok = file.open(QIODevice::ReadOnly);
  QTC_ASSERT(ok, qWarning() << fileName << "not there!"; return QByteArray());
  return file.readAll();
}

auto FileReader::fetch(const FilePath &filePath, QIODevice::OpenMode mode) -> bool
{
  QTC_ASSERT(!(mode & ~(QIODevice::ReadOnly | QIODevice::Text)), return false);

  if (filePath.needsDevice()) {
    // TODO: add error handling to FilePath::fileContents
    m_data = filePath.fileContents();
    return true;
  }

  QFile file(filePath.toString());
  if (!file.open(QIODevice::ReadOnly | mode)) {
    m_errorString = tr("Cannot open %1 for reading: %2").arg(filePath.toUserOutput(), file.errorString());
    return false;
  }
  m_data = file.readAll();
  if (file.error() != QFile::NoError) {
    m_errorString = tr("Cannot read %1: %2").arg(filePath.toUserOutput(), file.errorString());
    return false;
  }
  return true;
}

auto FileReader::fetch(const FilePath &filePath, QIODevice::OpenMode mode, QString *errorString) -> bool
{
  if (fetch(filePath, mode))
    return true;
  if (errorString)
    *errorString = m_errorString;
  return false;
}

#ifdef QT_GUI_LIB
auto FileReader::fetch(const FilePath &filePath, QIODevice::OpenMode mode, QWidget *parent) -> bool
{
  if (fetch(filePath, mode))
    return true;
  if (parent)
    QMessageBox::critical(parent, tr("File Error"), m_errorString);
  return false;
}
#endif // QT_GUI_LIB

// FileSaver

FileSaverBase::FileSaverBase() = default;

FileSaverBase::~FileSaverBase() = default;

auto FileSaverBase::finalize() -> bool
{
  m_file->close();
  setResult(m_file->error() == QFile::NoError);
  m_file.reset();
  return !m_hasError;
}

auto FileSaverBase::finalize(QString *errStr) -> bool
{
  if (finalize())
    return true;
  if (errStr)
    *errStr = errorString();
  return false;
}

#ifdef QT_GUI_LIB
auto FileSaverBase::finalize(QWidget *parent) -> bool
{
  if (finalize())
    return true;
  QMessageBox::critical(parent, tr("File Error"), errorString());
  return false;
}
#endif // QT_GUI_LIB

auto FileSaverBase::write(const char *data, int len) -> bool
{
  if (m_hasError)
    return false;
  return setResult(m_file->write(data, len) == len);
}

auto FileSaverBase::write(const QByteArray &bytes) -> bool
{
  if (m_hasError)
    return false;
  return setResult(m_file->write(bytes) == bytes.count());
}

auto FileSaverBase::setResult(bool ok) -> bool
{
  if (!ok && !m_hasError) {
    if (!m_file->errorString().isEmpty()) {
      m_errorString = tr("Cannot write file %1: %2").arg(m_filePath.toUserOutput(), m_file->errorString());
    } else {
      m_errorString = tr("Cannot write file %1. Disk full?").arg(m_filePath.toUserOutput());
    }
    m_hasError = true;
  }
  return ok;
}

auto FileSaverBase::setResult(QTextStream *stream) -> bool
{
  stream->flush();
  return setResult(stream->status() == QTextStream::Ok);
}

auto FileSaverBase::setResult(QDataStream *stream) -> bool
{
  return setResult(stream->status() == QDataStream::Ok);
}

auto FileSaverBase::setResult(QXmlStreamWriter *stream) -> bool
{
  return setResult(!stream->hasError());
}

// FileSaver

FileSaver::FileSaver(const FilePath &filePath, QIODevice::OpenMode mode)
{
  m_filePath = filePath;
  // Workaround an assert in Qt -- and provide a useful error message, too:
  if (m_filePath.osType() == OsType::OsTypeWindows) {
    // Taken from: https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247(v=vs.85).aspx
    static const QStringList reservedNames = {"CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};
    const QString fn = filePath.baseName().toUpper();
    if (reservedNames.contains(fn)) {
      m_errorString = tr("%1: Is a reserved filename on Windows. Cannot save.").arg(filePath.toUserOutput());
      m_hasError = true;
      return;
    }
  }
  if (filePath.needsDevice()) {
    // Write to a local temporary file first. Actual saving to the selected location
    // is done via m_filePath.writeFileContents() in finalize()
    m_isSafe = false;
    auto tf = new QTemporaryFile(QDir::tempPath() + "/remotefilesaver-XXXXXX");
    tf->setAutoRemove(false);
    m_file.reset(tf);
  } else if (mode & (QIODevice::ReadOnly | QIODevice::Append)) {
    m_file.reset(new QFile{filePath.path()});
    m_isSafe = false;
  } else {
    m_file.reset(new SaveFile{filePath.path()});
    m_isSafe = true;
  }
  if (!m_file->open(QIODevice::WriteOnly | mode)) {
    QString err = filePath.exists() ? tr("Cannot overwrite file %1: %2") : tr("Cannot create file %1: %2");
    m_errorString = err.arg(filePath.toUserOutput(), m_file->errorString());
    m_hasError = true;
  }
}

auto FileSaver::finalize() -> bool
{
  if (m_filePath.needsDevice()) {
    m_file->close();
    m_file->open(QIODevice::ReadOnly);
    const QByteArray data = m_file->readAll();
    const bool res = m_filePath.writeFileContents(data);
    m_file->remove();
    m_file.reset();
    return res;
  }

  if (!m_isSafe)
    return FileSaverBase::finalize();

  auto sf = static_cast<SaveFile*>(m_file.get());
  if (m_hasError) {
    if (sf->isOpen())
      sf->rollback();
  } else {
    setResult(sf->commit());
  }
  m_file.reset();
  return !m_hasError;
}

TempFileSaver::TempFileSaver(const QString &templ)
{
  m_file.reset(new QTemporaryFile{});
  auto tempFile = static_cast<QTemporaryFile*>(m_file.get());
  if (!templ.isEmpty())
    tempFile->setFileTemplate(templ);
  tempFile->setAutoRemove(false);
  if (!tempFile->open()) {
    m_errorString = tr("Cannot create temporary file in %1: %2").arg(QDir::toNativeSeparators(QFileInfo(tempFile->fileTemplate()).absolutePath()), tempFile->errorString());
    m_hasError = true;
  }
  m_filePath = FilePath::fromString(tempFile->fileName());
}

TempFileSaver::~TempFileSaver()
{
  m_file.reset();
  if (m_autoRemove)
    QFile::remove(m_filePath.toString());
}

#ifdef QT_GUI_LIB
FileUtils::CopyAskingForOverwrite::CopyAskingForOverwrite(QWidget *dialogParent, const std::function<void (FilePath)> &postOperation) : m_parent(dialogParent), m_postOperation(postOperation) {}

auto FileUtils::CopyAskingForOverwrite::operator()(const FilePath &src, const FilePath &dest, QString *error) -> bool
{
  bool copyFile = true;
  if (dest.exists()) {
    if (m_skipAll)
      copyFile = false;
    else if (!m_overwriteAll) {
      const int res = QMessageBox::question(m_parent, QCoreApplication::translate("Utils::FileUtils", "Overwrite File?"), QCoreApplication::translate("Utils::FileUtils", "Overwrite existing file \"%1\"?").arg(dest.toUserOutput()), QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel);
      if (res == QMessageBox::Cancel) {
        return false;
      } else if (res == QMessageBox::No) {
        copyFile = false;
      } else if (res == QMessageBox::NoToAll) {
        m_skipAll = true;
        copyFile = false;
      } else if (res == QMessageBox::YesToAll) {
        m_overwriteAll = true;
      }
      if (copyFile)
        dest.removeFile();
    }
  }
  if (copyFile) {
    dest.parentDir().ensureWritableDir();
    if (!src.copyFile(dest)) {
      if (error) {
        *error = QCoreApplication::translate("Utils::FileUtils", "Could not copy file \"%1\" to \"%2\".").arg(src.toUserOutput(), dest.toUserOutput());
      }
      return false;
    }
    if (m_postOperation)
      m_postOperation(dest);
  }
  m_files.append(dest.absoluteFilePath());
  return true;
}

auto FileUtils::CopyAskingForOverwrite::files() const -> FilePaths
{
  return m_files;
}
#endif // QT_GUI_LIB

// Copied from qfilesystemengine_win.cpp
#ifdef Q_OS_WIN

// File ID for Windows up to version 7.
static inline auto fileIdWin7(HANDLE handle) -> QByteArray
{
  BY_HANDLE_FILE_INFORMATION info;
  if (GetFileInformationByHandle(handle, &info)) {
    char buffer[sizeof "01234567:0123456701234567\0"];
    qsnprintf(buffer, sizeof(buffer), "%lx:%08lx%08lx", info.dwVolumeSerialNumber, info.nFileIndexHigh, info.nFileIndexLow);
    return QByteArray(buffer);
  }
  return QByteArray();
}

// File ID for Windows starting from version 8.
static auto fileIdWin8(HANDLE handle) -> QByteArray
{
  QByteArray result;
  FILE_ID_INFO infoEx;
  if (GetFileInformationByHandleEx(handle, static_cast<FILE_INFO_BY_HANDLE_CLASS>(18), // FileIdInfo in Windows 8
                                   &infoEx, sizeof(FILE_ID_INFO))) {
    result = QByteArray::number(infoEx.VolumeSerialNumber, 16);
    result += ':';
    // Note: MinGW-64's definition of FILE_ID_128 differs from the MSVC one.
    result += QByteArray(reinterpret_cast<const char*>(&infoEx.FileId), int(sizeof(infoEx.FileId))).toHex();
  }
  return result;
}

static auto fileIdWin(HANDLE fHandle) -> QByteArray
{
  return QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows8 ? fileIdWin8(HANDLE(fHandle)) : fileIdWin7(HANDLE(fHandle));
}
#endif

auto FileUtils::fileId(const FilePath &fileName) -> QByteArray
{
  QByteArray result;

  #ifdef Q_OS_WIN
  const HANDLE handle = CreateFile((wchar_t*)fileName.toUserOutput().utf16(), 0, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if (handle != INVALID_HANDLE_VALUE) {
    result = fileIdWin(handle);
    CloseHandle(handle);
  }
  #else // Copied from qfilesystemengine_unix.cpp
    if (Q_UNLIKELY(fileName.isEmpty()))
        return result;

    QT_STATBUF statResult;
    if (QT_STAT(fileName.toString().toLocal8Bit().constData(), &statResult))
        return result;
    result = QByteArray::number(quint64(statResult.st_dev), 16);
    result += ':';
    result += QByteArray::number(quint64(statResult.st_ino));
  #endif
  return result;
}

#ifdef Q_OS_WIN

template <>
auto withNtfsPermissions(const std::function<void()> &task) -> void
{
  qt_ntfs_permission_lookup++;
  task();
  qt_ntfs_permission_lookup--;
}
#endif

#ifdef QT_WIDGETS_LIB

static std::function<QWidget *()> s_dialogParentGetter;

auto FileUtils::setDialogParentGetter(const std::function<QWidget *()> &getter) -> void
{
  s_dialogParentGetter = getter;
}

static auto dialogParent(QWidget *parent) -> QWidget*
{
  return parent ? parent : s_dialogParentGetter ? s_dialogParentGetter() : nullptr;
}

auto FileUtils::getOpenFilePath(QWidget *parent, const QString &caption, const FilePath &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options) -> FilePath
{
  const QString result = QFileDialog::getOpenFileName(dialogParent(parent), caption, dir.toString(), filter, selectedFilter, options);
  return FilePath::fromString(result);
}

auto FileUtils::getSaveFilePath(QWidget *parent, const QString &caption, const FilePath &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options) -> FilePath
{
  const QString result = QFileDialog::getSaveFileName(dialogParent(parent), caption, dir.toString(), filter, selectedFilter, options);
  return FilePath::fromString(result);
}

auto FileUtils::getExistingDirectory(QWidget *parent, const QString &caption, const FilePath &dir, QFileDialog::Options options) -> FilePath
{
  const QString result = QFileDialog::getExistingDirectory(dialogParent(parent), caption, dir.toString(), options);
  return FilePath::fromString(result);
}

auto FileUtils::getOpenFilePaths(QWidget *parent, const QString &caption, const FilePath &dir, const QString &filter, QString *selectedFilter, QFileDialog::Options options) -> FilePaths
{
  const QStringList result = QFileDialog::getOpenFileNames(dialogParent(parent), caption, dir.toString(), filter, selectedFilter, options);
  return transform(result, &FilePath::fromString);
}

#endif // QT_WIDGETS_LIB

} // namespace Utils
