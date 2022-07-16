// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "refactoringchanges.hpp"
#include "texteditor.hpp"
#include "textdocument.hpp"

#include <core/core-interface.hpp>
#include <core/core-readonly-files-dialog.hpp>
#include <core/core-document-manager.hpp>
#include <core/core-editor-manager.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>

#include <QFileInfo>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QDebug>
#include <QApplication>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace TextEditor {

RefactoringChanges::RefactoringChanges() : m_data(new RefactoringChangesData) {}
RefactoringChanges::RefactoringChanges(RefactoringChangesData *data) : m_data(data) {}
RefactoringChanges::~RefactoringChanges() = default;

auto RefactoringChanges::rangesToSelections(QTextDocument *document, const QList<Range> &ranges) -> RefactoringSelections
{
  RefactoringSelections selections;

  foreach(const Range &range, ranges) {
    QTextCursor start(document);
    start.setPosition(range.start);
    start.setKeepPositionOnInsert(true);
    QTextCursor end(document);
    end.setPosition(qMin(range.end, document->characterCount() - 1));

    selections.append(qMakePair(start, end));
  }

  return selections;
}

auto RefactoringChanges::createFile(const FilePath &filePath, const QString &contents, bool reindent, bool openEditor) const -> bool
{
  if (filePath.exists())
    return false;

  // Create a text document for the new file:
  const auto document = new QTextDocument;
  QTextCursor cursor(document);
  cursor.beginEditBlock();
  cursor.insertText(contents);

  // Reindent the contents:
  if (reindent) {
    cursor.select(QTextCursor::Document);
    m_data->indentSelection(cursor, filePath, nullptr);
  }
  cursor.endEditBlock();

  // Write the file to disk:
  TextFileFormat format;
  format.codec = EditorManager::defaultTextCodec();
  QString error;
  const auto saveOk = format.writeFile(filePath, document->toPlainText(), &error);
  delete document;
  if (!saveOk)
    return false;

  m_data->fileChanged(filePath);

  if (openEditor)
    RefactoringChanges::openEditor(filePath, /*bool activate =*/ false, -1, -1);

  return true;
}

auto RefactoringChanges::removeFile(const FilePath &filePath) const -> bool
{
  if (!filePath.exists())
    return false;

  // ### implement!
  qWarning() << "RefactoringChanges::removeFile is not implemented";
  return true;
}

auto RefactoringChanges::openEditor(const FilePath &filePath, bool activate, int line, int column) -> TextEditorWidget*
{
  EditorManager::OpenEditorFlags flags = EditorManager::IgnoreNavigationHistory;
  if (activate)
    flags |= EditorManager::SwitchSplitIfAlreadyVisible;
  else
    flags |= EditorManager::DoNotChangeCurrentEditor;
  if (line != -1) {
    // openEditorAt uses a 1-based line and a 0-based column!
    column -= 1;
  }
  const IEditor *editor = EditorManager::openEditorAt(Link{filePath, line, column}, Id(), flags);

  if (editor)
    return TextEditorWidget::fromEditor(editor);
  return nullptr;
}

auto RefactoringChanges::file(TextEditorWidget *editor) -> RefactoringFilePtr
{
  return RefactoringFilePtr(new RefactoringFile(editor));
}

auto RefactoringChanges::file(const FilePath &filePath) const -> RefactoringFilePtr
{
  return RefactoringFilePtr(new RefactoringFile(filePath, m_data));
}

RefactoringFile::RefactoringFile(QTextDocument *document, const FilePath &filePath) : m_filePath(filePath), m_document(document) { }

RefactoringFile::RefactoringFile(TextEditorWidget *editor) : m_filePath(editor->textDocument()->filePath()), m_editor(editor) { }

RefactoringFile::RefactoringFile(const FilePath &filePath, const QSharedPointer<RefactoringChangesData> &data) : m_filePath(filePath), m_data(data)
{
  QList<IEditor*> editors = DocumentModel::editorsForFilePath(filePath);
  if (!editors.isEmpty()) {
    auto editorWidget = TextEditorWidget::fromEditor(editors.first());
    if (editorWidget && !editorWidget->isReadOnly())
      m_editor = editorWidget;
  }
}

