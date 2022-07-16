// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "indenter.hpp"
#include "refactoroverlay.hpp"
#include "core/core-highlight-scroll-bar-controller.hpp"

#include <texteditor/codeassist/assistenums.hpp>
#include <texteditor/snippets/snippetparser.hpp>

#include <core/core-editor-manager.hpp>
#include <core/core-editor-interface.hpp>
#include <core/core-editor-factory-interface.hpp>
#include <core/core-help-item.hpp>

#include <utils/elidinglabel.hpp>
#include <utils/link.hpp>
#include <utils/multitextcursor.hpp>
#include <utils/porting.hpp>
#include <utils/uncommentselection.hpp>

#include <QPlainTextEdit>
#include <QSharedPointer>
#include <functional>

QT_BEGIN_NAMESPACE
class QToolBar;
class QPrinter;
class QMenu;
class QPainter;
class QPoint;
class QRect;
class QTextBlock;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {
class HighlightScrollBarController;
}

namespace TextEditor {
class TextDocument;
class TextMark;
class BaseHoverHandler;
class RefactorOverlay;
class SyntaxHighlighter;
class AssistInterface;
class IAssistProvider;
class ICodeStylePreferences;
class CompletionAssistProvider;
using RefactorMarkers = QList<RefactorMarker>;
using TextMarks = QList<TextMark *>;

namespace Internal {
class BaseTextEditorPrivate;
class TextEditorFactoryPrivate;
class TextEditorWidgetPrivate;
class TextEditorOverlay;
}

class AutoCompleter;
class BaseTextEditor;
class TextEditorFactory;
class TextEditorWidget;
class PlainTextEditorFactory;

class BehaviorSettings;
class CompletionSettings;
class DisplaySettings;
class ExtraEncodingSettings;
class FontSettings;
class MarginSettings;
class StorageSettings;
class TypingSettings;

enum TextMarkRequestKind {
  BreakpointRequest,
  BookmarkRequest,
  TaskMarkRequest
};

class TEXTEDITOR_EXPORT BaseTextEditor : public Orca::Plugin::Core::IEditor {
  Q_OBJECT

public:
  BaseTextEditor();
  ~BaseTextEditor() override;

  virtual auto finalizeInitialization() -> void {}
  static auto currentTextEditor() -> BaseTextEditor*;
  static auto textEditorsForDocument(TextDocument *textDocument) -> QVector<BaseTextEditor*>;
  auto editorWidget() const -> TextEditorWidget*;
  auto textDocument() const -> TextDocument*;
  // Some convenience text access
  auto setTextCursor(const QTextCursor &cursor) -> void;
  auto textCursor() const -> QTextCursor;
  auto characterAt(int pos) const -> QChar;
  auto textAt(int from, int to) const -> QString;
  auto addContext(Utils::Id id) -> void;

  // IEditor
  auto document() const -> Orca::Plugin::Core::IDocument* override;
  auto duplicate() -> IEditor* override;
  auto saveState() const -> QByteArray override;
  auto restoreState(const QByteArray &state) -> void override;
  auto toolBar() -> QWidget* override;
  auto contextHelp(const HelpCallback &callback) const -> void override; // from IContext
  auto setContextHelp(const Orca::Plugin::Core::HelpItem &item) -> void override;
  auto currentLine() const -> int override;
  auto currentColumn() const -> int override;
  auto gotoLine(int line, int column = 0, bool centerLine = true) -> void override;

  /*! Returns the amount of visible columns (in characters) in the editor */
  auto columnCount() const -> int;
  /*! Returns the amount of visible lines (in characters) in the editor */
  auto rowCount() const -> int;
  /*! Returns the position at \a posOp in characters from the beginning of the document */
  virtual auto position(TextPositionOperation posOp = CurrentPosition, int at = -1) const -> int;
  /*! Converts the \a pos in characters from beginning of document to \a line and \a column */
  virtual auto convertPosition(int pos, int *line, int *column) const -> void;
  virtual auto selectedText() const -> QString;
  /*! Removes \a length characters to the right of the cursor. */
  virtual auto remove(int length) -> void;
  /*! Inserts the given string to the right of the cursor. */
  virtual auto insert(const QString &string) -> void;
  /*! Replaces \a length characters to the right of the cursor with the given string. */
  virtual auto replace(int length, const QString &string) -> void;
  /*! Sets current cursor position to \a pos. */
  virtual auto setCursorPosition(int pos) -> void;
  /*! Selects text between current cursor position and \a toPos. */
  virtual auto select(int toPos) -> void;

private:
  friend class TextEditorFactory;
  friend class Internal::TextEditorFactoryPrivate;
  Internal::BaseTextEditorPrivate *d;
};

class TEXTEDITOR_EXPORT TextEditorWidget : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit TextEditorWidget(QWidget *parent = nullptr);
  ~TextEditorWidget() override;

