// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QStringList>

QT_BEGIN_NAMESPACE
class QTextCodec;
class QByteArray;
QT_END_NAMESPACE

namespace Utils {

class FilePath;

class ORCA_UTILS_EXPORT TextFileFormat {
public:
  enum LineTerminationMode {
    LFLineTerminator,
    CRLFLineTerminator,
    NativeLineTerminator =
    #if defined (Q_OS_WIN)
    CRLFLineTerminator,
    #else
        LFLineTerminator
    #endif
  };

  enum ReadResult {
    ReadSuccess,
    ReadEncodingError,
    ReadMemoryAllocationError,
    ReadIOError
  };

  TextFileFormat();

  static auto detect(const QByteArray &data) -> TextFileFormat;

  auto decode(const QByteArray &data, QString *target) const -> bool;
  auto decode(const QByteArray &data, QStringList *target) const -> bool;

  static auto readFile(const FilePath &filePath, const QTextCodec *defaultCodec, QStringList *plainText, TextFileFormat *format, QString *errorString, QByteArray *decodingErrorSample = nullptr) -> ReadResult;
  static auto readFile(const FilePath &filePath, const QTextCodec *defaultCodec, QString *plainText, TextFileFormat *format, QString *errorString, QByteArray *decodingErrorSample = nullptr) -> ReadResult;
  static auto readFileUTF8(const FilePath &filePath, const QTextCodec *defaultCodec, QByteArray *plainText, QString *errorString) -> ReadResult;

  auto writeFile(const FilePath &filePath, QString plainText, QString *errorString) const -> bool;
  static auto decodingErrorSample(const QByteArray &data) -> QByteArray;

  LineTerminationMode lineTerminationMode = NativeLineTerminator;
  bool hasUtf8Bom = false;
  const QTextCodec *codec = nullptr;
};

} // namespace Utils
