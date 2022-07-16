// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textdocument.hpp"

#include "extraencodingsettings.hpp"
#include "fontsettings.hpp"
#include "textindenter.hpp"
#include "storagesettings.hpp"
#include "syntaxhighlighter.hpp"
#include "tabsettings.hpp"
#include "textdocumentlayout.hpp"
#include "texteditor.hpp"
#include "texteditorconstants.hpp"
#include "typingsettings.hpp"
#include "refactoringchanges.hpp"

#include <core/core-interface.hpp>
#include <core/core-progress-manager.hpp>
#include <core/core-diff-service.hpp>
#include <core/core-editor-manager.hpp>
#include <core/core-document-model.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <utils/textutils.hpp>
#include <utils/guard.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>

#include <QAction>
#include <QApplication>
#include <QFutureInterface>
#include <QScrollBar>
#include <QTextCodec>


using namespace Orca::Plugin::Core;
using namespace Utils;

/*!
    \class TextEditor::BaseTextDocument
    \brief The BaseTextDocument class is the base class for QTextDocument based documents.

    It is the base class for documents used by implementations of the BaseTextEditor class,
    and contains basic functions for retrieving text content and markers (like bookmarks).

    Subclasses of BaseTextEditor can either use BaseTextDocument as is (and this is the default),
    or created subclasses of BaseTextDocument if they have special requirements.
*/

namespace TextEditor {

class TextDocumentPrivate {
public:
  TextDocumentPrivate() : m_indenter(new TextIndenter(&m_document)) { }

  auto indentOrUnindent(const MultiTextCursor &cursor, bool doIndent, const TabSettings &tabSettings) -> MultiTextCursor;
  auto resetRevisions() -> void;
  auto updateRevisions() -> void;

public:
  FilePath m_defaultPath;
  QString m_suggestedFileName;
  TypingSettings m_typingSettings;
  StorageSettings m_storageSettings;
  TabSettings m_tabSettings;
  ExtraEncodingSettings m_extraEncodingSettings;
  FontSettings m_fontSettings;
  bool m_fontSettingsNeedsApply = false; // for applying font settings delayed till an editor becomes visible
  QTextDocument m_document;
  SyntaxHighlighter *m_highlighter = nullptr;
  CompletionAssistProvider *m_completionAssistProvider = nullptr;
  CompletionAssistProvider *m_functionHintAssistProvider = nullptr;
  IAssistProvider *m_quickFixProvider = nullptr;
  QScopedPointer<Indenter> m_indenter;
  QScopedPointer<Formatter> m_formatter;

  int m_autoSaveRevision = -1;
  bool m_silentReload = false;

