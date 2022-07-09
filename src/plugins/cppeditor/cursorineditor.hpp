// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/fileutils.hpp>

#include <QTextCursor>

namespace TextEditor {
class TextDocument;
}

namespace CppEditor {

class CppEditorWidget;

class CursorInEditor {
public:
  CursorInEditor(const QTextCursor &cursor, const Utils::FilePath &filePath, CppEditorWidget *editorWidget = nullptr, TextEditor::TextDocument *textDocument = nullptr) : m_cursor(cursor), m_filePath(filePath), m_editorWidget(editorWidget), m_textDocument(textDocument) {}

  auto editorWidget() const -> CppEditorWidget* { return m_editorWidget; }
  auto textDocument() const -> TextEditor::TextDocument* { return m_textDocument; }
  auto cursor() const -> const QTextCursor& { return m_cursor; }
  auto filePath() const -> const Utils::FilePath& { return m_filePath; }

private:
  QTextCursor m_cursor;
  Utils::FilePath m_filePath;
  CppEditorWidget *m_editorWidget = nullptr;
  TextEditor::TextDocument *const m_textDocument;
};

} // namespace CppEditor
