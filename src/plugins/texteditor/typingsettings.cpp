// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "typingsettings.hpp"

#include <utils/settingsutils.hpp>

#include <QTextCursor>
#include <QTextDocument>

static constexpr char autoIndentKey[] = "AutoIndent";
static constexpr char tabKeyBehaviorKey[] = "TabKeyBehavior";
static constexpr char smartBackspaceBehaviorKey[] = "SmartBackspaceBehavior";
static constexpr char preferSingleLineCommentsKey[] = "PreferSingleLineComments";
static constexpr char groupPostfix[] = "TypingSettings";

namespace TextEditor {

TypingSettings::TypingSettings(): m_autoIndent(true), m_tabKeyBehavior(TabNeverIndents), m_smartBackspaceBehavior(BackspaceNeverIndents), m_preferSingleLineComments(false) {}

auto TypingSettings::toSettings(const QString &category, QSettings *s) const -> void
{
  Utils::toSettings(QLatin1String(groupPostfix), category, s, this);
}

auto TypingSettings::fromSettings(const QString &category, QSettings *s) -> void
{
  *this = TypingSettings(); // Assign defaults
  Utils::fromSettings(QLatin1String(groupPostfix), category, s, this);
}

auto TypingSettings::toMap() const -> QVariantMap
{
  return {{autoIndentKey, m_autoIndent}, {tabKeyBehaviorKey, m_tabKeyBehavior}, {smartBackspaceBehaviorKey, m_smartBackspaceBehavior}, {preferSingleLineCommentsKey, m_preferSingleLineComments}};
}

auto TypingSettings::fromMap(const QVariantMap &map) -> void
{
  m_autoIndent = map.value(autoIndentKey, m_autoIndent).toBool();
  m_tabKeyBehavior = (TabKeyBehavior)map.value(tabKeyBehaviorKey, m_tabKeyBehavior).toInt();
  m_smartBackspaceBehavior = (SmartBackspaceBehavior)map.value(smartBackspaceBehaviorKey, m_smartBackspaceBehavior).toInt();
  m_preferSingleLineComments = map.value(preferSingleLineCommentsKey, m_preferSingleLineComments).toBool();
}

auto TypingSettings::equals(const TypingSettings &ts) const -> bool
{
  return m_autoIndent == ts.m_autoIndent && m_tabKeyBehavior == ts.m_tabKeyBehavior && m_smartBackspaceBehavior == ts.m_smartBackspaceBehavior && m_preferSingleLineComments == ts.m_preferSingleLineComments;
}

auto TypingSettings::tabShouldIndent(const QTextDocument *document, const QTextCursor &cursor, int *suggestedPosition) const -> bool
{
  if (m_tabKeyBehavior == TabNeverIndents)
    return false;
  auto tc = cursor;
  if (suggestedPosition)
    *suggestedPosition = tc.position(); // At least suggest original position
  tc.movePosition(QTextCursor::StartOfLine);
  if (tc.atBlockEnd()) // cursor was on a blank line
    return true;
  if (document->characterAt(tc.position()).isSpace()) {
    tc.movePosition(QTextCursor::WordRight);
    if (tc.positionInBlock() >= cursor.positionInBlock()) {
      if (suggestedPosition)
        *suggestedPosition = tc.position(); // Suggest position after whitespace
      if (m_tabKeyBehavior == TabLeadingWhitespaceIndents)
        return true;
    }
  }
  return m_tabKeyBehavior == TabAlwaysIndents;
}

} // namespace TextEditor