  TextMarks m_marksCache; // Marks not owned
  Guard m_modificationChangedGuard;
};

auto TextDocumentPrivate::indentOrUnindent(const MultiTextCursor &cursors, bool doIndent, const TabSettings &tabSettings) -> MultiTextCursor
{
  MultiTextCursor result;
  auto first = true;
  for (const auto &textCursor : cursors) {
    auto cursor = textCursor;
    if (first) {
      cursor.beginEditBlock();
      first = false;
    } else {
      cursor.joinPreviousEditBlock();
    }

    // Indent or unindent the selected lines
    auto pos = cursor.position();
    const auto column = tabSettings.columnAt(cursor.block().text(), cursor.positionInBlock());
    auto anchor = cursor.anchor();
    const auto start = qMin(anchor, pos);
    const auto end = qMax(anchor, pos);

    auto startBlock = m_document.findBlock(start);
    auto endBlock = m_document.findBlock(qMax(end - 1, 0)).next();
    const auto cursorAtBlockStart = cursor.position() == startBlock.position();
    const auto anchorAtBlockStart = cursor.anchor() == startBlock.position();
    const auto oneLinePartial = startBlock.next() == endBlock && (start > startBlock.position() || end < endBlock.position() - 1) && !cursors.hasMultipleCursors();

    // Make sure one line selection will get processed in "for" loop
    if (startBlock == endBlock)
      endBlock = endBlock.next();

    if (cursor.hasSelection()) {
      if (oneLinePartial) {
        cursor.removeSelectedText();
      } else {
        for (auto block = startBlock; block != endBlock; block = block.next()) {
          const auto text = block.text();
          auto indentPosition = tabSettings.lineIndentPosition(text);
          if (!doIndent && !indentPosition)
            indentPosition = TabSettings::firstNonSpace(text);
          const auto targetColumn = tabSettings.indentedColumn(tabSettings.columnAt(text, indentPosition), doIndent);
          cursor.setPosition(block.position() + indentPosition);
          cursor.insertText(tabSettings.indentationString(0, targetColumn, 0, block));
          cursor.setPosition(block.position());
          cursor.setPosition(block.position() + indentPosition, QTextCursor::KeepAnchor);
          cursor.removeSelectedText();
        }
        // make sure that selection that begins in first column stays at first column
        // even if we insert text at first column
        cursor = textCursor;
        if (cursorAtBlockStart) {
          cursor.setPosition(startBlock.position(), QTextCursor::KeepAnchor);
        } else if (anchorAtBlockStart) {
          cursor.setPosition(startBlock.position(), QTextCursor::MoveAnchor);
          cursor.setPosition(textCursor.position(), QTextCursor::KeepAnchor);
        }
      }
    } else {
      auto text = startBlock.text();
      const auto indentPosition = tabSettings.positionAtColumn(text, column, nullptr, true);
      const auto spaces = tabSettings.spacesLeftFromPosition(text, indentPosition);
      const auto startColumn = tabSettings.columnAt(text, indentPosition - spaces);
      const auto targetColumn = tabSettings.indentedColumn(tabSettings.columnAt(text, indentPosition), doIndent);
      cursor.setPosition(startBlock.position() + indentPosition);
      cursor.setPosition(startBlock.position() + indentPosition - spaces, QTextCursor::KeepAnchor);
      cursor.removeSelectedText();
      cursor.insertText(tabSettings.indentationString(startColumn, targetColumn, 0, startBlock));
    }

    cursor.endEditBlock();
    result.addCursor(cursor);
  }

  return result;
}

auto TextDocumentPrivate::resetRevisions() -> void
{
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(m_document.documentLayout());
  QTC_ASSERT(documentLayout, return);
  documentLayout->lastSaveRevision = m_document.revision();

  for (auto block = m_document.begin(); block.isValid(); block = block.next())
    block.setRevision(documentLayout->lastSaveRevision);
}

auto TextDocumentPrivate::updateRevisions() -> void
{
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(m_document.documentLayout());
  QTC_ASSERT(documentLayout, return);
  const auto oldLastSaveRevision = documentLayout->lastSaveRevision;
  documentLayout->lastSaveRevision = m_document.revision();

  if (oldLastSaveRevision != documentLayout->lastSaveRevision) {
    for (auto block = m_document.begin(); block.isValid(); block = block.next()) {
      if (block.revision() < 0 || block.revision() != oldLastSaveRevision)
        block.setRevision(-documentLayout->lastSaveRevision - 1);
      else
        block.setRevision(documentLayout->lastSaveRevision);
    }
  }
}

///////////////////////////////////////////////////////////////////////
//
// BaseTextDocument
//
///////////////////////////////////////////////////////////////////////

TextDocument::TextDocument(Id id) : d(new TextDocumentPrivate)
{
  connect(&d->m_document, &QTextDocument::modificationChanged, this, &TextDocument::modificationChanged);
  connect(&d->m_document, &QTextDocument::contentsChanged, this, &IDocument::contentsChanged);
  connect(&d->m_document, &QTextDocument::contentsChange, this, &TextDocument::contentsChangedWithPosition);

  // set new document layout
  auto opt = d->m_document.defaultTextOption();
  opt.setTextDirection(Qt::LeftToRight);
  opt.setFlags(opt.flags() | QTextOption::IncludeTrailingSpaces | QTextOption::AddSpaceForLineAndParagraphSeparators);
  d->m_document.setDefaultTextOption(opt);
  d->m_document.setDocumentLayout(new TextDocumentLayout(&d->m_document));

  if (id.isValid())
    setId(id);

  setSuspendAllowed(true);
}

TextDocument::~TextDocument()
{
  delete d;
}

auto TextDocument::openedTextDocumentContents() -> QMap<QString, QString>
{
  QMap<QString, QString> workingCopy;
  foreach(IDocument *document, DocumentModel::openedDocuments()) {
    auto textEditorDocument = qobject_cast<TextDocument*>(document);
    if (!textEditorDocument)
      continue;
    QString fileName = textEditorDocument->filePath().toString();
    workingCopy[fileName] = textEditorDocument->plainText();
  }
  return workingCopy;
}

auto TextDocument::openedTextDocumentEncodings() -> QMap<QString, QTextCodec*>
{
  QMap<QString, QTextCodec*> workingCopy;
  foreach(IDocument *document, DocumentModel::openedDocuments()) {
    auto textEditorDocument = qobject_cast<TextDocument*>(document);
    if (!textEditorDocument)
      continue;
    QString fileName = textEditorDocument->filePath().toString();
    workingCopy[fileName] = const_cast<QTextCodec*>(textEditorDocument->codec());
  }
  return workingCopy;
}

auto TextDocument::currentTextDocument() -> TextDocument*
{
  return qobject_cast<TextDocument*>(EditorManager::currentDocument());
}

auto TextDocument::textDocumentForFilePath(const FilePath &filePath) -> TextDocument*
{
  return qobject_cast<TextDocument*>(DocumentModel::documentForFilePath(filePath));
}

auto TextDocument::plainText() const -> QString
{
  return document()->toPlainText();
}

auto TextDocument::textAt(int pos, int length) const -> QString
{
  return Text::textAt(QTextCursor(document()), pos, length);
}

auto TextDocument::characterAt(int pos) const -> QChar
{
  return document()->characterAt(pos);
}

auto TextDocument::setTypingSettings(const TypingSettings &typingSettings) -> void
{
  d->m_typingSettings = typingSettings;
}

auto TextDocument::setStorageSettings(const StorageSettings &storageSettings) -> void
{
  d->m_storageSettings = storageSettings;
}

auto TextDocument::typingSettings() const -> const TypingSettings&
{
  return d->m_typingSettings;
}

auto TextDocument::storageSettings() const -> const StorageSettings&
{
  return d->m_storageSettings;
}

auto TextDocument::setTabSettings(const TabSettings &newTabSettings) -> void
{
  if (newTabSettings == d->m_tabSettings)
    return;
  d->m_tabSettings = newTabSettings;

  emit tabSettingsChanged();
}

auto TextDocument::tabSettings() const -> TabSettings
{
  return d->m_tabSettings;
}

auto TextDocument::setFontSettings(const FontSettings &fontSettings) -> void
{
  if (fontSettings == d->m_fontSettings)
    return;
  d->m_fontSettings = fontSettings;
  d->m_fontSettingsNeedsApply = true;
  emit fontSettingsChanged();
}

auto TextDocument::createDiffAgainstCurrentFileAction(QObject *parent, const std::function<FilePath()> &filePath) -> QAction*
{
  const auto diffAgainstCurrentFile = [filePath]() {
    auto diffService = DiffService::instance();
    auto textDocument = currentTextDocument();
    const QString leftFilePath = textDocument ? textDocument->filePath().toString() : QString();
    const auto rightFilePath = filePath().toString();
    if (diffService && !leftFilePath.isEmpty() && !rightFilePath.isEmpty())
      diffService->diffFiles(leftFilePath, rightFilePath);
  };
  const auto diffAction = new QAction(tr("Diff Against Current File"), parent);
  QObject::connect(diffAction, &QAction::triggered, parent, diffAgainstCurrentFile);
  return diffAction;
}

#ifdef WITH_TESTS
void TextDocument::setSilentReload()
{
    d->m_silentReload = true;
}
#endif

auto TextDocument::triggerPendingUpdates() -> void
{
  if (d->m_fontSettingsNeedsApply)
    applyFontSettings();
}

auto TextDocument::setCompletionAssistProvider(CompletionAssistProvider *provider) -> void
{
  d->m_completionAssistProvider = provider;
}

auto TextDocument::completionAssistProvider() const -> CompletionAssistProvider*
{
  return d->m_completionAssistProvider;
}

auto TextDocument::setFunctionHintAssistProvider(CompletionAssistProvider *provider) -> void
{
  d->m_functionHintAssistProvider = provider;
}

auto TextDocument::functionHintAssistProvider() const -> CompletionAssistProvider*
{
  return d->m_functionHintAssistProvider;
}

auto TextDocument::setQuickFixAssistProvider(IAssistProvider *provider) const -> void
{
  d->m_quickFixProvider = provider;
}

auto TextDocument::quickFixAssistProvider() const -> IAssistProvider*
{
  return d->m_quickFixProvider;
}

auto TextDocument::applyFontSettings() -> void
{
  d->m_fontSettingsNeedsApply = false;
  if (d->m_highlighter) {
    d->m_highlighter->setFontSettings(d->m_fontSettings);
    d->m_highlighter->rehighlight();
  }
}

auto TextDocument::fontSettings() const -> const FontSettings&
{
  return d->m_fontSettings;
}

auto TextDocument::setExtraEncodingSettings(const ExtraEncodingSettings &extraEncodingSettings) -> void
{
  d->m_extraEncodingSettings = extraEncodingSettings;
}

auto TextDocument::autoIndent(const QTextCursor &cursor, QChar typedChar, int currentCursorPosition) -> void
{
  d->m_indenter->indent(cursor, typedChar, tabSettings(), currentCursorPosition);
}

auto TextDocument::autoReindent(const QTextCursor &cursor, int currentCursorPosition) -> void
{
  d->m_indenter->reindent(cursor, tabSettings(), currentCursorPosition);
}

auto TextDocument::autoFormatOrIndent(const QTextCursor &cursor) -> void
{
  d->m_indenter->autoIndent(cursor, tabSettings());
}

auto TextDocument::indent(const MultiTextCursor &cursor) -> MultiTextCursor
{
  return d->indentOrUnindent(cursor, true, tabSettings());
}

auto TextDocument::unindent(const MultiTextCursor &cursor) -> MultiTextCursor
{
  return d->indentOrUnindent(cursor, false, tabSettings());
}

auto TextDocument::setFormatter(Formatter *formatter) -> void
{
  d->m_formatter.reset(formatter);
}

auto TextDocument::autoFormat(const QTextCursor &cursor) -> void
{
  using namespace Text;
  if (!d->m_formatter)
    return;
  if (auto watcher = d->m_formatter->format(cursor, tabSettings())) {
    connect(watcher, &QFutureWatcher<ChangeSet>::finished, this, [this, watcher]() {
      if (!watcher->isCanceled())
        applyChangeSet(watcher->result());
      delete watcher;
    });
  }
}

auto TextDocument::applyChangeSet(const ChangeSet &changeSet) -> bool
{
  if (changeSet.isEmpty())
    return true;
  RefactoringChanges changes;
  const RefactoringFilePtr file = changes.file(filePath());
  file->setChangeSet(changeSet);
  return file->apply();
}

// the blocks list must be sorted
auto TextDocument::setIfdefedOutBlocks(const QList<BlockRange> &blocks) -> void
{
  const auto doc = document();
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
  QTC_ASSERT(documentLayout, return);

  auto needUpdate = false;

  auto block = doc->firstBlock();

  auto rangeNumber = 0;
  auto braceDepthDelta = 0;
  while (block.isValid()) {
    auto cleared = false;
    auto set = false;
    if (rangeNumber < blocks.size()) {
      const auto &range = blocks.at(rangeNumber);
      if (block.position() >= range.first() && (block.position() + block.length() - 1 <= range.last() || !range.last()))
        set = TextDocumentLayout::setIfdefedOut(block);
      else
        cleared = TextDocumentLayout::clearIfdefedOut(block);
      if (block.contains(range.last()))
        ++rangeNumber;
    } else {
      cleared = TextDocumentLayout::clearIfdefedOut(block);
    }

    if (cleared || set) {
      needUpdate = true;
      const auto delta = TextDocumentLayout::braceDepthDelta(block);
      if (cleared)
        braceDepthDelta += delta;
      else if (set)
        braceDepthDelta -= delta;
    }

    if (braceDepthDelta) {
      TextDocumentLayout::changeBraceDepth(block, braceDepthDelta);
      TextDocumentLayout::changeFoldingIndent(block, braceDepthDelta); // ### C++ only, refactor!
    }

    block = block.next();
  }

  if (needUpdate)
    documentLayout->requestUpdate();

  #ifdef WITH_TESTS
    emit ifdefedOutBlocksChanged(blocks);
  #endif
}

auto TextDocument::extraEncodingSettings() const -> const ExtraEncodingSettings&
{
  return d->m_extraEncodingSettings;
}

auto TextDocument::setIndenter(Indenter *indenter) -> void
{
  // clear out existing code formatter data
  for (auto it = document()->begin(); it.isValid(); it = it.next()) {
    const auto userData = TextDocumentLayout::textUserData(it);
    if (userData)
      userData->setCodeFormatterData(nullptr);
  }
  d->m_indenter.reset(indenter);
}

auto TextDocument::indenter() const -> Indenter*
{
  return d->m_indenter.data();
}

auto TextDocument::isSaveAsAllowed() const -> bool
{
  return true;
}

auto TextDocument::fallbackSaveAsPath() const -> FilePath
{
  return d->m_defaultPath;
}

auto TextDocument::fallbackSaveAsFileName() const -> QString
{
  return d->m_suggestedFileName;
}

auto TextDocument::setFallbackSaveAsPath(const FilePath &defaultPath) -> void
{
  d->m_defaultPath = defaultPath;
}

auto TextDocument::setFallbackSaveAsFileName(const QString &suggestedFileName) -> void
{
  d->m_suggestedFileName = suggestedFileName;
}

auto TextDocument::document() const -> QTextDocument*
{
  return &d->m_document;
}

auto TextDocument::syntaxHighlighter() const -> SyntaxHighlighter*
{
  return d->m_highlighter;
}

/*!
 * Saves the document to the file specified by \a fileName. If errors occur,
 * \a errorString contains their cause.
 * \a autoSave returns whether this function was called by the automatic save routine.
 * If \a autoSave is true, the cursor will be restored and some signals suppressed
 * and we do not clean up the text file (cleanWhitespace(), ensureFinalNewLine()).
 */
auto TextDocument::save(QString *errorString, const FilePath &filePath, bool autoSave) -> bool
{
  QTextCursor cursor(&d->m_document);

  // When autosaving, we don't want to modify the document/location under the user's fingers.
  TextEditorWidget *editorWidget = nullptr;
  auto savedPosition = 0;
  auto savedAnchor = 0;
  auto savedVScrollBarValue = 0;
  auto savedHScrollBarValue = 0;
  const auto undos = d->m_document.availableUndoSteps();

  // When saving the current editor, make sure to maintain the cursor and scroll bar
  // positions for undo
  if (const auto editor = BaseTextEditor::currentTextEditor()) {
    if (editor->document() == this) {
      editorWidget = editor->editorWidget();
      const auto cur = editor->textCursor();
      savedPosition = cur.position();
      savedAnchor = cur.anchor();
      savedVScrollBarValue = editorWidget->verticalScrollBar()->value();
      savedHScrollBarValue = editorWidget->horizontalScrollBar()->value();
      cursor.setPosition(cur.position());
    }
  }

  if (!autoSave) {
    cursor.beginEditBlock();
    cursor.movePosition(QTextCursor::Start);

    if (d->m_storageSettings.m_cleanWhitespace) {
      cleanWhitespace(cursor, d->m_storageSettings.m_inEntireDocument, d->m_storageSettings.m_cleanIndentation);
    }
    if (d->m_storageSettings.m_addFinalNewLine)
      ensureFinalNewLine(cursor);
    cursor.endEditBlock();
  }

  const FilePath &savePath = filePath.isEmpty() ? this->filePath() : filePath;

  // check if UTF8-BOM has to be added or removed
  TextFileFormat saveFormat = format();
  if (saveFormat.codec->name() == "UTF-8" && supportsUtf8Bom()) {
    switch (d->m_extraEncodingSettings.m_utf8BomSetting) {
    case ExtraEncodingSettings::AlwaysAdd:
      saveFormat.hasUtf8Bom = true;
      break;
    case ExtraEncodingSettings::OnlyKeep:
      break;
    case ExtraEncodingSettings::AlwaysDelete:
      saveFormat.hasUtf8Bom = false;
      break;
    }
  }

  const bool ok = write(savePath, saveFormat, d->m_document.toPlainText(), errorString);

  // restore text cursor and scroll bar positions
  if (autoSave && undos < d->m_document.availableUndoSteps()) {
    d->m_document.undo();
    if (editorWidget) {
      auto cur = editorWidget->textCursor();
      cur.setPosition(savedAnchor);
      cur.setPosition(savedPosition, QTextCursor::KeepAnchor);
      editorWidget->verticalScrollBar()->setValue(savedVScrollBarValue);
      editorWidget->horizontalScrollBar()->setValue(savedHScrollBarValue);
      editorWidget->setTextCursor(cur);
    }
  }

  if (!ok)
    return false;
  d->m_autoSaveRevision = d->m_document.revision();
  if (autoSave)
    return true;

  // inform about the new filename
  d->m_document.setModified(false); // also triggers update of the block revisions
  setFilePath(savePath.absoluteFilePath());
  emit changed();
  return true;
}

auto TextDocument::contents() const -> QByteArray
{
  return plainText().toUtf8();
}

auto TextDocument::setContents(const QByteArray &contents) -> bool
{
  return setPlainText(QString::fromUtf8(contents));
}

auto TextDocument::shouldAutoSave() const -> bool
{
  return d->m_autoSaveRevision != d->m_document.revision();
}

auto TextDocument::setFilePath(const FilePath &newName) -> void
{
  if (newName == filePath())
    return;
  IDocument::setFilePath(newName.absoluteFilePath().cleanPath());
}

auto TextDocument::reloadBehavior(ChangeTrigger state, ChangeType type) const -> IDocument::ReloadBehavior
{
  if (d->m_silentReload)
    return IDocument::BehaviorSilent;
  return BaseTextDocument::reloadBehavior(state, type);
}

auto TextDocument::isModified() const -> bool
{
  return d->m_document.isModified();
}

auto TextDocument::open(QString *errorString, const FilePath &filePath, const FilePath &realFilePath) -> IDocument::OpenResult
{
  emit aboutToOpen(filePath, realFilePath);
  OpenResult success = openImpl(errorString, filePath, realFilePath, /*reload =*/ false);
  if (success == OpenResult::Success) {
    setMimeType(mimeTypeForFile(filePath).name());
    emit openFinishedSuccessfully();
  }
  return success;
}

auto TextDocument::openImpl(QString *errorString, const FilePath &filePath, const FilePath &realFilePath, bool reload) -> IDocument::OpenResult
{
  QStringList content;

  ReadResult readResult = TextFileFormat::ReadIOError;

  if (!filePath.isEmpty()) {
    readResult = read(realFilePath, &content, errorString);
    const int chunks = content.size();

    // Don't call setUndoRedoEnabled(true) when reload is true and filenames are different,
    // since it will reset the undo's clear index
    if (!reload || filePath == realFilePath)
      d->m_document.setUndoRedoEnabled(reload);

    QTextCursor c(&d->m_document);
    c.beginEditBlock();
    if (reload) {
      c.select(QTextCursor::Document);
      c.removeSelectedText();
    } else {
      d->m_document.clear();
    }

    if (chunks == 1) {
      c.insertText(content.at(0));
    } else if (chunks > 1) {
      QFutureInterface<void> interface;
      interface.setProgressRange(0, chunks);
      ProgressManager::addTask(interface.future(), tr("Opening File"), Constants::TASK_OPEN_FILE);
      interface.reportStarted();

      for (auto i = 0; i < chunks; ++i) {
        c.insertText(content.at(i));
        interface.setProgressValue(i + 1);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
      }

      interface.reportFinished();
    }

    c.endEditBlock();

    // Don't call setUndoRedoEnabled(true) when reload is true and filenames are different,
    // since it will reset the undo's clear index
    if (!reload || filePath == realFilePath)
      d->m_document.setUndoRedoEnabled(true);

    const auto documentLayout = qobject_cast<TextDocumentLayout*>(d->m_document.documentLayout());
    QTC_ASSERT(documentLayout, return OpenResult::CannotHandle);
    documentLayout->lastSaveRevision = d->m_autoSaveRevision = d->m_document.revision();
    d->updateRevisions();
    d->m_document.setModified(filePath != realFilePath);
    setFilePath(filePath);
  }
  if (readResult == TextFileFormat::ReadIOError)
    return OpenResult::ReadError;
  return OpenResult::Success;
}

auto TextDocument::reload(QString *errorString, QTextCodec *codec) -> bool
{
  QTC_ASSERT(codec, return false);
  setCodec(codec);
  return reload(errorString);
}

auto TextDocument::reload(QString *errorString) -> bool
{
  return reload(errorString, filePath());
}

auto TextDocument::reload(QString *errorString, const FilePath &realFilePath) -> bool
{
  emit aboutToReload();
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(d->m_document.documentLayout());
  TextMarks marks;
  if (documentLayout)
    marks = documentLayout->documentClosing(); // removes text marks non-permanently

  bool success = openImpl(errorString, filePath(), realFilePath, /*reload =*/true) == OpenResult::Success;

  if (documentLayout)
    documentLayout->documentReloaded(marks, this); // re-adds text marks
  emit reloadFinished(success);
  return success;
}

auto TextDocument::setPlainText(const QString &text) -> bool
{
  if (text.size() > EditorManager::maxTextFileSize()) {
    document()->setPlainText(TextEditorWidget::msgTextTooLarge(text.size()));
    d->resetRevisions();
    document()->setModified(false);
    return false;
  }
  document()->setPlainText(text);
  d->resetRevisions();
  document()->setModified(false);
  return true;
}

auto TextDocument::reload(QString *errorString, ReloadFlag flag, ChangeType type) -> bool
{
  if (flag == FlagIgnore) {
    if (type != TypeContents)
      return true;

    const auto wasModified = document()->isModified();
    {
      GuardLocker locker(d->m_modificationChangedGuard);
      // hack to ensure we clean the clear state in QTextDocument
      document()->setModified(false);
      document()->setModified(true);
    }
    if (!wasModified)
      modificationChanged(true);
    return true;
  }
  return reload(errorString);
}

auto TextDocument::setSyntaxHighlighter(SyntaxHighlighter *highlighter) -> void
{
  if (d->m_highlighter)
    delete d->m_highlighter;
  d->m_highlighter = highlighter;
  d->m_highlighter->setParent(this);
  d->m_highlighter->setDocument(&d->m_document);
}

auto TextDocument::cleanWhitespace(const QTextCursor &cursor) -> void
{
  const auto hasSelection = cursor.hasSelection();
  auto copyCursor = cursor;
  copyCursor.setVisualNavigation(false);
  copyCursor.beginEditBlock();

  cleanWhitespace(copyCursor, true, true);

  if (!hasSelection)
    ensureFinalNewLine(copyCursor);

  copyCursor.endEditBlock();
}

auto TextDocument::cleanWhitespace(QTextCursor &cursor, bool inEntireDocument, bool cleanIndentation) -> void
{
  const auto removeTrailingWhitespace = d->m_storageSettings.removeTrailingWhitespace(filePath().fileName());

  const auto documentLayout = qobject_cast<TextDocumentLayout*>(d->m_document.documentLayout());
  Q_ASSERT(cursor.visualNavigation() == false);

  auto block = d->m_document.findBlock(cursor.selectionStart());
  QTextBlock end;
  if (cursor.hasSelection())
    end = d->m_document.findBlock(cursor.selectionEnd() - 1).next();

  QVector<QTextBlock> blocks;
  while (block.isValid() && block != end) {
    if (inEntireDocument || block.revision() != documentLayout->lastSaveRevision) {
      blocks.append(block);
    }
    block = block.next();
  }
  if (blocks.isEmpty())
    return;

  const auto currentTabSettings = tabSettings();
  const auto &indentations = d->m_indenter->indentationForBlocks(blocks, currentTabSettings);

  foreach(block, blocks) {
    auto blockText = block.text();

    if (removeTrailingWhitespace)
      TabSettings::removeTrailingWhitespace(cursor, block);

    const auto indent = indentations[block.blockNumber()];
    if (cleanIndentation && !currentTabSettings.isIndentationClean(block, indent)) {
      cursor.setPosition(block.position());
      const auto firstNonSpace = TabSettings::firstNonSpace(blockText);
      if (firstNonSpace == blockText.length()) {
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
      } else {
        const auto column = currentTabSettings.columnAt(blockText, firstNonSpace);
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, firstNonSpace);
        auto indentationString = currentTabSettings.indentationString(0, column, column - indent, block);
        cursor.insertText(indentationString);
      }
    }
  }
}

