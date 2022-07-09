// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/textindenter.hpp>

namespace TextEditor {
class ICodeStylePreferences;
}

namespace CppEditor {

class CppCodeStyleSettings;
class CppCodeStylePreferences;

namespace Internal {

class CppQtStyleIndenter : public TextEditor::TextIndenter {
public:
  explicit CppQtStyleIndenter(QTextDocument *doc);
  ~CppQtStyleIndenter() override;

  auto isElectricCharacter(const QChar &ch) const -> bool override;
  auto indentBlock(const QTextBlock &block, const QChar &typedChar, const TextEditor::TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void override;
  auto indent(const QTextCursor &cursor, const QChar &typedChar, const TextEditor::TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void override;
  auto setCodeStylePreferences(TextEditor::ICodeStylePreferences *preferences) -> void override;
  auto invalidateCache() -> void override;
  auto indentFor(const QTextBlock &block, const TextEditor::TabSettings &tabSettings, int cursorPositionInEditor = -1) -> int override;
  auto indentationForBlocks(const QVector<QTextBlock> &blocks, const TextEditor::TabSettings &tabSettings, int cursorPositionInEditor = -1) -> TextEditor::IndentationForBlock override;

private:
  auto codeStyleSettings() const -> CppCodeStyleSettings;
  CppCodeStylePreferences *m_cppCodeStylePreferences = nullptr;
};

} // namespace Internal
} // namespace CppEditor
