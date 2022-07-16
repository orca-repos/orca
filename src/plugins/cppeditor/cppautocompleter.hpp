// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/autocompleter.hpp>

#include <QObject>

namespace CppEditor {
namespace Internal {

class CppAutoCompleter : public TextEditor::AutoCompleter {
public:
  auto contextAllowsAutoBrackets(const QTextCursor &cursor, const QString &textToInsert = QString()) const -> bool override;
  auto contextAllowsAutoQuotes(const QTextCursor &cursor, const QString &textToInsert = QString()) const -> bool override;
  auto contextAllowsElectricCharacters(const QTextCursor &cursor) const -> bool override;
  auto isInComment(const QTextCursor &cursor) const -> bool override;
  auto isInString(const QTextCursor &cursor) const -> bool override;
  auto insertMatchingBrace(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString override;
  auto insertMatchingQuote(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString override;
  auto insertParagraphSeparator(const QTextCursor &cursor) const -> QString override;
};

#ifdef WITH_TESTS
namespace Tests {

class AutoCompleterTest : public QObject {
  Q_OBJECT

private slots:
  void testAutoComplete_data();
  void testAutoComplete();
  void testSurroundWithSelection_data();
  void testSurroundWithSelection();
  void testAutoBackspace_data();
  void testAutoBackspace();
  void testInsertParagraph_data();
  void testInsertParagraph();
};

} // namespace Tests
#endif // WITH_TESTS

} // Internal
} // CppEditor