  auto setTextDocument(const QSharedPointer<TextDocument> &doc) -> void;
  auto textDocument() const -> TextDocument*;
  auto textDocumentPtr() const -> QSharedPointer<TextDocument>;
  virtual auto aboutToOpen(const Utils::FilePath &filePath, const Utils::FilePath &realFilePath) -> void;
  virtual auto openFinishedSuccessfully() -> void;
  // IEditor
  auto saveState() const -> QByteArray;
  virtual auto restoreState(const QByteArray &state) -> void;
  auto gotoLine(int line, int column = 0, bool centerLine = true, bool animate = false) -> void;
  auto position(TextPositionOperation posOp = CurrentPosition, int at = -1) const -> int;
  auto convertPosition(int pos, int *line, int *column) const -> void;
  using QPlainTextEdit::cursorRect;
  auto cursorRect(int pos) const -> QRect;
  auto setCursorPosition(int pos) -> void;
  auto toolBar() -> QToolBar*;
  auto print(QPrinter *) -> void;
  auto appendStandardContextMenuActions(QMenu *menu) -> void;
  auto optionalActions() -> uint;
  auto setOptionalActions(uint optionalActions) -> void;
  auto addOptionalActions(uint optionalActions) -> void;
  auto setAutoCompleter(AutoCompleter *autoCompleter) -> void;
  auto autoCompleter() const -> AutoCompleter*;

  // Works only in conjunction with a syntax highlighter that puts
  // parentheses into text block user data
  auto setParenthesesMatchingEnabled(bool b) -> void;
  auto isParenthesesMatchingEnabled() const -> bool;
  auto setHighlightCurrentLine(bool b) -> void;
  auto highlightCurrentLine() const -> bool;
  auto setLineNumbersVisible(bool b) -> void;
  auto lineNumbersVisible() const -> bool;
  auto setAlwaysOpenLinksInNextSplit(bool b) -> void;
  auto alwaysOpenLinksInNextSplit() const -> bool;
  auto setMarksVisible(bool b) -> void;
  auto marksVisible() const -> bool;
  auto setRequestMarkEnabled(bool b) -> void;
  auto requestMarkEnabled() const -> bool;
  auto setLineSeparatorsAllowed(bool b) -> void;
  auto lineSeparatorsAllowed() const -> bool;
  auto codeFoldingVisible() const -> bool;
  auto setCodeFoldingSupported(bool b) -> void;
  auto codeFoldingSupported() const -> bool;
  auto setMouseNavigationEnabled(bool b) -> void;
  auto mouseNavigationEnabled() const -> bool;
  auto setMouseHidingEnabled(bool b) -> void;
  auto mouseHidingEnabled() const -> bool;
  auto setScrollWheelZoomingEnabled(bool b) -> void;
  auto scrollWheelZoomingEnabled() const -> bool;
  auto setConstrainTooltips(bool b) -> void;
  auto constrainTooltips() const -> bool;
  auto setCamelCaseNavigationEnabled(bool b) -> void;
  auto camelCaseNavigationEnabled() const -> bool;
  auto setRevisionsVisible(bool b) -> void;
  auto revisionsVisible() const -> bool;
  auto setVisibleWrapColumn(int column) -> void;
  auto visibleWrapColumn() const -> int;
  auto columnCount() const -> int;
  auto rowCount() const -> int;
  auto setReadOnly(bool b) -> void;
  auto insertCodeSnippet(const QTextCursor &cursor, const QString &snippet, const SnippetParser &parse) -> void;
  auto multiTextCursor() const -> Utils::MultiTextCursor;
  auto setMultiTextCursor(const Utils::MultiTextCursor &cursor) -> void;
  auto translatedLineRegion(int lineStart, int lineEnd) const -> QRegion;
  auto toolTipPosition(const QTextCursor &c) const -> QPoint;
  auto showTextMarksToolTip(const QPoint &pos, const TextMarks &marks, const TextMark *mainTextMark = nullptr) const -> void;
  auto invokeAssist(AssistKind assistKind, IAssistProvider *provider = nullptr) -> void;
  virtual auto createAssistInterface(AssistKind assistKind, AssistReason assistReason) const -> AssistInterface*;
  static auto duplicateMimeData(const QMimeData *source) -> QMimeData*;
  static auto msgTextTooLarge(quint64 size) -> QString;
  auto insertPlainText(const QString &text) -> void;
  auto extraArea() const -> QWidget*;
  virtual auto extraAreaWidth(int *markWidthPtr = nullptr) const -> int;
  virtual auto extraAreaPaintEvent(QPaintEvent *) -> void;
  virtual auto extraAreaLeaveEvent(QEvent *) -> void;
  virtual auto extraAreaContextMenuEvent(QContextMenuEvent *) -> void;
  virtual auto extraAreaMouseEvent(QMouseEvent *) -> void;
  auto updateFoldingHighlight(const QPoint &pos) -> void;
  auto setLanguageSettingsId(Utils::Id settingsId) -> void;
  auto languageSettingsId() const -> Utils::Id;
  auto setCodeStyle(ICodeStylePreferences *settings) -> void;
  auto displaySettings() const -> const DisplaySettings&;
  auto marginSettings() const -> const MarginSettings&;
  auto behaviorSettings() const -> const BehaviorSettings&;
  auto ensureCursorVisible() -> void;
  auto ensureBlockIsUnfolded(QTextBlock block) -> void;