auto TextDocument::ensureFinalNewLine(QTextCursor &cursor) -> void
{
  if (!d->m_storageSettings.m_addFinalNewLine)
    return;

  cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
  const auto emptyFile = !cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);

  if (!emptyFile && cursor.selectedText().at(0) != QChar::ParagraphSeparator) {
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    cursor.insertText(QLatin1String("\n"));
  }
}

auto TextDocument::modificationChanged(bool modified) -> void
{
  if (d->m_modificationChangedGuard.isLocked())
    return;
  // we only want to update the block revisions when going back to the saved version,
  // e.g. with undo
  if (!modified)
    d->updateRevisions();
  emit changed();
}

auto TextDocument::updateLayout() const -> void
{
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(d->m_document.documentLayout());
  QTC_ASSERT(documentLayout, return);
  documentLayout->requestUpdate();
}

auto TextDocument::marks() const -> TextMarks
{
  return d->m_marksCache;
}

auto TextDocument::addMark(TextMark *mark) -> bool
{
  if (mark->baseTextDocument())
    return false;
  QTC_ASSERT(mark->lineNumber() >= 1, return false);
  const auto blockNumber = mark->lineNumber() - 1;
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(d->m_document.documentLayout());
  QTC_ASSERT(documentLayout, return false);
  const auto block = d->m_document.findBlockByNumber(blockNumber);

  if (block.isValid()) {
    const auto userData = TextDocumentLayout::userData(block);
    userData->addMark(mark);
    d->m_marksCache.append(mark);
    mark->updateLineNumber(blockNumber + 1);
    QTC_CHECK(mark->lineNumber() == blockNumber + 1); // Checks that the base class is called
    mark->updateBlock(block);
    mark->setBaseTextDocument(this);
    if (!mark->isVisible())
      return true;
    // Update document layout
    const auto newMaxWidthFactor = qMax(mark->widthFactor(), documentLayout->maxMarkWidthFactor);
    const auto fullUpdate = newMaxWidthFactor > documentLayout->maxMarkWidthFactor || !documentLayout->hasMarks;
    documentLayout->hasMarks = true;
    documentLayout->maxMarkWidthFactor = newMaxWidthFactor;
    if (fullUpdate)
      documentLayout->requestUpdate();
    else
      documentLayout->requestExtraAreaUpdate();
    return true;
  }
  return false;
}