RefactoringFile::~RefactoringFile()
{
  delete m_document;
}

auto RefactoringFile::isValid() const -> bool
{
  if (m_filePath.isEmpty())
    return false;
  return document();
}

auto RefactoringFile::document() const -> const QTextDocument*
{
  return mutableDocument();
}

auto RefactoringFile::mutableDocument() const -> QTextDocument*
{
  if (m_editor)
    return m_editor->document();
  if (!m_document) {
    QString fileContents;
    if (!m_filePath.isEmpty()) {
      QString error;
      const QTextCodec *defaultCodec = EditorManager::defaultTextCodec();
      TextFileFormat::ReadResult result = TextFileFormat::readFile(m_filePath, defaultCodec, &fileContents, &m_textFileFormat, &error);
      if (result != TextFileFormat::ReadSuccess) {
        qWarning() << "Could not read " << m_filePath << ". Error: " << error;
        m_textFileFormat.codec = nullptr;
      }
    }
    // always make a QTextDocument to avoid excessive null checks
    m_document = new QTextDocument(fileContents);
  }
  return m_document;
}

auto RefactoringFile::cursor() const -> const QTextCursor
{
  if (m_editor)
    return m_editor->textCursor();
  if (!m_filePath.isEmpty()) {
    if (const auto doc = mutableDocument())
      return QTextCursor(doc);
  }

  return QTextCursor();
}

auto RefactoringFile::filePath() const -> FilePath
{
  return m_filePath;
}

auto RefactoringFile::editor() const -> TextEditorWidget*
{
  return m_editor;
}

auto RefactoringFile::position(int line, int column) const -> int
{
  QTC_ASSERT(line != 0, return -1);
  QTC_ASSERT(column != 0, return -1);
  if (const auto doc = document())
    return doc->findBlockByNumber(line - 1).position() + column - 1;
  return -1;
}

auto RefactoringFile::lineAndColumn(int offset, int *line, int *column) const -> void
{
  QTC_ASSERT(line, return);
  QTC_ASSERT(column, return);
  QTC_ASSERT(offset >= 0, return);
  auto c(cursor());
  c.setPosition(offset);
  *line = c.blockNumber() + 1;
  *column = c.positionInBlock() + 1;
}

auto RefactoringFile::charAt(int pos) const -> QChar
{
  if (const auto doc = document())
    return doc->characterAt(pos);
  return QChar();
}

auto RefactoringFile::textOf(int start, int end) const -> QString
{
  auto c = cursor();
  c.setPosition(start);
  c.setPosition(end, QTextCursor::KeepAnchor);
  return c.selectedText();
}

auto RefactoringFile::textOf(const Range &range) const -> QString
{
  return textOf(range.start, range.end);
}

auto RefactoringFile::changeSet() const -> ChangeSet
{
  return m_changes;
}

auto RefactoringFile::setChangeSet(const ChangeSet &changeSet) -> void
{
  if (m_filePath.isEmpty())
    return;

  m_changes = changeSet;
}

auto RefactoringFile::appendIndentRange(const Range &range) -> void
{
  if (m_filePath.isEmpty())
    return;

  m_indentRanges.append(range);
}

auto RefactoringFile::appendReindentRange(const Range &range) -> void
{
  if (m_filePath.isEmpty())
    return;

  m_reindentRanges.append(range);
}

auto RefactoringFile::setOpenEditor(bool activate, int pos) -> void
{
  m_openEditor = true;
  m_activateEditor = activate;
  m_editorCursorPosition = pos;
}