  static Utils::Id FakeVimSelection;
  static Utils::Id SnippetPlaceholderSelection;
  static Utils::Id CurrentLineSelection;
  static Utils::Id ParenthesesMatchingSelection;
  static Utils::Id AutoCompleteSelection;
  static Utils::Id CodeWarningsSelection;
  static Utils::Id CodeSemanticsSelection;
  static Utils::Id CursorSelection;
  static Utils::Id UndefinedSymbolSelection;
  static Utils::Id UnusedSymbolSelection;
  static Utils::Id OtherSelection;
  static Utils::Id ObjCSelection;
  static Utils::Id DebuggerExceptionSelection;

  auto setExtraSelections(Utils::Id kind, const QList<QTextEdit::ExtraSelection> &selections) -> void;
  auto extraSelections(Utils::Id kind) const -> QList<QTextEdit::ExtraSelection>;
  auto extraSelectionTooltip(int pos) const -> QString;
  auto refactorMarkers() const -> RefactorMarkers;
  auto setRefactorMarkers(const RefactorMarkers &markers) -> void;

  enum Side {
    Left,
    Right
  };

  auto insertExtraToolBarWidget(Side side, QWidget *widget) -> QAction*;

  // keep the auto completion even if the focus is lost
  auto keepAutoCompletionHighlight(bool keepHighlight) -> void;
  auto setAutoCompleteSkipPosition(const QTextCursor &cursor) -> void;

