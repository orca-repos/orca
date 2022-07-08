// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"
#include "tabsettings.hpp"

#include <QString>

QT_BEGIN_NAMESPACE
class QTextBlock;
class QTextCursor;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT AutoCompleter {
public:
  AutoCompleter();
  virtual ~AutoCompleter();

  auto setAutoInsertBracketsEnabled(bool b) -> void { m_autoInsertBrackets = b; }
  auto isAutoInsertBracketsEnabled() const -> bool { return m_autoInsertBrackets; }
  auto setSurroundWithBracketsEnabled(bool b) -> void { m_surroundWithBrackets = b; }
  auto isSurroundWithBracketsEnabled() const -> bool { return m_surroundWithBrackets; }
  auto setAutoInsertQuotesEnabled(bool b) -> void { m_autoInsertQuotes = b; }
  auto isAutoInsertQuotesEnabled() const -> bool { return m_autoInsertQuotes; }
  auto setSurroundWithQuotesEnabled(bool b) -> void { m_surroundWithQuotes = b; }
  auto isSurroundWithQuotesEnabled() const -> bool { return m_surroundWithQuotes; }
  auto setOverwriteClosingCharsEnabled(bool b) -> void { m_overwriteClosingChars = b; }
  auto isOverwriteClosingCharsEnabled() const -> bool { return m_overwriteClosingChars; }
  auto setTabSettings(const TabSettings &tabSettings) -> void { m_tabSettings = tabSettings; }
  auto tabSettings() const -> const TabSettings& { return m_tabSettings; }

  // Returns the text to complete at the cursor position, or an empty string
  virtual auto autoComplete(QTextCursor &cursor, const QString &text, bool skipChars) const -> QString;
  // Handles backspace. When returning true, backspace processing is stopped
  virtual auto autoBackspace(QTextCursor &cursor) -> bool;
  // Hook to insert special characters on enter. Returns the number of extra blocks inserted.
  virtual auto paragraphSeparatorAboutToBeInserted(QTextCursor &cursor) -> int;
  virtual auto contextAllowsAutoBrackets(const QTextCursor &cursor, const QString &textToInsert = QString()) const -> bool;
  virtual auto contextAllowsAutoQuotes(const QTextCursor &cursor, const QString &textToInsert = QString()) const -> bool;
  virtual auto contextAllowsElectricCharacters(const QTextCursor &cursor) const -> bool;
  // Returns true if the cursor is inside a comment.
  virtual auto isInComment(const QTextCursor &cursor) const -> bool;
  // Returns true if the cursor is inside a string.
  virtual auto isInString(const QTextCursor &cursor) const -> bool;
  virtual auto insertMatchingBrace(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString;
  virtual auto insertMatchingQuote(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString;
  // Returns the text that needs to be inserted
  virtual auto insertParagraphSeparator(const QTextCursor &cursor) const -> QString;
  static auto isQuote(const QString &text) -> bool;
  auto isNextBlockIndented(const QTextBlock &currentBlock) const -> bool;

private:
  auto replaceSelection(QTextCursor &cursor, const QString &textToInsert) const -> QString;

  TabSettings m_tabSettings;
  mutable bool m_allowSkippingOfBlockEnd;
  bool m_autoInsertBrackets;
  bool m_surroundWithBrackets;
  bool m_autoInsertQuotes;
  bool m_surroundWithQuotes;
  bool m_overwriteClosingChars;
};

} // TextEditor
