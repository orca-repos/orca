// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include <texteditor/autocompleter.hpp>

namespace CMakeProjectManager {
namespace Internal {

class CMAKE_EXPORT CMakeAutoCompleter : public TextEditor::AutoCompleter {
public:
  CMakeAutoCompleter();

  auto isInComment(const QTextCursor &cursor) const -> bool override;
  auto isInString(const QTextCursor &cursor) const -> bool override;
  auto insertMatchingBrace(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString override;
  auto insertMatchingQuote(const QTextCursor &cursor, const QString &text, QChar lookAhead, bool skipChars, int *skippedChars) const -> QString override;
  auto paragraphSeparatorAboutToBeInserted(QTextCursor &cursor) -> int override;
  auto contextAllowsAutoBrackets(const QTextCursor &cursor, const QString &textToInsert) const -> bool override;
  auto contextAllowsAutoQuotes(const QTextCursor &cursor, const QString &textToInsert) const -> bool override;
  auto contextAllowsElectricCharacters(const QTextCursor &cursor) const -> bool override;
};

} // namespace Internal
} // namespace CMakeProjectManager