  virtual auto copy() -> void;
  virtual auto paste() -> void;
  virtual auto cut() -> void;
  virtual auto selectAll() -> void;
  virtual auto autoIndent() -> void;
  virtual auto rewrapParagraph() -> void;
  virtual auto unCommentSelection() -> void;
  virtual auto autoFormat() -> void;
  virtual auto encourageApply() -> void;
  virtual auto setDisplaySettings(const DisplaySettings &) -> void;
  virtual auto setMarginSettings(const MarginSettings &) -> void;
  auto setBehaviorSettings(const BehaviorSettings &) -> void;
  auto setTypingSettings(const TypingSettings &) -> void;
  auto setStorageSettings(const StorageSettings &) -> void;
  auto setCompletionSettings(const CompletionSettings &) -> void;
  auto setExtraEncodingSettings(const ExtraEncodingSettings &) -> void;
  auto circularPaste() -> void;
  auto pasteWithoutFormat() -> void;
  auto switchUtf8bom() -> void;
  auto zoomF(float delta) -> void;
  auto zoomReset() -> void;
  auto cutLine() -> void;
  auto copyLine() -> void;
  auto duplicateSelection() -> void;
  auto duplicateSelectionAndComment() -> void;
  auto deleteLine() -> void;
  auto deleteEndOfLine() -> void;
  auto deleteEndOfWord() -> void;
  auto deleteEndOfWordCamelCase() -> void;
  auto deleteStartOfLine() -> void;
  auto deleteStartOfWord() -> void;
  auto deleteStartOfWordCamelCase() -> void;
  auto unfoldAll() -> void;
  auto fold() -> void;
  auto unfold() -> void;
  auto selectEncoding() -> void;
  auto updateTextCodecLabel() -> void;
  auto selectLineEnding(int index) -> void;
  auto updateTextLineEndingLabel() -> void;
  auto gotoBlockStart() -> void;
  auto gotoBlockEnd() -> void;
  auto gotoBlockStartWithSelection() -> void;
  auto gotoBlockEndWithSelection() -> void;
  auto gotoDocumentStart() -> void;
  auto gotoDocumentEnd() -> void;
  auto gotoLineStart() -> void;
  auto gotoLineStartWithSelection() -> void;
  auto gotoLineEnd() -> void;
  auto gotoLineEndWithSelection() -> void;
  auto gotoNextLine() -> void;
  auto gotoNextLineWithSelection() -> void;
  auto gotoPreviousLine() -> void;
  auto gotoPreviousLineWithSelection() -> void;
  auto gotoPreviousCharacter() -> void;
  auto gotoPreviousCharacterWithSelection() -> void;
  auto gotoNextCharacter() -> void;
  auto gotoNextCharacterWithSelection() -> void;
  auto gotoPreviousWord() -> void;
  auto gotoPreviousWordWithSelection() -> void;
  auto gotoNextWord() -> void;
  auto gotoNextWordWithSelection() -> void;
  auto gotoPreviousWordCamelCase() -> void;
  auto gotoPreviousWordCamelCaseWithSelection() -> void;
  auto gotoNextWordCamelCase() -> void;
  auto gotoNextWordCamelCaseWithSelection() -> void;

  virtual auto selectBlockUp() -> bool;
  virtual auto selectBlockDown() -> bool;

  auto selectWordUnderCursor() -> void;
  auto showContextMenu() -> void;
  auto moveLineUp() -> void;
  auto moveLineDown() -> void;
  auto viewPageUp() -> void;
  auto viewPageDown() -> void;
  auto viewLineUp() -> void;
  auto viewLineDown() -> void;
  auto copyLineUp() -> void;
  auto copyLineDown() -> void;
  auto joinLines() -> void;
  auto insertLineAbove() -> void;
  auto insertLineBelow() -> void;
  auto uppercaseSelection() -> void;
  auto lowercaseSelection() -> void;
  auto sortSelectedLines() -> void;
  auto cleanWhitespace() -> void;
  auto indent() -> void;
  auto unindent() -> void;
  auto undo() -> void;
  auto redo() -> void;
  auto openLinkUnderCursor() -> void;
  auto openLinkUnderCursorInNextSplit() -> void;

  virtual auto findUsages() -> void;
  virtual auto renameSymbolUnderCursor() -> void;

  /// Abort code assistant if it is running.
  auto abortAssist() -> void;
  auto configureGenericHighlighter() -> void;

  auto inSnippetMode(bool *active) -> Q_INVOKABLE void; // Used by FakeVim.

  /*! Returns the document line number for the visible \a row.
   *
   * The first visible row is 0, the last visible row is rowCount() - 1.
   *
   * Any Invalid row will return -1 as line number.
   */
  auto blockNumberForVisibleRow(int row) const -> int;
  /*! Returns the first visible line of the document. */
  auto firstVisibleBlockNumber() const -> int;
  /*! Returns the last visible line of the document. */
  auto lastVisibleBlockNumber() const -> int;
  /*! Returns the line visible closest to the vertical center of the editor. */
  auto centerVisibleBlockNumber() const -> int;
  auto highlightScrollBarController() const -> Orca::Plugin::Core::HighlightScrollBarController*;
  auto addHoverHandler(BaseHoverHandler *handler) -> void;
  auto removeHoverHandler(BaseHoverHandler *handler) -> void;