auto TextDocument::marksAt(int line) const -> TextMarks
{
  QTC_ASSERT(line >= 1, return TextMarks());
  const auto blockNumber = line - 1;
  const auto block = d->m_document.findBlockByNumber(blockNumber);

  if (block.isValid()) {
    if (const auto userData = TextDocumentLayout::textUserData(block))
      return userData->marks();
  }
  return TextMarks();
}

auto TextDocument::removeMarkFromMarksCache(TextMark *mark) -> void
{
  auto documentLayout = qobject_cast<TextDocumentLayout*>(d->m_document.documentLayout());
  QTC_ASSERT(documentLayout, return);
  d->m_marksCache.removeAll(mark);

  auto scheduleLayoutUpdate = [documentLayout]() {
    // make sure all destructors that may directly or indirectly call this function are
    // completed before updating.
    QMetaObject::invokeMethod(documentLayout, &QPlainTextDocumentLayout::requestUpdate, Qt::QueuedConnection);
  };

  if (d->m_marksCache.isEmpty()) {
    documentLayout->hasMarks = false;
    documentLayout->maxMarkWidthFactor = 1.0;
    scheduleLayoutUpdate();
    return;
  }

  if (!mark->isVisible())
    return;

  if (documentLayout->maxMarkWidthFactor == 1.0 || mark->widthFactor() == 1.0 || mark->widthFactor() < documentLayout->maxMarkWidthFactor) {
    // No change in width possible
    documentLayout->requestExtraAreaUpdate();
  } else {
    auto maxWidthFactor = 1.0;
    foreach(const TextMark *mark, marks()) {
      if (!mark->isVisible())
        continue;
      maxWidthFactor = qMax(mark->widthFactor(), maxWidthFactor);
      if (maxWidthFactor == documentLayout->maxMarkWidthFactor)
        break; // Still a mark with the maxMarkWidthFactor
    }

    if (maxWidthFactor != documentLayout->maxMarkWidthFactor) {
      documentLayout->maxMarkWidthFactor = maxWidthFactor;
      scheduleLayoutUpdate();
    } else {
      documentLayout->requestExtraAreaUpdate();
    }
  }
}

