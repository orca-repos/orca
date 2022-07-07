// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once
#include "utils_global.hpp"

#include <QMap>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

QT_FORWARD_DECLARE_CLASS(QLocale)

namespace Utils {

namespace Internal {
class QrcParserPrivate;
class QrcCachePrivate;
}

class ORCA_UTILS_EXPORT QrcParser {
public:
  using Ptr = QSharedPointer<QrcParser>;
  using ConstPtr = QSharedPointer<const QrcParser>;

  ~QrcParser();

  auto parseFile(const QString &path, const QString &contents) -> bool;
  auto firstFileAtPath(const QString &path, const QLocale &locale) const -> QString;
  auto collectFilesAtPath(const QString &path, QStringList *res, const QLocale *locale = nullptr) const -> void;
  auto hasDirAtPath(const QString &path, const QLocale *locale = nullptr) const -> bool;
  auto collectFilesInPath(const QString &path, QMap<QString, QStringList> *res, bool addDirs = false, const QLocale *locale = nullptr) const -> void;
  auto collectResourceFilesForSourceFile(const QString &sourceFile, QStringList *results, const QLocale *locale = nullptr) const -> void;
  auto errorMessages() const -> QStringList;
  auto languages() const -> QStringList;
  auto isValid() const -> bool;

  static auto parseQrcFile(const QString &path, const QString &contents) -> Ptr;
  static auto normalizedQrcFilePath(const QString &path) -> QString;
  static auto normalizedQrcDirectoryPath(const QString &path) -> QString;
  static auto qrcDirectoryPathForQrcFilePath(const QString &file) -> QString;

private:
  QrcParser();
  QrcParser(const QrcParser &);
  Internal::QrcParserPrivate *d;
};

class ORCA_UTILS_EXPORT QrcCache {
public:
  QrcCache();
  ~QrcCache();

  auto addPath(const QString &path, const QString &contents) -> QrcParser::ConstPtr;
  auto removePath(const QString &path) -> void;
  auto updatePath(const QString &path, const QString &contents) -> QrcParser::ConstPtr;
  auto parsedPath(const QString &path) -> QrcParser::ConstPtr;
  auto clear() -> void;

private:
  Internal::QrcCachePrivate *d;
};

}