  #ifdef WITH_TESTS
    void processTooltipRequest(const QTextCursor &c);
  #endif

signals:
  auto assistFinished() -> void; // Used in tests.
  auto readOnlyChanged() -> void;
  auto requestBlockUpdate(const QTextBlock &) -> void;
  auto requestLinkAt(const QTextCursor &cursor, Utils::ProcessLinkCallback &callback, bool resolveTarget, bool inNextSplit) -> void;
  auto requestUsages(const QTextCursor &cursor) -> void;
  auto requestRename(const QTextCursor &cursor) -> void;
  auto optionalActionMaskChanged() -> void;

protected:
  auto blockForVisibleRow(int row) const -> QTextBlock;
  auto blockForVerticalOffset(int offset) const -> QTextBlock;
  auto event(QEvent *e) -> bool override;
  auto contextMenuEvent(QContextMenuEvent *e) -> void override;
  auto keyPressEvent(QKeyEvent *e) -> void override;
  auto wheelEvent(QWheelEvent *e) -> void override;
  auto changeEvent(QEvent *e) -> void override;
  auto focusInEvent(QFocusEvent *e) -> void override;
  auto focusOutEvent(QFocusEvent *e) -> void override;
  auto showEvent(QShowEvent *) -> void override;
  auto viewportEvent(QEvent *event) -> bool override;
  auto resizeEvent(QResizeEvent *) -> void override;
  auto paintEvent(QPaintEvent *) -> void override;
  virtual auto paintBlock(QPainter *painter, const QTextBlock &block, const QPointF &offset, const QVector<QTextLayout::FormatRange> &selections, const QRect &clipRect) const -> void;
  auto timerEvent(QTimerEvent *) -> void override;
  auto mouseMoveEvent(QMouseEvent *) -> void override;
  auto mousePressEvent(QMouseEvent *) -> void override;
  auto mouseReleaseEvent(QMouseEvent *) -> void override;
  auto mouseDoubleClickEvent(QMouseEvent *) -> void override;
  auto leaveEvent(QEvent *) -> void override;
  auto keyReleaseEvent(QKeyEvent *) -> void override;
  auto dragEnterEvent(QDragEnterEvent *e) -> void override;
  auto createMimeDataFromSelection() const -> QMimeData* override;
  auto canInsertFromMimeData(const QMimeData *source) const -> bool override;
  auto insertFromMimeData(const QMimeData *source) -> void override;
  auto dragLeaveEvent(QDragLeaveEvent *e) -> void override;
  auto dragMoveEvent(QDragMoveEvent *e) -> void override;
  auto dropEvent(QDropEvent *e) -> void override;
  virtual auto plainTextFromSelection(const QTextCursor &cursor) const -> QString;
  virtual auto plainTextFromSelection(const Utils::MultiTextCursor &cursor) const -> QString;
  static auto convertToPlainText(const QString &txt) -> QString;
  virtual auto lineNumber(int blockNumber) const -> QString;
  virtual auto lineNumberDigits() const -> int;
  virtual auto selectionVisible(int blockNumber) const -> bool;
  virtual auto replacementVisible(int blockNumber) const -> bool;
  virtual auto replacementPenColor(int blockNumber) const -> QColor;
  virtual auto triggerPendingUpdates() -> void;
  virtual auto applyFontSettings() -> void;
  auto showDefaultContextMenu(QContextMenuEvent *e, Utils::Id menuContextId) -> void;
  virtual auto finalizeInitialization() -> void {}
  virtual auto finalizeInitializationAfterDuplication(TextEditorWidget *) -> void {}
  static auto flippedCursor(const QTextCursor &cursor) -> QTextCursor;

public:
  auto selectedText() const -> QString;
  auto setupGenericHighlighter() -> void;
  auto setupFallBackEditor(Utils::Id id) -> void;
  auto remove(int length) -> void;
  auto replace(int length, const QString &string) -> void;
  auto characterAt(int pos) const -> QChar;
  auto textAt(int from, int to) const -> QString;
  auto contextHelpItem(const Orca::Plugin::Core::IContext::HelpCallback &callback) -> void;
  auto setContextHelpItem(const Orca::Plugin::Core::HelpItem &item) -> void;
  auto inFindScope(const QTextCursor &cursor) const -> Q_INVOKABLE bool;
  static auto currentTextEditorWidget() -> TextEditorWidget*;
  static auto fromEditor(const Orca::Plugin::Core::IEditor *editor) -> TextEditorWidget*;

protected:
  /*!
     Reimplement this function to enable code navigation.

     \a resolveTarget is set to true when the target of the link is relevant
     (it isn't until the link is used).
   */
  virtual auto findLinkAt(const QTextCursor &, Utils::ProcessLinkCallback &&processLinkCallback, bool resolveTarget = true, bool inNextSplit = false) -> void;

  /*!
     Returns whether the link was opened successfully.
   */
  auto openLink(const Utils::Link &link, bool inNextSplit = false) -> bool;

