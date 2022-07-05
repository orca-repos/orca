// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/stringutils.h>

#include <QObject>

namespace Core {
namespace Internal {

class UtilsJsExtension : public QObject {
  Q_OBJECT

public:
  UtilsJsExtension(QObject *parent = nullptr) : QObject(parent) { }

  // General information
  auto qtVersion() const -> Q_INVOKABLE QString;
  auto orcaVersion() const -> Q_INVOKABLE QString;

  // File name conversions:
  auto toNativeSeparators(const QString &in) const -> Q_INVOKABLE QString;
  auto fromNativeSeparators(const QString &in) const -> Q_INVOKABLE QString;
  auto baseName(const QString &in) const -> Q_INVOKABLE QString;
  auto fileName(const QString &in) const -> Q_INVOKABLE QString;
  auto completeBaseName(const QString &in) const -> Q_INVOKABLE QString;
  auto suffix(const QString &in) const -> Q_INVOKABLE QString;
  auto completeSuffix(const QString &in) const -> Q_INVOKABLE QString;
  auto path(const QString &in) const -> Q_INVOKABLE QString;
  auto absoluteFilePath(const QString &in) const -> Q_INVOKABLE QString;
  auto relativeFilePath(const QString &path, const QString &base) const -> Q_INVOKABLE QString;

  // File checks:
  auto exists(const QString &in) const ->Q_INVOKABLE bool;
  auto isDirectory(const QString &in) const ->Q_INVOKABLE bool;
  auto isFile(const QString &in) const ->Q_INVOKABLE bool;

  // MimeDB:
  auto preferredSuffix(const QString &mimetype) const -> Q_INVOKABLE QString;

  // Generate filename:
  auto fileName(const QString &path, const QString &extension) const -> Q_INVOKABLE QString;

  // Generate temporary file:
  auto mktemp(const QString &pattern) const -> Q_INVOKABLE QString;

  // Generate a ascii-only string:
  auto asciify(const QString &input) const -> Q_INVOKABLE QString;
};

} // namespace Internal
} // namespace Core
