// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

QT_BEGIN_NAMESPACE
class QSettings;
class QTextDocument;
class QTextCursor;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT TypingSettings {
public:
  // This enum must match the indexes of tabKeyBehavior widget
  enum TabKeyBehavior {
    TabNeverIndents = 0,
    TabAlwaysIndents = 1,
    TabLeadingWhitespaceIndents = 2
  };

  // This enum must match the indexes of smartBackspaceBehavior widget
  enum SmartBackspaceBehavior {
    BackspaceNeverIndents = 0,
    BackspaceFollowsPreviousIndents = 1,
    BackspaceUnindents = 2
  };

  TypingSettings();

  auto tabShouldIndent(const QTextDocument *document, const QTextCursor &cursor, int *suggestedPosition) const -> bool;
  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, QSettings *s) -> void;
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;
  auto equals(const TypingSettings &ts) const -> bool;

  friend auto operator==(const TypingSettings &t1, const TypingSettings &t2) -> bool { return t1.equals(t2); }
  friend auto operator!=(const TypingSettings &t1, const TypingSettings &t2) -> bool { return !t1.equals(t2); }

  bool m_autoIndent;
  TabKeyBehavior m_tabKeyBehavior;
  SmartBackspaceBehavior m_smartBackspaceBehavior;
  bool m_preferSingleLineComments;
};

} // namespace TextEditor

Q_DECLARE_METATYPE(TextEditor::TypingSettings)