  /*!
    Reimplement this function to change the default replacement text.
    */
  virtual auto foldReplacementText(const QTextBlock &block) const -> QString;
  virtual auto drawCollapsedBlockPopup(QPainter &painter, const QTextBlock &block, QPointF offset, const QRect &clip) -> void;
  auto visibleFoldedBlockNumber() const -> int;
  auto doSetTextCursor(const QTextCursor &cursor) -> void override;
  auto doSetTextCursor(const QTextCursor &cursor, bool keepMultiSelection) -> void;

signals:
  auto markRequested(TextEditorWidget *widget, int line, TextMarkRequestKind kind) -> void;
  auto markContextMenuRequested(TextEditorWidget *widget, int line, QMenu *menu) -> void;
  auto tooltipOverrideRequested(TextEditorWidget *widget, const QPoint &globalPos, int position, bool *handled) -> void;
  auto tooltipRequested(const QPoint &globalPos, int position) -> void;
  auto activateEditor(Orca::Plugin::Core::EditorManager::OpenEditorFlags flags = {}) -> void;

protected:
  virtual auto slotCursorPositionChanged() -> void;                    // Used in VcsBase
  virtual auto slotCodeStyleSettingsChanged(const QVariant &) -> void; // Used in CppEditor

private:
  Internal::TextEditorWidgetPrivate *d;
  friend class BaseTextEditor;
  friend class TextEditorFactory;
  friend class Internal::TextEditorFactoryPrivate;
  friend class Internal::TextEditorWidgetPrivate;
  friend class Internal::TextEditorOverlay;
  friend class RefactorOverlay;

  auto updateVisualWrapColumn() -> void;
};

class TEXTEDITOR_EXPORT TextEditorLinkLabel : public Utils::ElidingLabel {
public:
  TextEditorLinkLabel(QWidget *parent = nullptr);

  auto setLink(Utils::Link link) -> void;
  auto link() const -> Utils::Link;

protected:
  auto mousePressEvent(QMouseEvent *event) -> void override;
  auto mouseMoveEvent(QMouseEvent *event) -> void override;
  auto mouseReleaseEvent(QMouseEvent *event) -> void override;

private:
  QPoint m_dragStartPosition;
  Utils::Link m_link;
};

class TEXTEDITOR_EXPORT TextEditorFactory : public Orca::Plugin::Core::IEditorFactory {

public:
  TextEditorFactory();
  ~TextEditorFactory() override;

  using EditorCreator = std::function<BaseTextEditor *()>;
  using DocumentCreator = std::function<TextDocument *()>;
  // editor widget must be castable (qobject_cast or Aggregate::query) to TextEditorWidget
  using EditorWidgetCreator = std::function<QWidget *()>;
  using SyntaxHighLighterCreator = std::function<SyntaxHighlighter *()>;
  using IndenterCreator = std::function<Indenter *(QTextDocument *)>;
  using AutoCompleterCreator = std::function<AutoCompleter *()>;

  auto setDocumentCreator(const DocumentCreator &creator) -> void;
  auto setEditorWidgetCreator(const EditorWidgetCreator &creator) -> void;
  auto setEditorCreator(const EditorCreator &creator) -> void;
  auto setIndenterCreator(const IndenterCreator &creator) -> void;
  auto setSyntaxHighlighterCreator(const SyntaxHighLighterCreator &creator) -> void;
  auto setUseGenericHighlighter(bool enabled) -> void;
  auto setAutoCompleterCreator(const AutoCompleterCreator &creator) -> void;
  auto setEditorActionHandlers(uint optionalActions) -> void;
  auto addHoverHandler(BaseHoverHandler *handler) -> void;
  auto setCompletionAssistProvider(CompletionAssistProvider *provider) -> void;
  auto setCommentDefinition(Utils::CommentDefinition definition) -> void;
  auto setDuplicatedSupported(bool on) -> void;
  auto setMarksVisible(bool on) -> void;
  auto setParenthesesMatchingEnabled(bool on) -> void;
  auto setCodeFoldingSupported(bool on) -> void;

private:
  friend class BaseTextEditor;
  friend class PlainTextEditorFactory;
  Internal::TextEditorFactoryPrivate *d;
};

} // namespace TextEditor

QT_BEGIN_NAMESPACE

auto qHash(const QColor &color) -> Utils::QHashValueType;

QT_END_NAMESPACE