auto RefactoringFile::apply() -> bool
{
  // test file permissions
  if (!m_filePath.toFileInfo().isWritable()) {
    ReadOnlyFilesDialog roDialog(m_filePath, ICore::dialogParent());
    const auto &failDetailText = QApplication::translate("RefactoringFile::apply", "Refactoring cannot be applied.");
    roDialog.setShowFailWarning(true, failDetailText);
    if (roDialog.exec() == ReadOnlyFilesDialog::RO_Cancel)
      return false;
  }

  // open / activate / goto position
  auto ensureCursorVisible = false;
  if (m_openEditor && !m_filePath.isEmpty()) {
    auto line = -1, column = -1;
    if (m_editorCursorPosition != -1) {
      lineAndColumn(m_editorCursorPosition, &line, &column);
      ensureCursorVisible = true;
    }
    m_editor = RefactoringChanges::openEditor(m_filePath, m_activateEditor, line, column);
    m_openEditor = false;
    m_activateEditor = false;
    m_editorCursorPosition = -1;
  }

  const auto withUnmodifiedEditor = m_editor && !m_editor->textDocument()->isModified();
  auto result = true;

  // apply changes, if any
  if (m_data && !(m_indentRanges.isEmpty() && m_changes.isEmpty())) {
    const auto doc = mutableDocument();
    if (doc) {
      auto c = cursor();
      if (m_appliedOnce)
        c.joinPreviousEditBlock();
      else
        c.beginEditBlock();

      sort(m_indentRanges);
      sort(m_reindentRanges);

      // build indent selections now, applying the changeset will change locations
      const auto &indentSelections = RefactoringChanges::rangesToSelections(doc, m_indentRanges);
      m_indentRanges.clear();
      const auto &reindentSelections = RefactoringChanges::rangesToSelections(doc, m_reindentRanges);
      m_reindentRanges.clear();

      // apply changes and reindent
      m_changes.apply(&c);
      m_changes.clear();

      indentOrReindent(indentSelections, Indent);
      indentOrReindent(reindentSelections, Reindent);

      c.endEditBlock();

      // if this document doesn't have an editor, write the result to a file
      if (!m_editor && m_textFileFormat.codec) {
        QTC_ASSERT(!m_filePath.isEmpty(), return false);
        QString error;
        // suppress "file has changed" warnings if the file is open in a read-only editor
        FileChangeBlocker block(m_filePath);
        if (!m_textFileFormat.writeFile(m_filePath, doc->toPlainText(), &error)) {
          qWarning() << "Could not apply changes to" << m_filePath << ". Error: " << error;
          result = false;
        }
      }

      fileChanged();
      if (withUnmodifiedEditor && EditorManager::autoSaveAfterRefactoring())
        m_editor->textDocument()->save(nullptr, m_filePath, false);
    }
  }

  if (m_editor && ensureCursorVisible)
    m_editor->ensureCursorVisible();

  m_appliedOnce = true;
  return result;
}

auto RefactoringFile::indentOrReindent(const RefactoringSelections &ranges, IndentType indent) -> void
{
  const auto document = m_editor ? m_editor->textDocument() : nullptr;
  for (const auto &[position, anchor] : ranges) {
    auto selection(anchor);
    selection.setPosition(position.position(), QTextCursor::KeepAnchor);
    if (indent == Indent)
      m_data->indentSelection(selection, m_filePath, document);
    else
      m_data->reindentSelection(selection, m_filePath, document);
  }
}

auto RefactoringFile::fileChanged() -> void
{
  if (!m_filePath.isEmpty())
    m_data->fileChanged(m_filePath);
}

RefactoringChangesData::~RefactoringChangesData() = default;

auto RefactoringChangesData::indentSelection(const QTextCursor &, const FilePath &, const TextDocument *) const -> void
{
  qWarning() << Q_FUNC_INFO << "not implemented";
}

auto RefactoringChangesData::reindentSelection(const QTextCursor &, const FilePath &, const TextDocument *) const -> void
{
  qWarning() << Q_FUNC_INFO << "not implemented";
}

auto RefactoringChangesData::fileChanged(const FilePath &) -> void {}

} // namespace TextEditor
