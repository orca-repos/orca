// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#ifndef CPPLOCALRENAMING
#define CPPLOCALRENAMING

#include <texteditor/texteditorconstants.hpp>

#include <QTextEdit>

namespace TextEditor {
class TextEditorWidget;
}

namespace CppEditor {
namespace Internal {

class CppLocalRenaming : public QObject {
  Q_OBJECT Q_DISABLE_COPY(CppLocalRenaming)

public:
  explicit CppLocalRenaming(TextEditor::TextEditorWidget *editorWidget);

  auto start() -> bool;
  auto isActive() const -> bool;
  auto stop() -> void;

  // Delegates for the editor widget
  auto handlePaste() -> bool;
  auto handleCut() -> bool;
  auto handleSelectAll() -> bool;

  // E.g. limit navigation keys to selection, stop() on Esc/Return or delegate
  // to BaseTextEditorWidget::keyPressEvent()
  auto handleKeyPressEvent(QKeyEvent *e) -> bool;
  auto encourageApply() -> bool;
  auto onContentsChangeOfEditorWidgetDocument(int position, int charsRemoved, int charsAdded) -> void;
  auto updateSelectionsForVariableUnderCursor(const QList<QTextEdit::ExtraSelection> &selections) -> void;
  auto isSameSelection(int cursorPosition) const -> bool;

signals:
  auto finished() -> void;
  auto processKeyPressNormally(QKeyEvent *e) -> void;

private:
  CppLocalRenaming();

  // The "rename selection" is the local use selection on which the user started the renaming
  auto findRenameSelection(int cursorPosition) -> bool;
  auto forgetRenamingSelection() -> void;
  static auto isWithinSelection(const QTextEdit::ExtraSelection &selection, int position) -> bool;
  auto isWithinRenameSelection(int position) -> bool;
  auto renameSelection() -> QTextEdit::ExtraSelection&;
  auto renameSelectionBegin() -> int { return renameSelection().cursor.selectionStart(); }
  auto renameSelectionEnd() -> int { return renameSelection().cursor.selectionEnd(); }
  auto updateRenamingSelectionCursor(const QTextCursor &cursor) -> void;
  auto updateRenamingSelectionFormat(const QTextCharFormat &format) -> void;
  auto changeOtherSelectionsText(const QString &text) -> void;
  auto startRenameChange() -> void;
  auto finishRenameChange() -> void;
  auto updateEditorWidgetWithSelections() -> void;
  auto textCharFormat(TextEditor::TextStyle category) const -> QTextCharFormat;
  
  TextEditor::TextEditorWidget *m_editorWidget;

  QList<QTextEdit::ExtraSelection> m_selections;
  int m_renameSelectionIndex;
  bool m_modifyingSelections;
  bool m_renameSelectionChanged;
  bool m_firstRenameChangeExpected;
};

} // namespace Internal
} // namespace CppEditor

#endif // CPPLOCALRENAMING
