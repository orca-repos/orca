// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fileutils.hpp"
#include "utils_global.hpp"

#include <QVariant>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT PersistentSettingsReader {
public:
  PersistentSettingsReader();

  auto restoreValue(const QString &variable, const QVariant &defaultValue = QVariant()) const -> QVariant;
  auto restoreValues() const -> QVariantMap;
  auto load(const FilePath &fileName) -> bool;

private:
  QMap<QString, QVariant> m_valueMap;
};

class ORCA_UTILS_EXPORT PersistentSettingsWriter {
public:
  PersistentSettingsWriter(const FilePath &fileName, const QString &docType);

  auto save(const QVariantMap &data, QString *errorString) const -> bool;
  #ifdef QT_GUI_LIB
  auto save(const QVariantMap &data, QWidget *parent) const -> bool;
  #endif

  auto fileName() const -> FilePath;
  auto setContents(const QVariantMap &data) -> void;

private:
  auto write(const QVariantMap &data, QString *errorString) const -> bool;

  const FilePath m_fileName;
  const QString m_docType;
  mutable QMap<QString, QVariant> m_savedData;
};

} // namespace Utils
