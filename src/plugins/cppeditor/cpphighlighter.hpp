// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <texteditor/syntaxhighlighter.hpp>

#include <cplusplus/Token.h>

#include <QTextCharFormat>

namespace CppEditor {

class CPPEDITOR_EXPORT CppHighlighter : public TextEditor::SyntaxHighlighter {
  Q_OBJECT

public:
  CppHighlighter(QTextDocument *document = nullptr);

  auto setLanguageFeatures(const CPlusPlus::LanguageFeatures &languageFeatures) -> void;
  auto highlightBlock(const QString &text) -> void override;

private:
  auto highlightWord(QStringView word, int position, int length) -> void;
  auto highlightRawStringLiteral(QStringView text, const CPlusPlus::Token &tk) -> bool;
  auto highlightDoxygenComment(const QString &text, int position, int length) -> void;
  auto isPPKeyword(QStringView text) const -> bool;

  CPlusPlus::LanguageFeatures m_languageFeatures = CPlusPlus::LanguageFeatures::defaultFeatures();
};

} // namespace CppEditor