auto TextDocument::removeMark(TextMark *mark) -> void
{
  const auto block = d->m_document.findBlockByNumber(mark->lineNumber() - 1);
  if (const auto data = static_cast<TextBlockUserData*>(block.userData())) {
    if (!data->removeMark(mark))
      qDebug() << "Could not find mark" << mark << "on line" << mark->lineNumber();
  }

  removeMarkFromMarksCache(mark);
  emit markRemoved(mark);
  mark->setBaseTextDocument(nullptr);
  updateLayout();
}

auto TextDocument::updateMark(TextMark *mark) -> void
{
  const auto block = d->m_document.findBlockByNumber(mark->lineNumber() - 1);
  if (block.isValid()) {
    const auto userData = TextDocumentLayout::userData(block);
    // re-evaluate priority
    userData->removeMark(mark);
    userData->addMark(mark);
  }
  updateLayout();
}

auto TextDocument::moveMark(TextMark *mark, int previousLine) -> void
{
  const auto block = d->m_document.findBlockByNumber(previousLine - 1);
  if (const auto data = TextDocumentLayout::textUserData(block)) {
    if (!data->removeMark(mark))
      qDebug() << "Could not find mark" << mark << "on line" << previousLine;
  }
  removeMarkFromMarksCache(mark);
  mark->setBaseTextDocument(nullptr);
  addMark(mark);
}

} // namespace TextEditor
