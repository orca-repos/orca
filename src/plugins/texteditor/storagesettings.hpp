// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT StorageSettings {
public:
  StorageSettings();

  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, QSettings *s) -> void;
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;

  // calculated based on boolean setting plus file type blacklist examination
  auto removeTrailingWhitespace(const QString &filePattern) const -> bool;
  auto equals(const StorageSettings &ts) const -> bool;

  friend auto operator==(const StorageSettings &t1, const StorageSettings &t2) -> bool { return t1.equals(t2); }
  friend auto operator!=(const StorageSettings &t1, const StorageSettings &t2) -> bool { return !t1.equals(t2); }

  QString m_ignoreFileTypes;
  bool m_cleanWhitespace;
  bool m_inEntireDocument;
  bool m_addFinalNewLine;
  bool m_cleanIndentation;
  bool m_skipTrailingWhitespace;
};

} // namespace TextEditor
