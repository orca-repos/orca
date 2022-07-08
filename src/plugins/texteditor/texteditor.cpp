// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "texteditor.hpp"
#include "texteditor_p.hpp"
#include "displaysettings.hpp"
#include "marginsettings.hpp"
#include "fontsettings.hpp"
#include "texteditoractionhandler.hpp"

#include "autocompleter.hpp"
#include "basehoverhandler.hpp"
#include "behaviorsettings.hpp"
#include "circularclipboard.hpp"
#include "circularclipboardassist.hpp"
#include "completionsettings.hpp"
#include "extraencodingsettings.hpp"
#include "highlighter.hpp"
#include "highlightersettings.hpp"
#include "icodestylepreferences.hpp"
#include "refactoroverlay.hpp"
#include "snippets/snippet.hpp"
#include "snippets/snippetoverlay.hpp"
#include "storagesettings.hpp"
#include "syntaxhighlighter.hpp"
#include "tabsettings.hpp"
#include "textdocument.hpp"
#include "textdocumentlayout.hpp"
#include "texteditorconstants.hpp"
#include "texteditoroverlay.hpp"
#include "texteditorsettings.hpp"
#include "typingsettings.hpp"

#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/codeassist/codeassistant.hpp>
#include <texteditor/codeassist/completionassistprovider.hpp>
#include <texteditor/codeassist/documentcontentcompletion.hpp>

#include <core/actionmanager/actioncontainer.hpp>
#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>
#include <core/coreconstants.hpp>
#include <core/dialogs/codecselector.hpp>
#include <core/find/basetextfind.hpp>
#include <core/find/highlightscrollbarcontroller.hpp>
#include <core/icore.hpp>
#include <core/manhattanstyle.hpp>

#include <aggregation/aggregate.hpp>

#include <utils/algorithm.hpp>
#include <utils/camelcasecursor.hpp>
#include <utils/dropsupport.hpp>
#include <utils/executeondestruction.hpp>
#include <utils/fadingindicator.hpp>
#include <utils/filesearch.hpp>
#include <utils/fileutils.hpp>
#include <utils/fixedsizeclicklabel.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/infobar.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/multitextcursor.hpp>
#include <utils/qtcassert.hpp>
#include <utils/styledbar.hpp>
#include <utils/stylehelper.hpp>
#include <utils/textutils.hpp>
#include <utils/theme/theme.hpp>
#include <utils/tooltip/tooltip.hpp>
#include <utils/uncommentselection.hpp>

#include <QAbstractTextDocumentLayout>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QComboBox>
#include <QDebug>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QKeyEvent>
#include <QLoggingCategory>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QPrintDialog>
#include <QPrinter>
#include <QPropertyAnimation>
#include <QDrag>
#include <QSequentialAnimationGroup>
#include <QScreen>
#include <QScrollBar>
#include <QShortcut>
#include <QStyle>
#include <QStyleFactory>
#include <QTextBlock>
#include <QTextCodec>
#include <QTextCursor>
#include <QTextDocumentFragment>
#include <QTextLayout>
#include <QTime>
#include <QTimeLine>
#include <QTimer>
#include <QToolBar>

/*!
    \namespace TextEditor
    \brief The TextEditor namespace contains the base text editor and several classes which
    provide supporting functionality like snippets, highlighting, \l{CodeAssist}{code assist},
    indentation and style, and others.
*/

/*!
    \namespace TextEditor::Internal
    \internal
*/

/*!
    \class TextEditor::BaseTextEditor
    \brief The BaseTextEditor class is base implementation for QPlainTextEdit-based
    text editors. It can use the Kate text highlighting definitions, and some basic
    auto indentation.

    The corresponding document base class is BaseTextDocument, the corresponding
    widget base class is BaseTextEditorWidget.

    It is the default editor for text files used by \QC, if no other editor
    implementation matches the MIME type.
*/


using namespace Core;
using namespace Utils;

namespace TextEditor {
using namespace Internal;
namespace Internal {

enum {
  NExtraSelectionKinds = 12
};

using TransformationMethod = QString(const QString &);
using ListTransformationMethod = void(QStringList &);

static constexpr char dropProperty[] = "dropProp";

class LineColumnLabel : public FixedSizeClickLabel {
  Q_OBJECT

public:
  LineColumnLabel(TextEditorWidget *parent) : FixedSizeClickLabel(parent), m_editor(parent)
  {
    setMaxText(TextEditorWidget::tr("Line: 9999, Col: 999"));
    connect(m_editor, &QPlainTextEdit::cursorPositionChanged, this, &LineColumnLabel::update);
    connect(this, &FixedSizeClickLabel::clicked, ActionManager::instance(), [this] {
      emit m_editor->activateEditor(EditorManager::IgnoreNavigationHistory);
      QMetaObject::invokeMethod(ActionManager::instance(), [] {
        if (Command *cmd = ActionManager::command(Core::Constants::GOTO)) {
          if (QAction *act = cmd->action())
            act->trigger();
        }
      }, Qt::QueuedConnection);
    });
  }

private:
  auto update() -> void
  {
    const auto cursor = m_editor->textCursor();
    const auto block = cursor.block();
    const auto line = block.blockNumber() + 1;
    const auto &tabSettings = m_editor->textDocument()->tabSettings();
    const auto column = tabSettings.columnAt(block.text(), cursor.positionInBlock()) + 1;
    const auto text = TextEditorWidget::tr("Line: %1, Col: %2");
    setText(text.arg(line).arg(column));
    const auto toolTipText = TextEditorWidget::tr("Cursor position: %1");
    setToolTip(toolTipText.arg(QString::number(cursor.position())));
    auto f = font();
    f.setItalic(m_editor->multiTextCursor().hasMultipleCursors());
    setFont(f);
  }

  TextEditorWidget *m_editor;
};

class TextEditorAnimator : public QObject {
  Q_OBJECT

public:
  TextEditorAnimator(QObject *parent);

  auto init(const QTextCursor &cursor, const QFont &f, const QPalette &pal) -> void;
  auto cursor() const -> QTextCursor { return m_cursor; }
  auto draw(QPainter *p, const QPointF &pos) -> void;
  auto rect() const -> QRectF;
  auto value() const -> qreal { return m_value; }
  auto lastDrawPos() const -> QPointF { return m_lastDrawPos; }
  auto finish() -> void;
  auto isRunning() const -> bool;

signals:
  auto updateRequest(const QTextCursor &cursor, QPointF lastPos, QRectF rect) -> void;

private:
  auto step(qreal v) -> void;

  QTimeLine m_timeline;
  qreal m_value;
  QTextCursor m_cursor;
  QPointF m_lastDrawPos;
  QFont m_font;
  QPalette m_palette;
  QString m_text;
  QSizeF m_size;
};

class TextEditExtraArea : public QWidget {
public:
  TextEditExtraArea(TextEditorWidget *edit) : QWidget(edit)
  {
    textEdit = edit;
    setAutoFillBackground(true);
  }

protected:
  auto sizeHint() const -> QSize override
  {
    return {textEdit->extraAreaWidth(), 0};
  }

  auto paintEvent(QPaintEvent *event) -> void override
  {
    textEdit->extraAreaPaintEvent(event);
  }

  auto mousePressEvent(QMouseEvent *event) -> void override
  {
    textEdit->extraAreaMouseEvent(event);
  }

  auto mouseMoveEvent(QMouseEvent *event) -> void override
  {
    textEdit->extraAreaMouseEvent(event);
  }

  auto mouseReleaseEvent(QMouseEvent *event) -> void override
  {
    textEdit->extraAreaMouseEvent(event);
  }

  auto leaveEvent(QEvent *event) -> void override
  {
    textEdit->extraAreaLeaveEvent(event);
  }

  auto contextMenuEvent(QContextMenuEvent *event) -> void override
  {
    textEdit->extraAreaContextMenuEvent(event);
  }

  auto changeEvent(QEvent *event) -> void override
  {
    if (event->type() == QEvent::PaletteChange)
      QCoreApplication::sendEvent(textEdit, event);
  }

  auto wheelEvent(QWheelEvent *event) -> void override
  {
    QCoreApplication::sendEvent(textEdit->viewport(), event);
  }

private:
  TextEditorWidget *textEdit;
};

class BaseTextEditorPrivate {
public:
  BaseTextEditorPrivate() = default;

  TextEditorFactoryPrivate *m_origin = nullptr;
};

class HoverHandlerRunner {
public:
  using Callback = std::function<void(TextEditorWidget *, BaseHoverHandler *, int)>;

  HoverHandlerRunner(TextEditorWidget *widget, QList<BaseHoverHandler*> &handlers) : m_widget(widget), m_handlers(handlers) { }

  ~HoverHandlerRunner() { abortHandlers(); }

  auto startChecking(const QTextCursor &textCursor, const Callback &callback) -> void
  {
    if (m_handlers.empty())
      return;

    // Does the last handler still applies?
    const auto documentRevision = textCursor.document()->revision();
    const auto position = Text::wordStartCursor(textCursor).position();
    if (m_lastHandlerInfo.applies(documentRevision, position)) {
      callback(m_widget, m_lastHandlerInfo.handler, position);
      return;
    }

    if (isCheckRunning(documentRevision, position))
      return;

    // Update invocation data
    m_documentRevision = documentRevision;
    m_position = position;
    m_callback = callback;

    restart();
  }

  auto isCheckRunning(int documentRevision, int position) const -> bool
  {
    return m_currentHandlerIndex >= 0 && m_documentRevision == documentRevision && m_position == position;
  }

  auto checkNext() -> void
  {
    QTC_ASSERT(m_currentHandlerIndex >= 0, return);
    QTC_ASSERT(m_currentHandlerIndex < m_handlers.size(), return);
    const auto currentHandler = m_handlers[m_currentHandlerIndex];

    currentHandler->checkPriority(m_widget, m_position, [this](int priority) {
      onHandlerFinished(m_documentRevision, m_position, priority);
    });
  }

  auto onHandlerFinished(int documentRevision, int position, int priority) -> void
  {
    QTC_ASSERT(m_currentHandlerIndex >= 0, return);
    QTC_ASSERT(m_currentHandlerIndex < m_handlers.size(), return);
    QTC_ASSERT(documentRevision == m_documentRevision, return);
    QTC_ASSERT(position == m_position, return);

    const auto currentHandler = m_handlers[m_currentHandlerIndex];
    if (priority > m_highestHandlerPriority) {
      m_highestHandlerPriority = priority;
      m_bestHandler = currentHandler;
    }

    // There are more, check next
    ++m_currentHandlerIndex;
    if (m_currentHandlerIndex < m_handlers.size()) {
      checkNext();
      return;
    }
    m_currentHandlerIndex = -1;

    // All were queried, run the best
    if (m_bestHandler) {
      m_lastHandlerInfo = LastHandlerInfo(m_bestHandler, m_documentRevision, m_position);
      m_callback(m_widget, m_bestHandler, m_position);
    }
  }

  auto handlerRemoved(BaseHoverHandler *handler) -> void
  {
    if (m_lastHandlerInfo.handler == handler)
      m_lastHandlerInfo = LastHandlerInfo();
    if (m_currentHandlerIndex >= 0)
      restart();
  }

private:
  auto abortHandlers() -> void
  {
    for (const auto handler : m_handlers)
      handler->abort();
    m_currentHandlerIndex = -1;
  }

  auto restart() -> void
  {
    abortHandlers();

    if (m_handlers.empty())
      return;

    // Re-initialize process data
    m_currentHandlerIndex = 0;
    m_bestHandler = nullptr;
    m_highestHandlerPriority = -1;

    // Start checking
    checkNext();
  }

  TextEditorWidget *m_widget;
  const QList<BaseHoverHandler*> &m_handlers;

  struct LastHandlerInfo {
    LastHandlerInfo() = default;
    LastHandlerInfo(BaseHoverHandler *handler, int documentRevision, int cursorPosition) : handler(handler), documentRevision(documentRevision), cursorPosition(cursorPosition) {}

    auto applies(int documentRevision, int cursorPosition) const -> bool
    {
      return handler && documentRevision == this->documentRevision && cursorPosition == this->cursorPosition;
    }

    BaseHoverHandler *handler = nullptr;
    int documentRevision = -1;
    int cursorPosition = -1;
  } m_lastHandlerInfo;

  // invocation data
  Callback m_callback;
  int m_position = -1;
  int m_documentRevision = -1;

  // processing data
  int m_currentHandlerIndex = -1;
  int m_highestHandlerPriority = -1;
  BaseHoverHandler *m_bestHandler = nullptr;
};

struct CursorData {
  QTextLayout *layout = nullptr;
  QPointF offset;
  int pos = 0;
  QPen pen;
};

struct PaintEventData {
  PaintEventData(TextEditorWidget *editor, QPaintEvent *event, QPointF offset) : offset(offset), viewportRect(editor->viewport()->rect()), eventRect(event->rect()), doc(editor->document()), documentLayout(qobject_cast<TextDocumentLayout*>(doc->documentLayout())), documentWidth(int(doc->size().width())), textCursor(editor->textCursor()), textCursorBlock(textCursor.block()), isEditable(!editor->isReadOnly()), fontSettings(editor->textDocument()->fontSettings()), searchScopeFormat(fontSettings.toTextCharFormat(C_SEARCH_SCOPE)), searchResultFormat(fontSettings.toTextCharFormat(C_SEARCH_RESULT)), visualWhitespaceFormat(fontSettings.toTextCharFormat(C_VISUAL_WHITESPACE)), ifdefedOutFormat(fontSettings.toTextCharFormat(C_DISABLED_CODE)), suppressSyntaxInIfdefedOutBlock(ifdefedOutFormat.foreground() != fontSettings.toTextCharFormat(C_TEXT).foreground()) { }
  QPointF offset;
  const QRect viewportRect;
  const QRect eventRect;
  qreal rightMargin = -1;
  const QTextDocument *doc;
  TextDocumentLayout *documentLayout;
  const int documentWidth;
  const QTextCursor textCursor;
  const QTextBlock textCursorBlock;
  const bool isEditable;
  const FontSettings fontSettings;
  const QTextCharFormat searchScopeFormat;
  const QTextCharFormat searchResultFormat;
  const QTextCharFormat visualWhitespaceFormat;
  const QTextCharFormat ifdefedOutFormat;
  const bool suppressSyntaxInIfdefedOutBlock;
  QAbstractTextDocumentLayout::PaintContext context;
  QTextBlock visibleCollapsedBlock;
  QPointF visibleCollapsedBlockOffset;
  QTextBlock block;
  QList<CursorData> cursors;
};

struct PaintEventBlockData {
  QRectF boundingRect;
  QVector<QTextLayout::FormatRange> selections;
  QTextLayout *layout = nullptr;
  int position = 0;
  int length = 0;
};

struct ExtraAreaPaintEventData;

class TextEditorWidgetPrivate : public QObject {
public:
  TextEditorWidgetPrivate(TextEditorWidget *parent);
  ~TextEditorWidgetPrivate() override;

  auto setupDocumentSignals() -> void;
  auto updateLineSelectionColor() -> void;
  auto print(QPrinter *printer) -> void;
  auto maybeSelectLine() -> void;
  auto duplicateSelection(bool comment) -> void;
  auto updateCannotDecodeInfo() -> void;
  auto collectToCircularClipboard() -> void;
  auto setClipboardSelection() -> void;
  auto ctor(const QSharedPointer<TextDocument> &doc) -> void;
  auto handleHomeKey(bool anchor, bool block) -> void;
  auto handleBackspaceKey() -> void;
  auto moveLineUpDown(bool up) -> void;
  auto copyLineUpDown(bool up) -> void;
  auto saveCurrentCursorPositionForNavigation() -> void;
  auto updateHighlights() -> void;
  auto updateCurrentLineInScrollbar() -> void;
  auto updateCurrentLineHighlight() -> void;
  auto drawFoldingMarker(QPainter *painter, const QPalette &pal, const QRect &rect, bool expanded, bool active, bool hovered) const -> void;
  auto updateAnnotationBounds(TextBlockUserData *blockUserData, TextDocumentLayout *layout, bool annotationsVisible) -> bool;
  auto updateLineAnnotation(const PaintEventData &data, const PaintEventBlockData &blockData, QPainter &painter) -> void;
  auto paintRightMarginArea(PaintEventData &data, QPainter &painter) const -> void;
  auto paintRightMarginLine(const PaintEventData &data, QPainter &painter) const -> void;
  auto paintBlockHighlight(const PaintEventData &data, QPainter &painter) const -> void;
  auto paintSearchResultOverlay(const PaintEventData &data, QPainter &painter) const -> void;
  auto paintIfDefedOutBlocks(const PaintEventData &data, QPainter &painter) const -> void;
  auto paintFindScope(const PaintEventData &data, QPainter &painter) const -> void;
  auto paintCurrentLineHighlight(const PaintEventData &data, QPainter &painter) const -> void;
  auto paintCursorAsBlock(const PaintEventData &data, QPainter &painter, PaintEventBlockData &blockData, int cursorPosition) const -> void;
  auto paintAdditionalVisualWhitespaces(PaintEventData &data, QPainter &painter, qreal top) const -> void;
  auto paintReplacement(PaintEventData &data, QPainter &painter, qreal top) const -> void;
  auto paintWidgetBackground(const PaintEventData &data, QPainter &painter) const -> void;
  auto paintOverlays(const PaintEventData &data, QPainter &painter) const -> void;
  auto paintCursor(const PaintEventData &data, QPainter &painter) const -> void;
  auto setupBlockLayout(const PaintEventData &data, QPainter &painter, PaintEventBlockData &blockData) const -> void;
  auto setupSelections(const PaintEventData &data, PaintEventBlockData &blockData) const -> void;
  auto addCursorsPosition(PaintEventData &data, QPainter &painter, const PaintEventBlockData &blockData) const -> void;
  auto nextVisibleBlock(const QTextBlock &block) const -> QTextBlock;
  auto cleanupAnnotationCache() -> void;

  // extra area paint methods
  auto paintLineNumbers(QPainter &painter, const ExtraAreaPaintEventData &data, const QRectF &blockBoundingRect) const -> void;
  auto paintTextMarks(QPainter &painter, const ExtraAreaPaintEventData &data, const QRectF &blockBoundingRect) const -> void;
  auto paintCodeFolding(QPainter &painter, const ExtraAreaPaintEventData &data, const QRectF &blockBoundingRect) const -> void;
  auto paintRevisionMarker(QPainter &painter, const ExtraAreaPaintEventData &data, const QRectF &blockBoundingRect) const -> void;
  auto toggleBlockVisible(const QTextBlock &block) -> void;
  auto foldBox() -> QRect;
  auto foldedBlockAt(const QPoint &pos, QRect *box = nullptr) const -> QTextBlock;
  auto isMouseNavigationEvent(QMouseEvent *e) const -> bool;
  auto requestUpdateLink(QMouseEvent *e) -> void;
  auto updateLink() -> void;
  auto showLink(const Link &) -> void;
  auto clearLink() -> void;
  auto universalHelper() -> void; // test function for development
  auto cursorMoveKeyEvent(QKeyEvent *e) -> bool;
  auto processTooltipRequest(const QTextCursor &c) -> void;
  auto processAnnotaionTooltipRequest(const QTextBlock &block, const QPoint &pos) const -> bool;
  auto showTextMarksToolTip(const QPoint &pos, const TextMarks &marks, const TextMark *mainTextMark = nullptr) const -> void;
  auto transformSelection(TransformationMethod method) -> void;
  auto transformSelectedLines(ListTransformationMethod method) -> void;
  auto slotUpdateExtraAreaWidth(optional<int> width = {}) -> void;
  auto slotUpdateRequest(const QRect &r, int dy) -> void;
  auto slotUpdateBlockNotify(const QTextBlock &) -> void;
  auto updateTabStops() -> void;
  auto applyFontSettingsDelayed() -> void;
  auto markRemoved(TextMark *mark) -> void;
  auto editorContentsChange(int position, int charsRemoved, int charsAdded) -> void;
  auto documentAboutToBeReloaded() -> void;
  auto documentReloadFinished(bool success) -> void;
  auto highlightSearchResultsSlot(const QString &txt, FindFlags findFlags) -> void;
  auto searchResultsReady(int beginIndex, int endIndex) -> void;
  auto searchFinished() -> void;
  auto setupScrollBar() -> void;
  auto highlightSearchResultsInScrollBar() -> void;
  auto scheduleUpdateHighlightScrollBar() -> void;
  auto updateHighlightScrollBarNow() -> void;

  struct SearchResult {
    int start;
    int length;
  };

  auto addSearchResultsToScrollBar(QVector<SearchResult> results) -> void;
  auto adjustScrollBarRanges() -> void;
  auto setFindScope(const MultiTextCursor &scope) -> void;
  auto updateCursorPosition() -> void;

  // parentheses matcher
  auto _q_matchParentheses() -> void;
  auto _q_highlightBlocks() -> void;
  auto autocompleterHighlight(const QTextCursor &cursor = QTextCursor()) -> void;
  auto updateAnimator(QPointer<TextEditorAnimator> animator, QPainter &painter) -> void;
  auto cancelCurrentAnimations() -> void;
  auto slotSelectionChanged() -> void;
  auto _q_animateUpdate(const QTextCursor &cursor, QPointF lastPos, QRectF rect) -> void;
  auto updateCodeFoldingVisible() -> void;
  auto reconfigure() -> void;
  auto updateSyntaxInfoBar(const Highlighter::Definitions &definitions, const QString &fileName) -> void;
  auto configureGenericHighlighter(const KSyntaxHighlighting::Definition &definition) -> void;
  auto rememberCurrentSyntaxDefinition() -> void;
  auto openLinkUnderCursor(bool openInNextSplit) -> void;
  
  TextEditorWidget *q;
  QWidget *m_toolBarWidget = nullptr;
  QToolBar *m_toolBar = nullptr;
  QWidget *m_stretchWidget = nullptr;
  LineColumnLabel *m_cursorPositionLabel = nullptr;
  FixedSizeClickLabel *m_fileEncodingLabel = nullptr;
  QAction *m_fileEncodingLabelAction = nullptr;
  BaseTextFind *m_find = nullptr;
  QComboBox *m_fileLineEnding = nullptr;
  QAction *m_fileLineEndingAction = nullptr;
  uint m_optionalActionMask = TextEditorActionHandler::None;
  bool m_contentsChanged = false;
  bool m_lastCursorChangeWasInteresting = false;
  QSharedPointer<TextDocument> m_document;
  QByteArray m_tempState;
  QByteArray m_tempNavigationState;
  bool m_parenthesesMatchingEnabled = false;
  // parentheses matcher
  bool m_formatRange = false;
  QTimer m_parenthesesMatchingTimer;
  // end parentheses matcher
  QWidget *m_extraArea = nullptr;
  Id m_tabSettingsId;
  ICodeStylePreferences *m_codeStylePreferences = nullptr;
  DisplaySettings m_displaySettings;
  bool m_annotationsrRight = true;
  MarginSettings m_marginSettings;
  // apply when making visible the first time, for the split case
  bool m_fontSettingsNeedsApply = true;
  bool m_wasNotYetShown = true;
  BehaviorSettings m_behaviorSettings;
  int extraAreaSelectionAnchorBlockNumber = -1;
  int extraAreaToggleMarkBlockNumber = -1;
  int extraAreaHighlightFoldedBlockNumber = -1;
  int extraAreaPreviousMarkTooltipRequestedLine = -1;
  TextEditorOverlay *m_overlay = nullptr;
  SnippetOverlay *m_snippetOverlay = nullptr;
  TextEditorOverlay *m_searchResultOverlay = nullptr;

  auto snippetCheckCursor(const QTextCursor &cursor) -> bool;
  auto snippetTabOrBacktab(bool forward) -> void;

  struct AnnotationRect {
    QRectF rect;
    const TextMark *mark;
  };

  QMap<int, QList<AnnotationRect>> m_annotationRects;

  auto getLastLineLineRect(const QTextBlock &block) -> QRectF;

  RefactorOverlay *m_refactorOverlay = nullptr;
  HelpItem m_contextHelpItem;
  QBasicTimer foldedBlockTimer;
  int visibleFoldedBlockNumber = -1;
  int suggestedVisibleFoldedBlockNumber = -1;
  auto clearVisibleFoldedBlock() -> void;
  bool m_mouseOnFoldedMarker = false;
  auto foldLicenseHeader() -> void;
  QBasicTimer autoScrollTimer;
  uint m_marksVisible : 1;
  uint m_codeFoldingVisible : 1;
  uint m_codeFoldingSupported : 1;
  uint m_revisionsVisible : 1;
  uint m_lineNumbersVisible : 1;
  uint m_highlightCurrentLine : 1;
  uint m_requestMarkEnabled : 1;
  uint m_lineSeparatorsAllowed : 1;
  uint m_maybeFakeTooltipEvent : 1;
  int m_visibleWrapColumn = 0;
  Link m_currentLink;
  bool m_linkPressed = false;
  QTextCursor m_pendingLinkUpdate;
  QTextCursor m_lastLinkUpdate;
  QRegularExpression m_searchExpr;
  QString m_findText;
  FindFlags m_findFlags;
  auto highlightSearchResults(const QTextBlock &block, const PaintEventData &data) const -> void;

  QTimer m_delayedUpdateTimer;

  auto setExtraSelections(Id kind, const QList<QTextEdit::ExtraSelection> &selections) -> void;

  QHash<Id, QList<QTextEdit::ExtraSelection>> m_extraSelections;

  auto startCursorFlashTimer() -> void;
  auto resetCursorFlashTimer() -> void;

  QBasicTimer m_cursorFlashTimer;

  bool m_cursorVisible = false;
  bool m_moveLineUndoHack = false;
  auto updateCursorSelections() -> void;
  auto moveCursor(QTextCursor::MoveOperation operation, QTextCursor::MoveMode mode = QTextCursor::MoveAnchor) -> void;
  auto cursorUpdateRect(const MultiTextCursor &cursor) -> QRect;

  MultiTextCursor m_findScope;
  QTextCursor m_selectBlockAnchor;

  auto moveCursorVisible(bool ensureVisible = true) -> void;
  auto visualIndent(const QTextBlock &block) const -> int;

  TextEditorPrivateHighlightBlocks m_highlightBlocksInfo;
  QTimer m_highlightBlocksTimer;
  CodeAssistant m_codeAssistant;
  QList<BaseHoverHandler*> m_hoverHandlers; // Not owned
  HoverHandlerRunner m_hoverHandlerRunner;
  QPointer<QSequentialAnimationGroup> m_navigationAnimation;
  QPointer<TextEditorAnimator> m_bracketsAnimator;
  // Animation and highlighting of auto completed text
  QPointer<TextEditorAnimator> m_autocompleteAnimator;
  bool m_animateAutoComplete = true;
  bool m_highlightAutoComplete = true;
  bool m_skipAutoCompletedText = true;
  bool m_skipFormatOnPaste = false;
  bool m_removeAutoCompletedText = true;
  bool m_keepAutoCompletionHighlight = false;
  QList<QTextCursor> m_autoCompleteHighlightPos;

  auto updateAutoCompleteHighlight() -> void;

  QList<int> m_cursorBlockNumbers;
  int m_blockCount = 0;
  QPoint m_markDragStart;
  bool m_markDragging = false;
  QCursor m_markDragCursor;
  TextMark *m_dragMark = nullptr;
  QTextCursor m_dndCursor;
  QScopedPointer<ClipboardAssistProvider> m_clipboardAssistProvider;
  QScopedPointer<AutoCompleter> m_autoCompleter;
  CommentDefinition m_commentDefinition;
  QFutureWatcher<FileSearchResultList> *m_searchWatcher = nullptr;
  QVector<SearchResult> m_searchResults;
  QTimer m_scrollBarUpdateTimer;
  HighlightScrollBarController *m_highlightScrollBarController = nullptr;
  bool m_scrollBarUpdateScheduled = false;
  const MultiTextCursor m_cursors;

  struct BlockSelection {
    int blockNumber = -1;
    int column = -1;
    int anchorBlockNumber = -1;
    int anchorColumn = -1;
  };

  QList<BlockSelection> m_blockSelections;

  auto generateCursorsForBlockSelection(const BlockSelection &blockSelection) -> QList<QTextCursor>;
  auto initBlockSelection() -> void;
  auto clearBlockSelection() -> void;
  auto handleMoveBlockSelection(QTextCursor::MoveOperation op) -> void;

  class UndoCursor {
  public:
    int position = 0;
    int anchor = 0;
  };

  using UndoMultiCursor = QList<UndoCursor>;
  QStack<UndoMultiCursor> m_undoCursorStack;
};

class TextEditorWidgetFind : public BaseTextFind {
public:
  TextEditorWidgetFind(TextEditorWidget *editor) : BaseTextFind(editor), m_editor(editor)
  {
    setMultiTextCursorProvider([editor]() { return editor->multiTextCursor(); });
  }

  ~TextEditorWidgetFind() override { cancelCurrentSelectAll(); }

  auto supportsSelectAll() const -> bool override { return true; }
  auto selectAll(const QString &txt, FindFlags findFlags) -> void override;
  static auto cancelCurrentSelectAll() -> void;

private:
  TextEditorWidget *const m_editor;
  static QFutureWatcher<FileSearchResultList> *m_selectWatcher;
};

QFutureWatcher<FileSearchResultList> *TextEditorWidgetFind::m_selectWatcher = nullptr;

auto TextEditorWidgetFind::selectAll(const QString &txt, FindFlags findFlags) -> void
{
  if (txt.isEmpty())
    return;

  cancelCurrentSelectAll();

  m_selectWatcher = new QFutureWatcher<FileSearchResultList>();
  connect(m_selectWatcher, &QFutureWatcher<FileSearchResultList>::finished, this, [this]() {
    const auto future = m_selectWatcher->future();
    m_selectWatcher->deleteLater();
    m_selectWatcher = nullptr;
    if (future.resultCount() <= 0)
      return;
    const auto &results = future.result();
    const QTextCursor c(m_editor->document());
    auto cursorForResult = [c](const FileSearchResult &r) {
      return Text::selectAt(c, r.lineNumber, r.matchStart + 1, r.matchLength);
    };
    auto cursors = transform(results, cursorForResult);
    cursors = filtered(cursors, [this](const QTextCursor &c) {
      return m_editor->inFindScope(c);
    });
    m_editor->setMultiTextCursor(MultiTextCursor(cursors));
    m_editor->setFocus();
  });

  const QString &fileName = m_editor->textDocument()->filePath().toString();
  QMap<QString, QString> fileToContentsMap;
  fileToContentsMap[fileName] = m_editor->textDocument()->plainText();

  const auto it = new FileListIterator({fileName}, {const_cast<QTextCodec*>(m_editor->textDocument()->codec())});
  const QTextDocument::FindFlags findFlags2 = textDocumentFlagsForFindFlags(findFlags);

  if (findFlags & FindRegularExpression)
    m_selectWatcher->setFuture(findInFilesRegExp(txt, it, findFlags2, fileToContentsMap));
  else
    m_selectWatcher->setFuture(findInFiles(txt, it, findFlags2, fileToContentsMap));
}

auto TextEditorWidgetFind::cancelCurrentSelectAll() -> void
{
  if (m_selectWatcher) {
    m_selectWatcher->disconnect();
    m_selectWatcher->cancel();
    m_selectWatcher->deleteLater();
    m_selectWatcher = nullptr;
  }
}

TextEditorWidgetPrivate::TextEditorWidgetPrivate(TextEditorWidget *parent) : q(parent), m_marksVisible(false), m_codeFoldingVisible(false), m_codeFoldingSupported(false), m_revisionsVisible(false), m_lineNumbersVisible(true), m_highlightCurrentLine(true), m_requestMarkEnabled(true), m_lineSeparatorsAllowed(false), m_maybeFakeTooltipEvent(false), m_hoverHandlerRunner(parent, m_hoverHandlers), m_clipboardAssistProvider(new ClipboardAssistProvider), m_autoCompleter(new AutoCompleter)
{
  const auto aggregate = new Aggregation::Aggregate;
  m_find = new TextEditorWidgetFind(q);
  connect(m_find, &BaseTextFind::highlightAllRequested, this, &TextEditorWidgetPrivate::highlightSearchResultsSlot);
  connect(m_find, &BaseTextFind::findScopeChanged, this, &TextEditorWidgetPrivate::setFindScope);
  aggregate->add(m_find);
  aggregate->add(q);

  m_extraArea = new TextEditExtraArea(q);
  m_extraArea->setMouseTracking(true);

  const auto toolBarLayout = new QHBoxLayout;
  toolBarLayout->setContentsMargins(0, 0, 0, 0);
  toolBarLayout->setSpacing(0);
  m_toolBarWidget = new StyledBar;
  m_toolBarWidget->setLayout(toolBarLayout);
  m_stretchWidget = new QWidget;
  m_stretchWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_toolBar = new QToolBar;
  m_toolBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
  m_toolBar->addWidget(m_stretchWidget);
  m_toolBarWidget->layout()->addWidget(m_toolBar);

  m_cursorPositionLabel = new LineColumnLabel(q);
  const auto spacing = q->style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing) / 2;
  m_cursorPositionLabel->setContentsMargins(spacing, 0, spacing, 0);
  m_toolBarWidget->layout()->addWidget(m_cursorPositionLabel);

  m_fileLineEnding = new QComboBox();
  m_fileLineEnding->addItems(ExtraEncodingSettings::lineTerminationModeNames());
  m_fileLineEnding->setContentsMargins(spacing, 0, spacing, 0);
  m_fileLineEndingAction = m_toolBar->addWidget(m_fileLineEnding);
  m_fileLineEndingAction->setVisible(!q->isReadOnly());
  connect(q, &TextEditorWidget::readOnlyChanged, this, [this] {
    m_fileLineEndingAction->setVisible(!q->isReadOnly());
  });

  m_fileEncodingLabel = new FixedSizeClickLabel;
  m_fileEncodingLabel->setContentsMargins(spacing, 0, spacing, 0);
  m_fileEncodingLabelAction = m_toolBar->addWidget(m_fileEncodingLabel);

  m_extraSelections.reserve(NExtraSelectionKinds);
}

TextEditorWidgetPrivate::~TextEditorWidgetPrivate()
{
  const auto doc = m_document->document();
  QTC_CHECK(doc);
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
  QTC_CHECK(documentLayout);
  QTC_CHECK(m_document.data());
  documentLayout->disconnect(this);
  documentLayout->disconnect(m_extraArea);
  doc->disconnect(this);
  m_document.data()->disconnect(this);
  q->disconnect(documentLayout);
  q->disconnect(this);
  delete m_toolBarWidget;
  delete m_highlightScrollBarController;
}

static auto createSeparator(const QString &styleSheet) -> QFrame*
{
  const auto separator = new QFrame();
  separator->setStyleSheet(styleSheet);
  separator->setFrameShape(QFrame::HLine);
  auto sizePolicy = separator->sizePolicy();
  sizePolicy.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
  separator->setSizePolicy(sizePolicy);

  return separator;
}

static auto createSeparatorLayout() -> QLayout*
{
  const QString styleSheet = "color: gray";

  const auto separator1 = createSeparator(styleSheet);
  const auto separator2 = createSeparator(styleSheet);
  const auto label = new QLabel(TextEditorWidget::tr("Other annotations"));
  label->setStyleSheet(styleSheet);

  const auto layout = new QHBoxLayout;
  layout->addWidget(separator1);
  layout->addWidget(label);
  layout->addWidget(separator2);

  return layout;
}

auto TextEditorWidgetPrivate::showTextMarksToolTip(const QPoint &pos, const TextMarks &marks, const TextMark *mainTextMark) const -> void
{
  if (!mainTextMark && marks.isEmpty())
    return; // Nothing to show

  auto allMarks = marks;

  const auto layout = new QGridLayout;
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  if (mainTextMark) {
    mainTextMark->addToToolTipLayout(layout);
    if (allMarks.size() > 1)
      layout->addLayout(createSeparatorLayout(), layout->rowCount(), 0, 1, -1);
  }

  sort(allMarks, [](const TextMark *mark1, const TextMark *mark2) {
    return mark1->priority() > mark2->priority();
  });

  for (const TextMark *mark : qAsConst(allMarks)) {
    if (mark != mainTextMark)
      mark->addToToolTipLayout(layout);
  }

  layout->addWidget(DisplaySettings::createAnnotationSettingsLink(), layout->rowCount(), 0, 1, -1, Qt::AlignRight);
  ToolTip::show(pos, layout, q);
}

} // namespace Internal

auto TextEditorWidget::plainTextFromSelection(const QTextCursor &cursor) const -> QString
{
  // Copy the selected text as plain text
  const auto text = cursor.selectedText();
  return convertToPlainText(text);
}

auto TextEditorWidget::plainTextFromSelection(const MultiTextCursor &cursor) const -> QString
{
  return convertToPlainText(cursor.selectedText());
}

auto TextEditorWidget::convertToPlainText(const QString &txt) -> QString
{
  auto ret = txt;
  auto uc = ret.data();
  const auto e = uc + ret.size();

  for (; uc != e; ++uc) {
    switch (uc->unicode()) {
    case 0xfdd0: // QTextBeginningOfFrame
    case 0xfdd1: // QTextEndOfFrame
    case QChar::ParagraphSeparator:
    case QChar::LineSeparator:
      *uc = QLatin1Char('\n');
      break;
    case QChar::Nbsp:
      *uc = QLatin1Char(' ');
      break;
    default: ;
    }
  }
  return ret;
}

static const char kTextBlockMimeType[] = "application/vnd.qtcreator.blocktext";

Id TextEditorWidget::SnippetPlaceholderSelection("TextEdit.SnippetPlaceHolderSelection");
Id TextEditorWidget::CurrentLineSelection("TextEdit.CurrentLineSelection");
Id TextEditorWidget::ParenthesesMatchingSelection("TextEdit.ParenthesesMatchingSelection");
Id TextEditorWidget::AutoCompleteSelection("TextEdit.AutoCompleteSelection");
Id TextEditorWidget::CodeWarningsSelection("TextEdit.CodeWarningsSelection");
Id TextEditorWidget::CodeSemanticsSelection("TextEdit.CodeSemanticsSelection");
Id TextEditorWidget::CursorSelection("TextEdit.CursorSelection");
Id TextEditorWidget::UndefinedSymbolSelection("TextEdit.UndefinedSymbolSelection");
Id TextEditorWidget::UnusedSymbolSelection("TextEdit.UnusedSymbolSelection");
Id TextEditorWidget::OtherSelection("TextEdit.OtherSelection");
Id TextEditorWidget::ObjCSelection("TextEdit.ObjCSelection");
Id TextEditorWidget::DebuggerExceptionSelection("TextEdit.DebuggerExceptionSelection");
Id TextEditorWidget::FakeVimSelection("TextEdit.FakeVimSelection");

TextEditorWidget::TextEditorWidget(QWidget *parent) : QPlainTextEdit(parent)
{
  // "Needed", as the creation below triggers ChildEvents that are
  // passed to this object's event() which uses 'd'.
  d = nullptr;
  d = new TextEditorWidgetPrivate(this);
}

auto TextEditorWidget::setTextDocument(const QSharedPointer<TextDocument> &doc) -> void
{
  d->ctor(doc);
}

auto TextEditorWidgetPrivate::setupScrollBar() -> void
{
  if (m_displaySettings.m_scrollBarHighlights) {
    if (!m_highlightScrollBarController)
      m_highlightScrollBarController = new HighlightScrollBarController();

    m_highlightScrollBarController->setScrollArea(q);
    highlightSearchResultsInScrollBar();
    scheduleUpdateHighlightScrollBar();
  } else if (m_highlightScrollBarController) {
    delete m_highlightScrollBarController;
    m_highlightScrollBarController = nullptr;
  }
}

auto TextEditorWidgetPrivate::ctor(const QSharedPointer<TextDocument> &doc) -> void
{
  q->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

  m_overlay = new TextEditorOverlay(q);
  m_snippetOverlay = new SnippetOverlay(q);
  m_searchResultOverlay = new TextEditorOverlay(q);
  m_refactorOverlay = new RefactorOverlay(q);

  m_document = doc;
  setupDocumentSignals();
  m_blockCount = doc->document()->blockCount();

  // from RESEARCH

  q->setLayoutDirection(Qt::LeftToRight);
  q->viewport()->setMouseTracking(true);

  extraAreaSelectionAnchorBlockNumber = -1;
  extraAreaToggleMarkBlockNumber = -1;
  extraAreaHighlightFoldedBlockNumber = -1;
  visibleFoldedBlockNumber = -1;
  suggestedVisibleFoldedBlockNumber = -1;

  connect(&m_codeAssistant, &CodeAssistant::finished, q, &TextEditorWidget::assistFinished);

  connect(q, &QPlainTextEdit::blockCountChanged, this, &TextEditorWidgetPrivate::slotUpdateExtraAreaWidth);

  connect(q, &QPlainTextEdit::modificationChanged, m_extraArea, QOverload<>::of(&QWidget::update));

  connect(q, &QPlainTextEdit::cursorPositionChanged, q, &TextEditorWidget::slotCursorPositionChanged);

  connect(q, &QPlainTextEdit::cursorPositionChanged, this, &TextEditorWidgetPrivate::updateCursorPosition);

  connect(q, &QPlainTextEdit::updateRequest, this, &TextEditorWidgetPrivate::slotUpdateRequest);

  connect(q, &QPlainTextEdit::selectionChanged, this, &TextEditorWidgetPrivate::slotSelectionChanged);

  // parentheses matcher
  m_formatRange = true;
  m_parenthesesMatchingTimer.setSingleShot(true);
  m_parenthesesMatchingTimer.setInterval(50);
  connect(&m_parenthesesMatchingTimer, &QTimer::timeout, this, &TextEditorWidgetPrivate::_q_matchParentheses);

  m_highlightBlocksTimer.setSingleShot(true);
  connect(&m_highlightBlocksTimer, &QTimer::timeout, this, &TextEditorWidgetPrivate::_q_highlightBlocks);

  m_scrollBarUpdateTimer.setSingleShot(true);
  connect(&m_scrollBarUpdateTimer, &QTimer::timeout, this, &TextEditorWidgetPrivate::highlightSearchResultsInScrollBar);

  m_bracketsAnimator = nullptr;
  m_autocompleteAnimator = nullptr;

  slotUpdateExtraAreaWidth();
  updateHighlights();
  q->setFrameStyle(QFrame::NoFrame);

  m_delayedUpdateTimer.setSingleShot(true);
  connect(&m_delayedUpdateTimer, &QTimer::timeout, q->viewport(), QOverload<>::of(&QWidget::update));

  m_moveLineUndoHack = false;

  updateCannotDecodeInfo();

  connect(m_document.data(), &TextDocument::aboutToOpen, q, &TextEditorWidget::aboutToOpen);
  connect(m_document.data(), &TextDocument::openFinishedSuccessfully, q, &TextEditorWidget::openFinishedSuccessfully);
  connect(m_fileEncodingLabel, &FixedSizeClickLabel::clicked, q, &TextEditorWidget::selectEncoding);
  connect(m_document->document(), &QTextDocument::modificationChanged, q, &TextEditorWidget::updateTextCodecLabel);
  q->updateTextCodecLabel();

  connect(m_fileLineEnding, QOverload<int>::of(&QComboBox::currentIndexChanged), q, &TextEditorWidget::selectLineEnding);
  connect(m_document->document(), &QTextDocument::modificationChanged, q, &TextEditorWidget::updateTextLineEndingLabel);
  q->updateTextLineEndingLabel();
}

TextEditorWidget::~TextEditorWidget()
{
  delete d;
  d = nullptr;
}

auto TextEditorWidget::print(QPrinter *printer) -> void
{
  const auto oldFullPage = printer->fullPage();
  printer->setFullPage(true);
  const auto dlg = new QPrintDialog(printer, this);
  dlg->setWindowTitle(tr("Print Document"));
  if (dlg->exec() == QDialog::Accepted)
    d->print(printer);
  printer->setFullPage(oldFullPage);
  delete dlg;
}

static auto foldBoxWidth(const QFontMetrics &fm) -> int
{
  const auto lineSpacing = fm.lineSpacing();
  return lineSpacing + lineSpacing % 2 + 1;
}

static auto printPage(int index, QPainter *painter, const QTextDocument *doc, const QRectF &body, const QRectF &titleBox, const QString &title) -> void
{
  painter->save();

  painter->translate(body.left(), body.top() - (index - 1) * body.height());
  const QRectF view(0, (index - 1) * body.height(), body.width(), body.height());

  const auto layout = doc->documentLayout();
  QAbstractTextDocumentLayout::PaintContext ctx;

  painter->setFont(QFont(doc->defaultFont()));
  const auto box = titleBox.translated(0, view.top());
  const auto dpix = painter->device()->logicalDpiX();
  const auto dpiy = painter->device()->logicalDpiY();
  const auto mx = int(5 * dpix / 72.0);
  const auto my = int(2 * dpiy / 72.0);
  painter->fillRect(box.adjusted(-mx, -my, mx, my), QColor(210, 210, 210));
  if (!title.isEmpty())
    painter->drawText(box, Qt::AlignCenter, title);
  const auto pageString = QString::number(index);
  painter->drawText(box, Qt::AlignRight, pageString);

  painter->setClipRect(view);
  ctx.clip = view;
  // don't use the system palette text as default text color, on HP/UX
  // for example that's white, and white text on white paper doesn't
  // look that nice
  ctx.palette.setColor(QPalette::Text, Qt::black);

  layout->draw(painter, ctx);

  painter->restore();
}

Q_LOGGING_CATEGORY(printLog, "qtc.editor.print", QtWarningMsg)

auto TextEditorWidgetPrivate::print(QPrinter *printer) -> void
{
  auto doc = q->document();

  QString title = m_document->displayName();
  if (!title.isEmpty())
    printer->setDocName(title);

  QPainter p(printer);

  // Check that there is a valid device to print to.
  if (!p.isActive())
    return;

  QRectF pageRect(printer->pageLayout().paintRectPixels(printer->resolution()));
  if (pageRect.isEmpty())
    return;

  doc = doc->clone(doc);
  ExecuteOnDestruction docDeleter([doc]() { delete doc; });

  auto opt = doc->defaultTextOption();
  opt.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
  doc->setDefaultTextOption(opt);

  (void)doc->documentLayout(); // make sure that there is a layout

  auto background = m_document->fontSettings().toTextCharFormat(C_TEXT).background().color();
  auto backgroundIsDark = background.value() < 128;

  for (auto srcBlock = q->document()->firstBlock(), dstBlock = doc->firstBlock(); srcBlock.isValid() && dstBlock.isValid(); srcBlock = srcBlock.next(), dstBlock = dstBlock.next()) {

    auto formatList = srcBlock.layout()->formats();
    if (backgroundIsDark) {
      // adjust syntax highlighting colors for better contrast
      for (int i = formatList.count() - 1; i >= 0; --i) {
        auto &format = formatList[i].format;
        if (format.background().color() == background) {
          auto brush = format.foreground();
          auto color = brush.color();
          int h, s, v, a;
          color.getHsv(&h, &s, &v, &a);
          color.setHsv(h, s, qMin(128, v), a);
          brush.setColor(color);
          format.setForeground(brush);
        }
        format.setBackground(Qt::white);
      }
    }

    dstBlock.layout()->setFormats(formatList);
  }

  auto layout = doc->documentLayout();
  layout->setPaintDevice(p.device());

  auto dpiy = qRound(QGuiApplication::primaryScreen()->logicalDotsPerInchY());
  auto margin = int(2 / 2.54 * dpiy); // 2 cm margins

  auto fmt = doc->rootFrame()->frameFormat();
  fmt.setMargin(margin);
  doc->rootFrame()->setFrameFormat(fmt);

  auto body = QRectF(0, 0, pageRect.width(), pageRect.height());
  QFontMetrics fontMetrics(doc->defaultFont(), p.device());

  QRectF titleBox(margin, body.top() + margin - fontMetrics.height() - 6 * dpiy / 72.0, body.width() - 2 * margin, fontMetrics.height());
  doc->setPageSize(body.size());

  int docCopies;
  int pageCopies;
  if (printer->collateCopies() == true) {
    docCopies = 1;
    pageCopies = printer->copyCount();
  } else {
    docCopies = printer->copyCount();
    pageCopies = 1;
  }

  auto fromPage = printer->fromPage();
  auto toPage = printer->toPage();
  auto ascending = true;

  if (fromPage == 0 && toPage == 0) {
    fromPage = 1;
    toPage = doc->pageCount();
  }
  // paranoia check
  fromPage = qMax(1, fromPage);
  toPage = qMin(doc->pageCount(), toPage);

  if (printer->pageOrder() == QPrinter::LastPageFirst) {
    auto tmp = fromPage;
    fromPage = toPage;
    toPage = tmp;
    ascending = false;
  }

  qCDebug(printLog) << "Printing " << m_document->filePath() << ":\n" << "  number of copies:" << printer->copyCount() << '\n' << "  from page" << fromPage << "to" << toPage << '\n' << "  document page count:" << doc->pageCount() << '\n' << "  page rectangle:" << pageRect << '\n' << "  title box:" << titleBox << '\n';

  for (auto i = 0; i < docCopies; ++i) {

    auto page = fromPage;
    while (true) {
      for (auto j = 0; j < pageCopies; ++j) {
        if (printer->printerState() == QPrinter::Aborted || printer->printerState() == QPrinter::Error)
          return;
        printPage(page, &p, doc, body, titleBox, title);
        if (j < pageCopies - 1)
          printer->newPage();
      }

      if (page == toPage)
        break;

      if (ascending)
        ++page;
      else
        --page;

      printer->newPage();
    }

    if (i < docCopies - 1)
      printer->newPage();
  }
}

auto TextEditorWidgetPrivate::visualIndent(const QTextBlock &block) const -> int
{
  if (!block.isValid())
    return 0;
  const auto document = block.document();
  auto i = 0;
  while (i < block.length()) {
    if (!document->characterAt(block.position() + i).isSpace()) {
      QTextCursor cursor(block);
      cursor.setPosition(block.position() + i);
      return q->cursorRect(cursor).x();
    }
    ++i;
  }

  return 0;
}

auto TextEditorWidgetPrivate::updateAutoCompleteHighlight() -> void
{
  const auto matchFormat = m_document->fontSettings().toTextCharFormat(C_AUTOCOMPLETE);

  QList<QTextEdit::ExtraSelection> extraSelections;
  for (const auto &cursor : qAsConst(m_autoCompleteHighlightPos)) {
    QTextEdit::ExtraSelection sel;
    sel.cursor = cursor;
    sel.format.setBackground(matchFormat.background());
    extraSelections.append(sel);
  }
  q->setExtraSelections(TextEditorWidget::AutoCompleteSelection, extraSelections);
}

auto TextEditorWidgetPrivate::generateCursorsForBlockSelection(const BlockSelection &blockSelection) -> QList<QTextCursor>
{
  const auto tabSettings = m_document->tabSettings();

  QList<QTextCursor> result;
  auto block = m_document->document()->findBlockByNumber(blockSelection.anchorBlockNumber);
  QTextCursor cursor(block);
  cursor.setPosition(block.position() + tabSettings.positionAtColumn(block.text(), blockSelection.anchorColumn));

  const auto forward = blockSelection.blockNumber > blockSelection.anchorBlockNumber || blockSelection.blockNumber == blockSelection.anchorBlockNumber && blockSelection.column == blockSelection.anchorColumn;

  while (block.isValid()) {
    const auto &blockText = block.text();
    const auto columnCount = tabSettings.columnCountForText(blockText);
    if (blockSelection.anchorColumn <= columnCount || blockSelection.column <= columnCount) {
      const auto anchor = tabSettings.positionAtColumn(blockText, blockSelection.anchorColumn);
      const auto position = tabSettings.positionAtColumn(blockText, blockSelection.column);
      cursor.setPosition(block.position() + anchor);
      cursor.setPosition(block.position() + position, QTextCursor::KeepAnchor);
      result.append(cursor);
    }
    if (block.blockNumber() == blockSelection.blockNumber)
      break;
    block = forward ? block.next() : block.previous();
  }
  return result;
}

auto TextEditorWidgetPrivate::initBlockSelection() -> void
{
  const auto tabSettings = m_document->tabSettings();
  for (const auto &cursor : m_cursors) {
    const auto column = tabSettings.columnAtCursorPosition(cursor);
    auto anchor = cursor;
    anchor.setPosition(anchor.anchor());
    const auto anchorColumn = tabSettings.columnAtCursorPosition(anchor);
    m_blockSelections.append({cursor.blockNumber(), column, anchor.blockNumber(), anchorColumn});
  }
}

auto TextEditorWidgetPrivate::clearBlockSelection() -> void
{
  m_blockSelections.clear();
}

auto TextEditorWidgetPrivate::handleMoveBlockSelection(QTextCursor::MoveOperation op) -> void
{
  if (m_blockSelections.isEmpty())
    initBlockSelection();
  QList<QTextCursor> cursors;
  for (auto &blockSelection : m_blockSelections) {
    switch (op) {
    case QTextCursor::Up:
      blockSelection.blockNumber = qMax(0, blockSelection.blockNumber - 1);
      break;
    case QTextCursor::Down:
      blockSelection.blockNumber = qMin(m_document->document()->blockCount() - 1, blockSelection.blockNumber + 1);
      break;
    case QTextCursor::NextCharacter:
      ++blockSelection.column;
      break;
    case QTextCursor::PreviousCharacter:
      blockSelection.column = qMax(0, blockSelection.column - 1);
      break;
    default:
      return;
    }
    cursors.append(generateCursorsForBlockSelection(blockSelection));
  }
  q->setMultiTextCursor(MultiTextCursor(cursors));
}

auto TextEditorWidget::selectEncoding() -> void
{
  auto doc = d->m_document.data();
  CodecSelector codecSelector(this, doc);

  switch (codecSelector.exec()) {
  case CodecSelector::Reload: {
    QString errorString;
    if (!doc->reload(&errorString, codecSelector.selectedCodec())) {
      QMessageBox::critical(this, tr("File Error"), errorString);
      break;
    }
    break;
  }
  case CodecSelector::Save:
    doc->setCodec(codecSelector.selectedCodec());
    EditorManager::saveDocument(textDocument());
    updateTextCodecLabel();
    break;
  case CodecSelector::Cancel:
    break;
  }
}

auto TextEditorWidget::selectLineEnding(int index) -> void
{
  QTC_CHECK(index >= 0);
  const auto newMode = TextFileFormat::LineTerminationMode(index);
  if (d->m_document->lineTerminationMode() != newMode) {
    d->m_document->setLineTerminationMode(newMode);
    d->q->document()->setModified(true);
  }
}

auto TextEditorWidget::updateTextLineEndingLabel() -> void
{
  d->m_fileLineEnding->setCurrentIndex(d->m_document->lineTerminationMode());
}

auto TextEditorWidget::updateTextCodecLabel() -> void
{
  const auto text = QString::fromLatin1(d->m_document->codec()->name());
  d->m_fileEncodingLabel->setText(text, text);
}

auto TextEditorWidget::msgTextTooLarge(quint64 size) -> QString
{
  return tr("The text is too large to be displayed (%1 MB).").arg(size >> 20);
}

auto TextEditorWidget::insertPlainText(const QString &text) -> void
{
  auto cursor = d->m_cursors;
  cursor.insertText(text);
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::selectedText() const -> QString
{
  return d->m_cursors.selectedText();
}

auto TextEditorWidgetPrivate::updateCannotDecodeInfo() -> void
{
  q->setReadOnly(m_document->hasDecodingError());
  InfoBar *infoBar = m_document->infoBar();
  Id selectEncodingId(Constants::SELECT_ENCODING);
  if (m_document->hasDecodingError()) {
    if (!infoBar->canInfoBeAdded(selectEncodingId))
      return;
    InfoBarEntry info(selectEncodingId, TextEditorWidget::tr("<b>Error:</b> Could not decode \"%1\" with \"%2\"-encoding. Editing not possible.").arg(m_document->displayName(), QString::fromLatin1(m_document->codec()->name())));
    info.addCustomButton(TextEditorWidget::tr("Select Encoding"), [this]() { q->selectEncoding(); });
    infoBar->addInfo(info);
  } else {
    infoBar->removeInfo(selectEncodingId);
  }
}

// Skip over shebang to license header (Python, Perl, sh)
// '#!/bin/sh'
// ''
// '###############'

static auto skipShebang(const QTextBlock &block) -> QTextBlock
{
  if (!block.isValid() || !block.text().startsWith("#!"))
    return block;
  const auto nextBlock1 = block.next();
  if (!nextBlock1.isValid() || !nextBlock1.text().isEmpty())
    return block;
  const auto nextBlock2 = nextBlock1.next();
  return nextBlock2.isValid() && nextBlock2.text().startsWith('#') ? nextBlock2 : block;
}

/*
  Collapses the first comment in a file, if there is only whitespace/shebang line
  above
  */
auto TextEditorWidgetPrivate::foldLicenseHeader() -> void
{
  const auto doc = q->document();
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
  QTC_ASSERT(documentLayout, return);
  auto block = skipShebang(doc->firstBlock());
  while (block.isValid() && block.isVisible()) {
    auto text = block.text();
    if (TextDocumentLayout::canFold(block) && block.next().isVisible()) {
      const auto trimmedText = text.trimmed();
      QStringList commentMarker;
      if (const auto highlighter = qobject_cast<Highlighter*>(q->textDocument()->syntaxHighlighter())) {
        const auto def = highlighter->definition();
        for (const auto &marker : {def.singleLineCommentMarker(), def.multiLineCommentMarker().first}) {
          if (!marker.isEmpty())
            commentMarker << marker;
        }
      } else {
        commentMarker = QStringList({"/*", "#"});
      }

      if (anyOf(commentMarker, [&](const QString &marker) {
        return trimmedText.startsWith(marker);
      })) {
        TextDocumentLayout::doFoldOrUnfold(block, false);
        moveCursorVisible();
        documentLayout->requestUpdate();
        documentLayout->emitDocumentSizeChanged();
        break;
      }
    }
    if (TabSettings::firstNonSpace(text) < text.size())
      break;
    block = block.next();
  }
}

auto TextEditorWidget::textDocument() const -> TextDocument*
{
  return d->m_document.data();
}

auto TextEditorWidget::aboutToOpen(const FilePath &filePath, const FilePath &realFilePath) -> void
{
  Q_UNUSED(filePath)
  Q_UNUSED(realFilePath)
}

auto TextEditorWidget::openFinishedSuccessfully() -> void
{
  d->moveCursor(QTextCursor::Start);
  d->updateCannotDecodeInfo();
  updateTextCodecLabel();
  updateVisualWrapColumn();
}

auto TextEditorWidget::textDocumentPtr() const -> TextDocumentPtr
{
  return d->m_document;
}

auto TextEditorWidget::currentTextEditorWidget() -> TextEditorWidget*
{
  return fromEditor(EditorManager::currentEditor());
}

auto TextEditorWidget::fromEditor(const IEditor *editor) -> TextEditorWidget*
{
  if (editor)
    return Aggregation::query<TextEditorWidget>(editor->widget());
  return nullptr;
}

auto TextEditorWidgetPrivate::editorContentsChange(int position, int charsRemoved, int charsAdded) -> void
{
  if (m_bracketsAnimator)
    m_bracketsAnimator->finish();

  m_contentsChanged = true;
  const auto doc = q->document();
  const auto documentLayout = static_cast<TextDocumentLayout*>(doc->documentLayout());
  const auto posBlock = doc->findBlock(position);

  // Keep the line numbers and the block information for the text marks updated
  if (charsRemoved != 0) {
    documentLayout->updateMarksLineNumber();
    documentLayout->updateMarksBlock(posBlock);
  } else {
    const auto nextBlock = doc->findBlock(position + charsAdded);
    if (posBlock != nextBlock) {
      documentLayout->updateMarksLineNumber();
      documentLayout->updateMarksBlock(posBlock);
      documentLayout->updateMarksBlock(nextBlock);
    } else {
      documentLayout->updateMarksBlock(posBlock);
    }
  }

  if (m_snippetOverlay->isVisible()) {
    auto cursor = q->textCursor();
    cursor.setPosition(position);
    snippetCheckCursor(cursor);
  }

  if (charsAdded != 0 && q->document()->characterAt(position + charsAdded - 1).isPrint() || charsRemoved != 0)
    m_codeAssistant.notifyChange();

  const auto newBlockCount = doc->blockCount();
  if (!q->hasFocus() && newBlockCount != m_blockCount) {
    // lines were inserted or removed from outside, keep viewport on same part of text
    if (q->firstVisibleBlock().blockNumber() > posBlock.blockNumber())
      q->verticalScrollBar()->setValue(q->verticalScrollBar()->value() + newBlockCount - m_blockCount);
  }
  m_blockCount = newBlockCount;
  m_scrollBarUpdateTimer.start(500);
}

auto TextEditorWidgetPrivate::slotSelectionChanged() -> void
{
  if (!q->textCursor().hasSelection() && !m_selectBlockAnchor.isNull())
    m_selectBlockAnchor = QTextCursor();
  // Clear any link which might be showing when the selection changes
  clearLink();
  setClipboardSelection();
}

auto TextEditorWidget::gotoBlockStart() -> void
{
  if (multiTextCursor().hasMultipleCursors())
    return;

  auto cursor = textCursor();
  if (TextBlockUserData::findPreviousOpenParenthesis(&cursor, false)) {
    setTextCursor(cursor);
    d->_q_matchParentheses();
  }
}

auto TextEditorWidget::gotoBlockEnd() -> void
{
  if (multiTextCursor().hasMultipleCursors())
    return;

  auto cursor = textCursor();
  if (TextBlockUserData::findNextClosingParenthesis(&cursor, false)) {
    setTextCursor(cursor);
    d->_q_matchParentheses();
  }
}

auto TextEditorWidget::gotoBlockStartWithSelection() -> void
{
  if (multiTextCursor().hasMultipleCursors())
    return;

  auto cursor = textCursor();
  if (TextBlockUserData::findPreviousOpenParenthesis(&cursor, true)) {
    setTextCursor(cursor);
    d->_q_matchParentheses();
  }
}

auto TextEditorWidget::gotoBlockEndWithSelection() -> void
{
  if (multiTextCursor().hasMultipleCursors())
    return;

  auto cursor = textCursor();
  if (TextBlockUserData::findNextClosingParenthesis(&cursor, true)) {
    setTextCursor(cursor);
    d->_q_matchParentheses();
  }
}

auto TextEditorWidget::gotoDocumentStart() -> void
{
  d->moveCursor(QTextCursor::Start);
}

auto TextEditorWidget::gotoDocumentEnd() -> void
{
  d->moveCursor(QTextCursor::End);
}

auto TextEditorWidget::gotoLineStart() -> void
{
  d->handleHomeKey(false, true);
}

auto TextEditorWidget::gotoLineStartWithSelection() -> void
{
  d->handleHomeKey(true, true);
}

auto TextEditorWidget::gotoLineEnd() -> void
{
  d->moveCursor(QTextCursor::EndOfLine);
}

auto TextEditorWidget::gotoLineEndWithSelection() -> void
{
  d->moveCursor(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
}

auto TextEditorWidget::gotoNextLine() -> void
{
  d->moveCursor(QTextCursor::Down);
}

auto TextEditorWidget::gotoNextLineWithSelection() -> void
{
  d->moveCursor(QTextCursor::Down, QTextCursor::KeepAnchor);
}

auto TextEditorWidget::gotoPreviousLine() -> void
{
  d->moveCursor(QTextCursor::Up);
}

auto TextEditorWidget::gotoPreviousLineWithSelection() -> void
{
  d->moveCursor(QTextCursor::Up, QTextCursor::KeepAnchor);
}

auto TextEditorWidget::gotoPreviousCharacter() -> void
{
  d->moveCursor(QTextCursor::PreviousCharacter);
}

auto TextEditorWidget::gotoPreviousCharacterWithSelection() -> void
{
  d->moveCursor(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
}

auto TextEditorWidget::gotoNextCharacter() -> void
{
  d->moveCursor(QTextCursor::NextCharacter);
}

auto TextEditorWidget::gotoNextCharacterWithSelection() -> void
{
  d->moveCursor(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
}

auto TextEditorWidget::gotoPreviousWord() -> void
{
  d->moveCursor(QTextCursor::PreviousWord);
}

auto TextEditorWidget::gotoPreviousWordWithSelection() -> void
{
  d->moveCursor(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
}

auto TextEditorWidget::gotoNextWord() -> void
{
  d->moveCursor(QTextCursor::NextWord);
}

auto TextEditorWidget::gotoNextWordWithSelection() -> void
{
  d->moveCursor(QTextCursor::NextWord, QTextCursor::KeepAnchor);
}

auto TextEditorWidget::gotoPreviousWordCamelCase() -> void
{
  auto cursor = multiTextCursor();
  CamelCaseCursor::left(&cursor, this, QTextCursor::MoveAnchor);
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::gotoPreviousWordCamelCaseWithSelection() -> void
{
  auto cursor = multiTextCursor();
  CamelCaseCursor::left(&cursor, this, QTextCursor::KeepAnchor);
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::gotoNextWordCamelCase() -> void
{
  auto cursor = multiTextCursor();
  CamelCaseCursor::right(&cursor, this, QTextCursor::MoveAnchor);
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::gotoNextWordCamelCaseWithSelection() -> void
{
  auto cursor = multiTextCursor();
  CamelCaseCursor::right(&cursor, this, QTextCursor::KeepAnchor);
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::selectBlockUp() -> bool
{
  if (multiTextCursor().hasMultipleCursors())
    return false;

  auto cursor = textCursor();
  if (!cursor.hasSelection())
    d->m_selectBlockAnchor = cursor;
  else
    cursor.setPosition(cursor.selectionStart());

  if (!TextBlockUserData::findPreviousOpenParenthesis(&cursor, false))
    return false;
  if (!TextBlockUserData::findNextClosingParenthesis(&cursor, true))
    return false;

  setTextCursor(Text::flippedCursor(cursor));
  d->_q_matchParentheses();
  return true;
}

auto TextEditorWidget::selectBlockDown() -> bool
{
  if (multiTextCursor().hasMultipleCursors())
    return false;

  auto tc = textCursor();
  auto cursor = d->m_selectBlockAnchor;

  if (!tc.hasSelection() || cursor.isNull())
    return false;
  tc.setPosition(tc.selectionStart());

  forever {
    auto ahead = cursor;
    if (!TextBlockUserData::findPreviousOpenParenthesis(&ahead, false))
      break;
    if (ahead.position() <= tc.position())
      break;
    cursor = ahead;
  }
  if (cursor != d->m_selectBlockAnchor)
    TextBlockUserData::findNextClosingParenthesis(&cursor, true);

  setTextCursor(Text::flippedCursor(cursor));
  d->_q_matchParentheses();
  return true;
}

auto TextEditorWidget::selectWordUnderCursor() -> void
{
  auto cursor = multiTextCursor();
  for (auto &c : cursor) {
    if (!c.hasSelection())
      c.select(QTextCursor::WordUnderCursor);
  }
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::showContextMenu() -> void
{
  const auto tc = textCursor();
  const auto cursorPos = mapToGlobal(cursorRect(tc).bottomRight() + QPoint(1, 1));
  qGuiApp->postEvent(this, new QContextMenuEvent(QContextMenuEvent::Keyboard, cursorPos));
}

auto TextEditorWidget::copyLineUp() -> void
{
  d->copyLineUpDown(true);
}

auto TextEditorWidget::copyLineDown() -> void
{
  d->copyLineUpDown(false);
}

// @todo: Potential reuse of some code around the following functions...
auto TextEditorWidgetPrivate::copyLineUpDown(bool up) -> void
{
  if (q->multiTextCursor().hasMultipleCursors())
    return;
  const auto cursor = q->textCursor();
  auto move = cursor;
  move.beginEditBlock();

  const auto hasSelection = cursor.hasSelection();

  if (hasSelection) {
    move.setPosition(cursor.selectionStart());
    move.movePosition(QTextCursor::StartOfBlock);
    move.setPosition(cursor.selectionEnd(), QTextCursor::KeepAnchor);
    move.movePosition(move.atBlockStart() ? QTextCursor::Left : QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
  } else {
    move.movePosition(QTextCursor::StartOfBlock);
    move.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
  }

  const auto text = move.selectedText();

  if (up) {
    move.setPosition(cursor.selectionStart());
    move.movePosition(QTextCursor::StartOfBlock);
    move.insertBlock();
    move.movePosition(QTextCursor::Left);
  } else {
    move.movePosition(QTextCursor::EndOfBlock);
    if (move.atBlockStart()) {
      move.movePosition(QTextCursor::NextBlock);
      move.insertBlock();
      move.movePosition(QTextCursor::Left);
    } else {
      move.insertBlock();
    }
  }

  const auto start = move.position();
  move.clearSelection();
  move.insertText(text);
  const auto end = move.position();

  move.setPosition(start);
  move.setPosition(end, QTextCursor::KeepAnchor);

  m_document->autoIndent(move);
  move.endEditBlock();

  q->setTextCursor(move);
}

auto TextEditorWidget::joinLines() -> void
{
  auto cursor = multiTextCursor();
  cursor.beginEditBlock();
  for (auto &c : cursor) {
    auto start = c;
    auto end = c;

    start.setPosition(c.selectionStart());
    end.setPosition(c.selectionEnd() - 1);

    auto lineCount = qMax(1, end.blockNumber() - start.blockNumber());

    c.setPosition(c.selectionStart());
    while (lineCount--) {
      c.movePosition(QTextCursor::NextBlock);
      c.movePosition(QTextCursor::StartOfBlock);
      c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
      auto cutLine = c.selectedText();

      // Collapse leading whitespaces to one or insert whitespace
      cutLine.replace(QRegularExpression(QLatin1String("^\\s*")), QLatin1String(" "));
      c.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
      c.removeSelectedText();

      c.movePosition(QTextCursor::PreviousBlock);
      c.movePosition(QTextCursor::EndOfBlock);

      c.insertText(cutLine);
    }
  }
  cursor.endEditBlock();
  cursor.mergeCursors();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::insertLineAbove() -> void
{
  auto cursor = multiTextCursor();
  cursor.beginEditBlock();
  for (auto &c : cursor) {
    // If the cursor is at the beginning of the document,
    // it should still insert a line above the current line.
    c.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);
    c.insertBlock();
    c.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor);
    d->m_document->autoIndent(c);
  }
  cursor.endEditBlock();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::insertLineBelow() -> void
{
  auto cursor = multiTextCursor();
  cursor.beginEditBlock();
  for (auto &c : cursor) {
    c.movePosition(QTextCursor::EndOfBlock, QTextCursor::MoveAnchor);
    c.insertBlock();
    d->m_document->autoIndent(c);
  }
  cursor.endEditBlock();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::moveLineUp() -> void
{
  d->moveLineUpDown(true);
}

auto TextEditorWidget::moveLineDown() -> void
{
  d->moveLineUpDown(false);
}

auto TextEditorWidget::uppercaseSelection() -> void
{
  d->transformSelection([](const QString &str) { return str.toUpper(); });
}

auto TextEditorWidget::lowercaseSelection() -> void
{
  d->transformSelection([](const QString &str) { return str.toLower(); });
}

auto TextEditorWidget::sortSelectedLines() -> void
{
  d->transformSelectedLines([](QStringList &list) { list.sort(); });
}

auto TextEditorWidget::indent() -> void
{
  setMultiTextCursor(textDocument()->indent(multiTextCursor()));
}

auto TextEditorWidget::unindent() -> void
{
  setMultiTextCursor(textDocument()->unindent(multiTextCursor()));
}

auto TextEditorWidget::undo() -> void
{
  doSetTextCursor(multiTextCursor().mainCursor());
  QPlainTextEdit::undo();
}

auto TextEditorWidget::redo() -> void
{
  doSetTextCursor(multiTextCursor().mainCursor());
  QPlainTextEdit::redo();
}

auto TextEditorWidget::openLinkUnderCursor() -> void
{
  d->openLinkUnderCursor(alwaysOpenLinksInNextSplit());
}

auto TextEditorWidget::openLinkUnderCursorInNextSplit() -> void
{
  d->openLinkUnderCursor(!alwaysOpenLinksInNextSplit());
}

auto TextEditorWidget::findUsages() -> void
{
  emit requestUsages(textCursor());
}

auto TextEditorWidget::renameSymbolUnderCursor() -> void
{
  emit requestRename(textCursor());
}

auto TextEditorWidget::abortAssist() -> void
{
  d->m_codeAssistant.destroyContext();
}

auto TextEditorWidgetPrivate::moveLineUpDown(bool up) -> void
{
  if (m_cursors.hasMultipleCursors())
    return;
  const auto cursor = q->textCursor();
  auto move = cursor;

  move.setVisualNavigation(false); // this opens folded items instead of destroying them

  if (m_moveLineUndoHack)
    move.joinPreviousEditBlock();
  else
    move.beginEditBlock();

  const auto hasSelection = cursor.hasSelection();

  if (hasSelection) {
    move.setPosition(cursor.selectionStart());
    move.movePosition(QTextCursor::StartOfBlock);
    move.setPosition(cursor.selectionEnd(), QTextCursor::KeepAnchor);
    move.movePosition(move.atBlockStart() ? QTextCursor::PreviousCharacter : QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
  } else {
    move.movePosition(QTextCursor::StartOfBlock);
    move.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
  }
  const auto text = move.selectedText();

  RefactorMarkers affectedMarkers;
  RefactorMarkers nonAffectedMarkers;
  QList<int> markerOffsets;

  foreach(const RefactorMarker &marker, m_refactorOverlay->markers()) {
    //test if marker is part of the selection to be moved
    if (move.selectionStart() <= marker.cursor.position() && move.selectionEnd() >= marker.cursor.position()) {
      affectedMarkers.append(marker);
      //remember the offset of markers in text
      const auto offset = marker.cursor.position() - move.selectionStart();
      markerOffsets.append(offset);
    } else {
      nonAffectedMarkers.append(marker);
    }
  }

  move.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
  move.removeSelectedText();

  if (up) {
    move.movePosition(QTextCursor::PreviousBlock);
    move.insertBlock();
    move.movePosition(QTextCursor::PreviousCharacter);
  } else {
    move.movePosition(QTextCursor::EndOfBlock);
    if (move.atBlockStart()) {
      // empty block
      move.movePosition(QTextCursor::NextBlock);
      move.insertBlock();
      move.movePosition(QTextCursor::PreviousCharacter);
    } else {
      move.insertBlock();
    }
  }

  const auto start = move.position();
  move.clearSelection();
  move.insertText(text);
  const auto end = move.position();

  if (hasSelection) {
    move.setPosition(end);
    move.setPosition(start, QTextCursor::KeepAnchor);
  } else {
    move.setPosition(start);
  }

  //update positions of affectedMarkers
  for (auto i = 0; i < affectedMarkers.count(); i++) {
    const auto newPosition = start + markerOffsets.at(i);
    affectedMarkers[i].cursor.setPosition(newPosition);
  }
  m_refactorOverlay->setMarkers(nonAffectedMarkers + affectedMarkers);

  auto shouldReindent = true;
  if (m_commentDefinition.isValid()) {
    if (m_commentDefinition.hasMultiLineStyle()) {
      // Don't have any single line comments; try multi line.
      if (text.startsWith(m_commentDefinition.multiLineStart) && text.endsWith(m_commentDefinition.multiLineEnd)) {
        shouldReindent = false;
      }
    }
    if (shouldReindent && m_commentDefinition.hasSingleLineStyle()) {
      shouldReindent = false;
      auto block = move.block();
      while (block.isValid() && block.position() < end) {
        if (!block.text().startsWith(m_commentDefinition.singleLine))
          shouldReindent = true;
        block = block.next();
      }
    }
  }

  if (shouldReindent) {
    // The text was not commented at all; re-indent.
    m_document->autoReindent(move);
  }
  move.endEditBlock();

  q->setTextCursor(move);
  m_moveLineUndoHack = true;
}

auto TextEditorWidget::cleanWhitespace() -> void
{
  d->m_document->cleanWhitespace(textCursor());
}

auto TextEditorWidgetPrivate::cursorMoveKeyEvent(QKeyEvent *e) -> bool
{
  auto cursor = m_cursors;
  if (cursor.handleMoveKeyEvent(e, q, q->camelCaseNavigationEnabled())) {
    resetCursorFlashTimer();
    q->setMultiTextCursor(cursor);
    q->ensureCursorVisible();
    updateCurrentLineHighlight();
    return true;
  }
  return false;
}

auto TextEditorWidget::viewPageUp() -> void
{
  verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepSub);
}

auto TextEditorWidget::viewPageDown() -> void
{
  verticalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
}

auto TextEditorWidget::viewLineUp() -> void
{
  verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepSub);
}

auto TextEditorWidget::viewLineDown() -> void
{
  verticalScrollBar()->triggerAction(QAbstractSlider::SliderSingleStepAdd);
}

static inline auto isModifier(QKeyEvent *e) -> bool
{
  if (!e)
    return false;
  switch (e->key()) {
  case Qt::Key_Shift:
  case Qt::Key_Control:
  case Qt::Key_Meta:
  case Qt::Key_Alt:
    return true;
  default:
    return false;
  }
}

static inline auto isPrintableText(const QString &text) -> bool
{
  return !text.isEmpty() && (text.at(0).isPrint() || text.at(0) == QLatin1Char('\t'));
}

auto TextEditorWidget::keyPressEvent(QKeyEvent *e) -> void
{
  ExecuteOnDestruction eod([&]() { d->clearBlockSelection(); });

  if (!isModifier(e) && mouseHidingEnabled())
    viewport()->setCursor(Qt::BlankCursor);
  ToolTip::hide();

  d->m_moveLineUndoHack = false;
  d->clearVisibleFoldedBlock();

  auto cursor = multiTextCursor();

  if (e->key() == Qt::Key_Alt && d->m_behaviorSettings.m_keyboardTooltips) {
    d->m_maybeFakeTooltipEvent = true;
  } else {
    d->m_maybeFakeTooltipEvent = false;
    if (e->key() == Qt::Key_Escape) {
      TextEditorWidgetFind::cancelCurrentSelectAll();
      if (d->m_snippetOverlay->isVisible()) {
        e->accept();
        d->m_snippetOverlay->accept();
        auto cursor = textCursor();
        cursor.clearSelection();
        setTextCursor(cursor);
        return;
      }
      if (cursor.hasMultipleCursors()) {
        auto c = cursor.mainCursor();
        c.setPosition(c.position(), QTextCursor::MoveAnchor);
        doSetTextCursor(c);
        return;
      }
    }
  }

  const auto ro = isReadOnly();
  const auto inOverwriteMode = overwriteMode();
  const auto hasMultipleCursors = cursor.hasMultipleCursors();

  if (!ro && (e == QKeySequence::InsertParagraphSeparator || !d->m_lineSeparatorsAllowed && e == QKeySequence::InsertLineSeparator)) {
    if (d->m_snippetOverlay->isVisible()) {
      e->accept();
      d->m_snippetOverlay->accept();
      auto cursor = textCursor();
      cursor.movePosition(QTextCursor::EndOfBlock);
      setTextCursor(cursor);
      return;
    }

    e->accept();
    cursor.beginEditBlock();
    for (QTextCursor &cursor : cursor) {
      const auto ts = d->m_document->tabSettings();
      const auto &tps = d->m_document->typingSettings();

      auto extraBlocks = d->m_autoCompleter->paragraphSeparatorAboutToBeInserted(cursor);

      QString previousIndentationString;
      if (tps.m_autoIndent) {
        cursor.insertBlock();
        d->m_document->autoIndent(cursor);
      } else {
        cursor.insertBlock();

        // After inserting the block, to avoid duplicating whitespace on the same line
        const auto &previousBlockText = cursor.block().previous().text();
        previousIndentationString = ts.indentationString(previousBlockText);
        if (!previousIndentationString.isEmpty())
          cursor.insertText(previousIndentationString);
      }

      if (extraBlocks > 0) {
        const auto cursorPosition = cursor.position();
        auto ensureVisible = cursor;
        while (extraBlocks > 0) {
          --extraBlocks;
          ensureVisible.movePosition(QTextCursor::NextBlock);
          if (tps.m_autoIndent)
            d->m_document->autoIndent(ensureVisible, QChar::Null, cursorPosition);
          else if (!previousIndentationString.isEmpty())
            ensureVisible.insertText(previousIndentationString);
          if (d->m_animateAutoComplete || d->m_highlightAutoComplete) {
            auto tc = ensureVisible;
            tc.movePosition(QTextCursor::EndOfBlock);
            tc.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
            tc.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
            d->autocompleterHighlight(tc);
          }
        }
        cursor.setPosition(cursorPosition);
      }
    }
    cursor.endEditBlock();
    setMultiTextCursor(cursor);
    ensureCursorVisible();
    return;
  }
  if (!ro && (e == QKeySequence::MoveToStartOfBlock || e == QKeySequence::SelectStartOfBlock || e == QKeySequence::MoveToStartOfLine || e == QKeySequence::SelectStartOfLine)) {
    const auto blockOp = e == QKeySequence::MoveToStartOfBlock || e == QKeySequence::SelectStartOfBlock;
    const auto select = e == QKeySequence::SelectStartOfLine || e == QKeySequence::SelectStartOfBlock;
    d->handleHomeKey(select, blockOp);
    e->accept();
    return;
  }
  if (!ro && e == QKeySequence::DeleteStartOfWord) {
    e->accept();
    if (!cursor.hasSelection()) {
      if (camelCaseNavigationEnabled())
        CamelCaseCursor::left(&cursor, this, QTextCursor::KeepAnchor);
      else
        cursor.movePosition(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
    return;
  }
  if (!ro && e == QKeySequence::DeleteEndOfWord) {
    e->accept();
    if (!cursor.hasSelection()) {
      if (camelCaseNavigationEnabled())
        CamelCaseCursor::right(&cursor, this, QTextCursor::KeepAnchor);
      else
        cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
    }
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
    return;
  }
  if (!ro && e == QKeySequence::DeleteCompleteLine) {
    e->accept();
    for (auto &c : cursor)
      c.select(QTextCursor::BlockUnderCursor);
    cursor.mergeCursors();
    cursor.removeSelectedText();
    setMultiTextCursor(cursor);
    return;
  }
  switch (e->key()) {
    #if 0
    case Qt::Key_Dollar: {
            d->m_overlay->setVisible(!d->m_overlay->isVisible());
            d->m_overlay->setCursor(textCursor());
            e->accept();
        return;

    } break;
    #endif
  case Qt::Key_Tab:
  case Qt::Key_Backtab: {
    if (ro)
      break;
    if (d->m_snippetOverlay->isVisible() && !d->m_snippetOverlay->isEmpty()) {
      d->snippetTabOrBacktab(e->key() == Qt::Key_Tab);
      e->accept();
      return;
    }
    auto cursor = textCursor();
    if (d->m_skipAutoCompletedText && e->key() == Qt::Key_Tab) {
      auto skippedAutoCompletedText = false;
      while (!d->m_autoCompleteHighlightPos.isEmpty() && d->m_autoCompleteHighlightPos.last().selectionStart() == cursor.position()) {
        skippedAutoCompletedText = true;
        cursor.setPosition(d->m_autoCompleteHighlightPos.last().selectionEnd());
        d->m_autoCompleteHighlightPos.pop_back();
      }
      if (skippedAutoCompletedText) {
        setTextCursor(cursor);
        e->accept();
        d->updateAutoCompleteHighlight();
        return;
      }
    }
    int newPosition;
    if (!hasMultipleCursors && d->m_document->typingSettings().tabShouldIndent(document(), cursor, &newPosition)) {
      if (newPosition != cursor.position() && !cursor.hasSelection()) {
        cursor.setPosition(newPosition);
        setTextCursor(cursor);
      }
      d->m_document->autoIndent(cursor);
    } else {
      if (e->key() == Qt::Key_Tab)
        indent();
      else
        unindent();
    }
    e->accept();
    return;
  }
  break;
  case Qt::Key_Backspace:
    if (ro)
      break;
    if ((e->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier)) == Qt::NoModifier) {
      e->accept();
      if (cursor.hasSelection()) {
        cursor.removeSelectedText();
        setMultiTextCursor(cursor);
        return;
      }
      d->handleBackspaceKey();
      return;
    }
    break;
  case Qt::Key_Insert:
    if (ro)
      break;
    if (e->modifiers() == Qt::NoModifier) {
      setOverwriteMode(!inOverwriteMode);
      e->accept();
      return;
    }
    break;
  case Qt::Key_Delete:
    if (hasMultipleCursors && !ro && e->modifiers() == Qt::NoModifier) {
      if (cursor.hasSelection()) {
        cursor.removeSelectedText();
      } else {
        cursor.beginEditBlock();
        for (auto c : cursor)
          c.deleteChar();
        cursor.mergeCursors();
        cursor.endEditBlock();
      }
      e->accept();
      return;
    }
    break;
  default:
    break;
  }

  const auto eventText = e->text();

  if (e->key() == Qt::Key_H && e->modifiers() == Qt::KeyboardModifiers(HostOsInfo::controlModifier())) {
    d->universalHelper();
    e->accept();
    return;
  }

  if (ro || !isPrintableText(eventText)) {
    auto blockSelectionOperation = QTextCursor::NoMove;
    if (e->modifiers() == (Qt::AltModifier | Qt::ShiftModifier) && !HostOsInfo::isMacHost()) {
      if (MultiTextCursor::multiCursorAddEvent(e, QKeySequence::MoveToNextLine))
        blockSelectionOperation = QTextCursor::Down;
      else if (MultiTextCursor::multiCursorAddEvent(e, QKeySequence::MoveToPreviousLine))
        blockSelectionOperation = QTextCursor::Up;
      else if (MultiTextCursor::multiCursorAddEvent(e, QKeySequence::MoveToNextChar))
        blockSelectionOperation = QTextCursor::NextCharacter;
      else if (MultiTextCursor::multiCursorAddEvent(e, QKeySequence::MoveToPreviousChar))
        blockSelectionOperation = QTextCursor::PreviousCharacter;
    }

    if (blockSelectionOperation != QTextCursor::NoMove) {
      auto doNothing = []() {};
      eod.reset(doNothing);
      d->handleMoveBlockSelection(blockSelectionOperation);
    } else if (!d->cursorMoveKeyEvent(e)) {
      auto cursor = textCursor();
      auto cursorWithinSnippet = false;
      if (d->m_snippetOverlay->isVisible() && (e->key() == Qt::Key_Delete || e->key() == Qt::Key_Backspace)) {
        cursorWithinSnippet = d->snippetCheckCursor(cursor);
      }
      if (cursorWithinSnippet)
        cursor.beginEditBlock();

      QPlainTextEdit::keyPressEvent(e);

      if (cursorWithinSnippet) {
        cursor.endEditBlock();
        d->m_snippetOverlay->updateEquivalentSelections(textCursor());
      }
    }
  } else if (hasMultipleCursors) {
    if (inOverwriteMode) {
      cursor.beginEditBlock();
      for (auto &c : cursor) {
        auto block = c.block();
        auto eolPos = block.position() + block.length() - 1;
        int selEndPos = qMin(c.position() + eventText.length(), eolPos);
        c.setPosition(selEndPos, QTextCursor::KeepAnchor);
        c.insertText(eventText);
      }
      cursor.endEditBlock();
    } else {
      cursor.insertText(eventText);
    }
    setMultiTextCursor(cursor);
  } else if ((e->modifiers() & (Qt::ControlModifier | Qt::AltModifier)) != Qt::ControlModifier) {
    // only go here if control is not pressed, except if also alt is pressed
    // because AltGr maps to Alt + Ctrl
    auto cursor = textCursor();
    QString autoText;
    if (!inOverwriteMode) {
      const auto skipChar = d->m_skipAutoCompletedText && !d->m_autoCompleteHighlightPos.isEmpty() && cursor == d->m_autoCompleteHighlightPos.last();
      autoText = autoCompleter()->autoComplete(cursor, eventText, skipChar);
    }
    const auto cursorWithinSnippet = d->snippetCheckCursor(cursor);

    QChar electricChar;
    if (d->m_document->typingSettings().m_autoIndent) {
      foreach(QChar c, eventText) {
        if (d->m_document->indenter()->isElectricCharacter(c)) {
          electricChar = c;
          break;
        }
      }
    }

    auto doEditBlock = !electricChar.isNull() || !autoText.isEmpty() || cursorWithinSnippet;
    if (doEditBlock)
      cursor.beginEditBlock();

    if (inOverwriteMode) {
      if (!doEditBlock)
        cursor.beginEditBlock();
      auto block = cursor.block();
      auto eolPos = block.position() + block.length() - 1;
      int selEndPos = qMin(cursor.position() + eventText.length(), eolPos);
      cursor.setPosition(selEndPos, QTextCursor::KeepAnchor);
      cursor.insertText(eventText);
      if (!doEditBlock)
        cursor.endEditBlock();
    } else {
      cursor.insertText(eventText);
    }

    if (!autoText.isEmpty()) {
      auto pos = cursor.position();
      cursor.insertText(autoText);
      cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
      d->autocompleterHighlight(cursor);
      //Select the inserted text, to be able to re-indent the inserted text
      cursor.setPosition(pos, QTextCursor::KeepAnchor);
    }
    if (!electricChar.isNull() && d->m_autoCompleter->contextAllowsElectricCharacters(cursor))
      d->m_document->autoIndent(cursor, electricChar, cursor.position());
    if (!autoText.isEmpty())
      cursor.setPosition(autoText.length() == 1 ? cursor.position() : cursor.anchor());

    if (doEditBlock) {
      cursor.endEditBlock();
      if (cursorWithinSnippet)
        d->m_snippetOverlay->updateEquivalentSelections(textCursor());
    }

    setTextCursor(cursor);
  }

  if (!ro && e->key() == Qt::Key_Delete && d->m_parenthesesMatchingEnabled)
    d->m_parenthesesMatchingTimer.start();

  if (!ro && d->m_contentsChanged && isPrintableText(eventText) && !inOverwriteMode)
    d->m_codeAssistant.process();
}

class PositionedPart : public ParsedSnippet::Part {
public:
  explicit PositionedPart(const Part &part) : Part(part) {}
  int start;
  int end;
};

class CursorPart : public ParsedSnippet::Part {
public:
  CursorPart(const PositionedPart &part, QTextDocument *doc) : Part(part), cursor(doc)
  {
    cursor.setPosition(part.start);
    cursor.setPosition(part.end, QTextCursor::KeepAnchor);
  }

  QTextCursor cursor;
};

auto TextEditorWidget::insertCodeSnippet(const QTextCursor &cursor_arg, const QString &snippet, const SnippetParser &parse) -> void
{
  const auto result = parse(snippet);
  if (Utils::holds_alternative<SnippetParseError>(result)) {
    const auto &error = Utils::get<SnippetParseError>(result);
    QMessageBox::warning(this, tr("Snippet Parse Error"), error.htmlMessage());
    return;
  }
  QTC_ASSERT(Utils::holds_alternative<ParsedSnippet>(result), return);
  auto data = Utils::get<ParsedSnippet>(result);

  auto cursor = cursor_arg;
  cursor.beginEditBlock();
  cursor.removeSelectedText();
  const auto startCursorPosition = cursor.position();

  d->m_snippetOverlay->accept();

  QList<PositionedPart> positionedParts;
  for (const auto &part : qAsConst(data.parts)) {
    if (part.variableIndex >= 0) {
      PositionedPart posPart(part);
      posPart.start = cursor.position();
      cursor.insertText(part.text);
      posPart.end = cursor.position();
      positionedParts << posPart;
    } else {
      cursor.insertText(part.text);
    }
  }

  auto cursorParts = transform(positionedParts, [doc = document()](const PositionedPart &part) {
    return CursorPart(part, doc);
  });

  cursor.setPosition(startCursorPosition, QTextCursor::KeepAnchor);
  d->m_document->autoIndent(cursor);
  cursor.endEditBlock();

  const auto &occurrencesColor = textDocument()->fontSettings().toTextCharFormat(C_OCCURRENCES).background().color();
  const auto &renameColor = textDocument()->fontSettings().toTextCharFormat(C_OCCURRENCES_RENAME).background().color();

  for (const auto &part : cursorParts) {
    const auto &color = part.cursor.hasSelection() ? occurrencesColor : renameColor;
    if (part.finalPart) {
      d->m_snippetOverlay->setFinalSelection(part.cursor, color);
    } else {
      d->m_snippetOverlay->addSnippetSelection(part.cursor, color, part.mangler, part.variableIndex);
    }
  }

  cursor = d->m_snippetOverlay->firstSelectionCursor();
  if (!cursor.isNull()) {
    setTextCursor(cursor);
    if (d->m_snippetOverlay->isFinalSelection(cursor))
      d->m_snippetOverlay->accept();
    else
      d->m_snippetOverlay->setVisible(true);
  }
}

auto TextEditorWidgetPrivate::universalHelper() -> void
{
  // Test function for development. Place your new fangled experiment here to
  // give it proper scrutiny before pushing it onto others.
}

auto TextEditorWidget::doSetTextCursor(const QTextCursor &cursor, bool keepMultiSelection) -> void
{
  // workaround for QTextControl bug
  const auto selectionChange = cursor.hasSelection() || textCursor().hasSelection();
  auto c = cursor;
  c.setVisualNavigation(true);
  const auto oldCursor = d->m_cursors;
  if (!keepMultiSelection)
    const_cast<MultiTextCursor&>(d->m_cursors).setCursors({c});
  else
    const_cast<MultiTextCursor&>(d->m_cursors).replaceMainCursor(c);
  d->updateCursorSelections();
  d->resetCursorFlashTimer();
  QPlainTextEdit::doSetTextCursor(c);
  if (oldCursor != d->m_cursors) {
    auto updateRect = d->cursorUpdateRect(oldCursor);
    if (d->m_highlightCurrentLine)
      updateRect = QRect(0, updateRect.y(), viewport()->rect().width(), updateRect.height());
    updateRect |= d->cursorUpdateRect(d->m_cursors);
    viewport()->update(updateRect);
    emit cursorPositionChanged();
  }
  if (selectionChange)
    d->slotSelectionChanged();
}

auto TextEditorWidget::doSetTextCursor(const QTextCursor &cursor) -> void
{
  doSetTextCursor(cursor, false);
}

auto TextEditorWidget::gotoLine(int line, int column, bool centerLine, bool animate) -> void
{
  d->m_lastCursorChangeWasInteresting = false; // avoid adding the previous position to history
  const auto blockNumber = qMin(line, document()->blockCount()) - 1;
  const auto &block = document()->findBlockByNumber(blockNumber);
  if (block.isValid()) {
    QTextCursor cursor(block);
    if (column > 0) {
      cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, column);
    } else {
      auto pos = cursor.position();
      while (document()->characterAt(pos).category() == QChar::Separator_Space) {
        ++pos;
      }
      cursor.setPosition(pos);
    }

    const auto &ds = d->m_displaySettings;
    if (animate && ds.m_animateNavigationWithinFile) {
      const auto scrollBar = verticalScrollBar();
      const auto start = scrollBar->value();

      ensureBlockIsUnfolded(block);
      setUpdatesEnabled(false);
      setTextCursor(cursor);
      if (centerLine)
        centerCursor();
      else
        ensureCursorVisible();
      const auto end = scrollBar->value();
      scrollBar->setValue(start);
      setUpdatesEnabled(true);

      const auto delta = end - start;
      // limit the number of steps for the animation otherwise you wont be able to tell
      // the direction of the animantion for large delta values
      const auto steps = qMax(-ds.m_animateWithinFileTimeMax, qMin(ds.m_animateWithinFileTimeMax, delta));
      // limit the duration of the animation to at least 4 pictures on a 60Hz Monitor and
      // at most to the number of absolute steps
      const auto durationMinimum = int(4 // number of pictures
        * float(1) / 60                  // on a 60 Hz Monitor
        * 1000);                         // milliseconds
      const auto duration = qMax(durationMinimum, qAbs(steps));

      d->m_navigationAnimation = new QSequentialAnimationGroup(this);
      const auto startAnimation = new QPropertyAnimation(verticalScrollBar(), "value");
      startAnimation->setEasingCurve(QEasingCurve::InExpo);
      startAnimation->setStartValue(start);
      startAnimation->setEndValue(start + steps / 2);
      startAnimation->setDuration(duration / 2);
      d->m_navigationAnimation->addAnimation(startAnimation);
      const auto endAnimation = new QPropertyAnimation(verticalScrollBar(), "value");
      endAnimation->setEasingCurve(QEasingCurve::OutExpo);
      endAnimation->setStartValue(end - steps / 2);
      endAnimation->setEndValue(end);
      endAnimation->setDuration(duration / 2);
      d->m_navigationAnimation->addAnimation(endAnimation);
      d->m_navigationAnimation->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
      setTextCursor(cursor);
      if (centerLine)
        centerCursor();
      else
        ensureCursorVisible();
    }
  }
  d->saveCurrentCursorPositionForNavigation();
}

auto TextEditorWidget::position(TextPositionOperation posOp, int at) const -> int
{
  auto tc = textCursor();

  if (at != -1)
    tc.setPosition(at);

  if (posOp == CurrentPosition)
    return tc.position();

  switch (posOp) {
  case EndOfLinePosition:
    tc.movePosition(QTextCursor::EndOfLine);
    return tc.position();
  case StartOfLinePosition:
    tc.movePosition(QTextCursor::StartOfLine);
    return tc.position();
  case AnchorPosition:
    if (tc.hasSelection())
      return tc.anchor();
    break;
  case EndOfDocPosition:
    tc.movePosition(QTextCursor::End);
    return tc.position();
  default:
    break;
  }

  return -1;
}

auto TextEditorWidget::cursorRect(int pos) const -> QRect
{
  auto tc = textCursor();
  if (pos >= 0)
    tc.setPosition(pos);
  auto result = cursorRect(tc);
  result.moveTo(viewport()->mapToGlobal(result.topLeft()));
  return result;
}

auto TextEditorWidget::convertPosition(int pos, int *line, int *column) const -> void
{
  Text::convertPosition(document(), pos, line, column);
}

auto TextEditorWidget::event(QEvent *e) -> bool
{
  if (!d)
    return QPlainTextEdit::event(e);

  // FIXME: That's far too heavy, and triggers e.g for ChildEvent
  if (e->type() != QEvent::InputMethodQuery)
    d->m_contentsChanged = false;
  switch (e->type()) {
  case QEvent::ShortcutOverride: {
    const auto ke = static_cast<QKeyEvent*>(e);
    if (ke->key() == Qt::Key_Escape && (d->m_snippetOverlay->isVisible() || multiTextCursor().hasMultipleCursors())) {
      e->accept();
    } else {
      // hack copied from QInputControl::isCommonTextEditShortcut
      // Fixes: QTCREATORBUG-22854
      e->setAccepted((ke->modifiers() == Qt::NoModifier || ke->modifiers() == Qt::ShiftModifier || ke->modifiers() == Qt::KeypadModifier) && ke->key() < Qt::Key_Escape);
      d->m_maybeFakeTooltipEvent = false;
    }
    return true;
  }
  case QEvent::ApplicationPaletteChange: {
    // slight hack: ignore palette changes
    // at this point the palette has changed already,
    // so undo it by re-setting the palette:
    applyFontSettings();
    return true;
  }
  default:
    break;
  }

  return QPlainTextEdit::event(e);
}

auto TextEditorWidget::contextMenuEvent(QContextMenuEvent *e) -> void
{
  showDefaultContextMenu(e, Id());
}

auto TextEditorWidgetPrivate::documentAboutToBeReloaded() -> void
{
  //memorize cursor position
  m_tempState = q->saveState();

  // remove extra selections (loads of QTextCursor objects)

  m_extraSelections.clear();
  m_extraSelections.reserve(NExtraSelectionKinds);
  q->QPlainTextEdit::setExtraSelections(QList<QTextEdit::ExtraSelection>());

  // clear all overlays
  m_overlay->clear();
  m_snippetOverlay->clear();
  m_searchResultOverlay->clear();
  m_refactorOverlay->clear();

  // clear search results
  m_searchResults.clear();
}

auto TextEditorWidgetPrivate::documentReloadFinished(bool success) -> void
{
  if (!success)
    return;

  // restore cursor position
  q->restoreState(m_tempState);
  updateCannotDecodeInfo();
}

auto TextEditorWidget::saveState() const -> QByteArray
{
  QByteArray state;
  QDataStream stream(&state, QIODevice::WriteOnly);
  stream << 2; // version number
  stream << verticalScrollBar()->value();
  stream << horizontalScrollBar()->value();
  int line, column;
  convertPosition(textCursor().position(), &line, &column);
  stream << line;
  stream << column;

  // store code folding state
  QList<int> foldedBlocks;
  auto block = document()->firstBlock();
  while (block.isValid()) {
    if (block.userData() && static_cast<TextBlockUserData*>(block.userData())->folded()) {
      const auto number = block.blockNumber();
      foldedBlocks += number;
    }
    block = block.next();
  }
  stream << foldedBlocks;

  stream << firstVisibleBlockNumber();
  stream << lastVisibleBlockNumber();

  return state;
}

auto TextEditorWidget::restoreState(const QByteArray &state) -> void
{
  if (state.isEmpty()) {
    if (d->m_displaySettings.m_autoFoldFirstComment)
      d->foldLicenseHeader();
    return;
  }
  int version;
  int vval;
  int hval;
  int lineVal;
  int columnVal;
  QDataStream stream(state);
  stream >> version;
  stream >> vval;
  stream >> hval;
  stream >> lineVal;
  stream >> columnVal;

  if (version >= 1) {
    QList<int> collapsedBlocks;
    stream >> collapsedBlocks;
    const auto doc = document();
    auto layoutChanged = false;
    foreach(int blockNumber, collapsedBlocks) {
      auto block = doc->findBlockByNumber(qMax(0, blockNumber));
      if (block.isValid()) {
        TextDocumentLayout::doFoldOrUnfold(block, false);
        layoutChanged = true;
      }
    }
    if (layoutChanged) {
      const auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
      QTC_ASSERT(documentLayout, return);
      documentLayout->requestUpdate();
      documentLayout->emitDocumentSizeChanged();
    }
  } else {
    if (d->m_displaySettings.m_autoFoldFirstComment)
      d->foldLicenseHeader();
  }

  d->m_lastCursorChangeWasInteresting = false; // avoid adding last position to history
  // line is 1-based, column is 0-based
  gotoLine(lineVal, columnVal - 1);
  verticalScrollBar()->setValue(vval);
  horizontalScrollBar()->setValue(hval);

  if (version >= 2) {
    int originalFirstBlock, originalLastBlock;
    stream >> originalFirstBlock;
    stream >> originalLastBlock;
    // If current line was visible in the old state, make sure it is visible in the new state.
    // This can happen if the height of the editor changed in the meantime
    const auto lineBlock = lineVal - 1; // line is 1-based, blocks are 0-based
    const auto originalCursorVisible = originalFirstBlock <= lineBlock && lineBlock <= originalLastBlock;
    const auto firstBlock = firstVisibleBlockNumber();
    const auto lastBlock = lastVisibleBlockNumber();
    const auto cursorVisible = firstBlock <= lineBlock && lineBlock <= lastBlock;
    if (originalCursorVisible && !cursorVisible)
      centerCursor();
  }

  d->saveCurrentCursorPositionForNavigation();
}

auto TextEditorWidget::setParenthesesMatchingEnabled(bool b) -> void
{
  d->m_parenthesesMatchingEnabled = b;
}

auto TextEditorWidget::isParenthesesMatchingEnabled() const -> bool
{
  return d->m_parenthesesMatchingEnabled;
}

auto TextEditorWidget::setHighlightCurrentLine(bool b) -> void
{
  d->m_highlightCurrentLine = b;
  d->updateCurrentLineHighlight();
}

auto TextEditorWidget::highlightCurrentLine() const -> bool
{
  return d->m_highlightCurrentLine;
}

auto TextEditorWidget::setLineNumbersVisible(bool b) -> void
{
  d->m_lineNumbersVisible = b;
  d->slotUpdateExtraAreaWidth();
}

auto TextEditorWidget::lineNumbersVisible() const -> bool
{
  return d->m_lineNumbersVisible;
}

auto TextEditorWidget::setAlwaysOpenLinksInNextSplit(bool b) -> void
{
  d->m_displaySettings.m_openLinksInNextSplit = b;
}

auto TextEditorWidget::alwaysOpenLinksInNextSplit() const -> bool
{
  return d->m_displaySettings.m_openLinksInNextSplit;
}

auto TextEditorWidget::setMarksVisible(bool b) -> void
{
  d->m_marksVisible = b;
  d->slotUpdateExtraAreaWidth();
}

auto TextEditorWidget::marksVisible() const -> bool
{
  return d->m_marksVisible;
}

auto TextEditorWidget::setRequestMarkEnabled(bool b) -> void
{
  d->m_requestMarkEnabled = b;
}

auto TextEditorWidget::requestMarkEnabled() const -> bool
{
  return d->m_requestMarkEnabled;
}

auto TextEditorWidget::setLineSeparatorsAllowed(bool b) -> void
{
  d->m_lineSeparatorsAllowed = b;
}

auto TextEditorWidget::lineSeparatorsAllowed() const -> bool
{
  return d->m_lineSeparatorsAllowed;
}

auto TextEditorWidgetPrivate::updateCodeFoldingVisible() -> void
{
  const auto visible = m_codeFoldingSupported && m_displaySettings.m_displayFoldingMarkers;
  if (m_codeFoldingVisible != visible) {
    m_codeFoldingVisible = visible;
    slotUpdateExtraAreaWidth();
  }
}

auto TextEditorWidgetPrivate::reconfigure() -> void
{
  m_document->setMimeType(Utils::mimeTypeForFile(m_document->filePath()).name());
  q->configureGenericHighlighter();
}

auto TextEditorWidgetPrivate::updateSyntaxInfoBar(const Highlighter::Definitions &definitions, const QString &fileName) -> void
{
  Id missing(Constants::INFO_MISSING_SYNTAX_DEFINITION);
  Id multiple(Constants::INFO_MULTIPLE_SYNTAX_DEFINITIONS);
  InfoBar *infoBar = m_document->infoBar();

  if (definitions.isEmpty() && infoBar->canInfoBeAdded(missing) && !TextEditorSettings::highlighterSettings().isIgnoredFilePattern(fileName)) {
    InfoBarEntry info(missing, BaseTextEditor::tr("A highlight definition was not found for this file. " "Would you like to download additional highlight definition files?"), InfoBarEntry::GlobalSuppression::Enabled);
    info.addCustomButton(BaseTextEditor::tr("Download Definitions"), [missing, this]() {
      m_document->infoBar()->removeInfo(missing);
      Highlighter::downloadDefinitions();
    });

    infoBar->removeInfo(multiple);
    infoBar->addInfo(info);
  } else if (definitions.size() > 1) {
    InfoBarEntry info(multiple, BaseTextEditor::tr("More than one highlight definition was found for this file. " "Which one should be used to highlight this file?"));
    info.setComboInfo(transform(definitions, &Highlighter::Definition::name), [this](const QString &definition) {
      this->configureGenericHighlighter(Highlighter::definitionForName(definition));
    });

    info.addCustomButton(BaseTextEditor::tr("Remember My Choice"), [multiple, this]() {
      m_document->infoBar()->removeInfo(multiple);
      rememberCurrentSyntaxDefinition();
    });

    infoBar->removeInfo(missing);
    infoBar->addInfo(info);
  } else {
    infoBar->removeInfo(multiple);
    infoBar->removeInfo(missing);
  }
}

auto TextEditorWidgetPrivate::configureGenericHighlighter(const KSyntaxHighlighting::Definition &definition) -> void
{
  const auto highlighter = new Highlighter();
  m_document->setSyntaxHighlighter(highlighter);

  if (definition.isValid()) {
    highlighter->setDefinition(definition);
    m_commentDefinition.singleLine = definition.singleLineCommentMarker();
    m_commentDefinition.multiLineStart = definition.multiLineCommentMarker().first;
    m_commentDefinition.multiLineEnd = definition.multiLineCommentMarker().second;
    q->setCodeFoldingSupported(true);
  } else {
    q->setCodeFoldingSupported(false);
  }

  m_document->setFontSettings(TextEditorSettings::fontSettings());
}

auto TextEditorWidgetPrivate::rememberCurrentSyntaxDefinition() -> void
{
  const auto highlighter = qobject_cast<Highlighter*>(m_document->syntaxHighlighter());
  if (!highlighter)
    return;
  const auto &definition = highlighter->definition();
  if (definition.isValid())
    Highlighter::rememberDefinitionForDocument(definition, m_document.data());
}

auto TextEditorWidgetPrivate::openLinkUnderCursor(bool openInNextSplit) -> void
{
  q->findLinkAt(q->textCursor(), [openInNextSplit, self = QPointer(q)](const Link &symbolLink) {
    if (self)
      self->openLink(symbolLink, openInNextSplit);
  }, true, openInNextSplit);
}

auto TextEditorWidget::codeFoldingVisible() const -> bool
{
  return d->m_codeFoldingVisible;
}

/**
 * Sets whether code folding is supported by the syntax highlighter. When not
 * supported (the default), this makes sure the code folding is not shown.
 *
 * Needs to be called before calling setCodeFoldingVisible.
 */
auto TextEditorWidget::setCodeFoldingSupported(bool b) -> void
{
  d->m_codeFoldingSupported = b;
  d->updateCodeFoldingVisible();
}

auto TextEditorWidget::codeFoldingSupported() const -> bool
{
  return d->m_codeFoldingSupported;
}

auto TextEditorWidget::setMouseNavigationEnabled(bool b) -> void
{
  d->m_behaviorSettings.m_mouseNavigation = b;
}

auto TextEditorWidget::mouseNavigationEnabled() const -> bool
{
  return d->m_behaviorSettings.m_mouseNavigation;
}

auto TextEditorWidget::setMouseHidingEnabled(bool b) -> void
{
  d->m_behaviorSettings.m_mouseHiding = b;
}

auto TextEditorWidget::mouseHidingEnabled() const -> bool
{
  return d->m_behaviorSettings.m_mouseHiding;
}

auto TextEditorWidget::setScrollWheelZoomingEnabled(bool b) -> void
{
  d->m_behaviorSettings.m_scrollWheelZooming = b;
}

auto TextEditorWidget::scrollWheelZoomingEnabled() const -> bool
{
  return d->m_behaviorSettings.m_scrollWheelZooming;
}

auto TextEditorWidget::setConstrainTooltips(bool b) -> void
{
  d->m_behaviorSettings.m_constrainHoverTooltips = b;
}

auto TextEditorWidget::constrainTooltips() const -> bool
{
  return d->m_behaviorSettings.m_constrainHoverTooltips;
}

auto TextEditorWidget::setCamelCaseNavigationEnabled(bool b) -> void
{
  d->m_behaviorSettings.m_camelCaseNavigation = b;
}

auto TextEditorWidget::camelCaseNavigationEnabled() const -> bool
{
  return d->m_behaviorSettings.m_camelCaseNavigation;
}

auto TextEditorWidget::setRevisionsVisible(bool b) -> void
{
  d->m_revisionsVisible = b;
  d->slotUpdateExtraAreaWidth();
}

auto TextEditorWidget::revisionsVisible() const -> bool
{
  return d->m_revisionsVisible;
}

auto TextEditorWidget::setVisibleWrapColumn(int column) -> void
{
  d->m_visibleWrapColumn = column;
  viewport()->update();
}

auto TextEditorWidget::visibleWrapColumn() const -> int
{
  return d->m_visibleWrapColumn;
}

auto TextEditorWidget::setAutoCompleter(AutoCompleter *autoCompleter) -> void
{
  d->m_autoCompleter.reset(autoCompleter);
}

auto TextEditorWidget::autoCompleter() const -> AutoCompleter*
{
  return d->m_autoCompleter.data();
}

//
// TextEditorWidgetPrivate
//

auto TextEditorWidgetPrivate::setupDocumentSignals() -> void
{
  const auto doc = m_document->document();
  q->setDocument(doc);
  q->setCursorWidth(2); // Applies to the document layout

  const auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
  QTC_CHECK(documentLayout);

  connect(documentLayout, &QPlainTextDocumentLayout::updateBlock, this, &TextEditorWidgetPrivate::slotUpdateBlockNotify);

  connect(documentLayout, &TextDocumentLayout::updateExtraArea, m_extraArea, QOverload<>::of(&QWidget::update));

  connect(q, &TextEditorWidget::requestBlockUpdate, documentLayout, &QPlainTextDocumentLayout::updateBlock);

  connect(documentLayout, &TextDocumentLayout::updateExtraArea, this, &TextEditorWidgetPrivate::scheduleUpdateHighlightScrollBar);

  connect(documentLayout, &TextDocumentLayout::parenthesesChanged, &m_parenthesesMatchingTimer, QOverload<>::of(&QTimer::start));

  connect(documentLayout, &QAbstractTextDocumentLayout::documentSizeChanged, this, &TextEditorWidgetPrivate::scheduleUpdateHighlightScrollBar);

  connect(documentLayout, &QAbstractTextDocumentLayout::update, this, &TextEditorWidgetPrivate::scheduleUpdateHighlightScrollBar);

  connect(doc, &QTextDocument::contentsChange, this, &TextEditorWidgetPrivate::editorContentsChange);

  QObject::connect(m_document.data(), &TextDocument::aboutToReload, this, &TextEditorWidgetPrivate::documentAboutToBeReloaded);

  QObject::connect(m_document.data(), &TextDocument::reloadFinished, this, &TextEditorWidgetPrivate::documentReloadFinished);

  connect(m_document.data(), &TextDocument::tabSettingsChanged, this, [this]() {
    updateTabStops();
    m_autoCompleter->setTabSettings(m_document->tabSettings());
  });

  connect(m_document.data(), &TextDocument::fontSettingsChanged, this, &TextEditorWidgetPrivate::applyFontSettingsDelayed);

  connect(m_document.data(), &TextDocument::markRemoved, this, &TextEditorWidgetPrivate::markRemoved);

  slotUpdateExtraAreaWidth();

  const auto settings = TextEditorSettings::instance();

  // Connect to settings change signals
  connect(settings, &TextEditorSettings::fontSettingsChanged, m_document.data(), &TextDocument::setFontSettings);
  connect(settings, &TextEditorSettings::typingSettingsChanged, q, &TextEditorWidget::setTypingSettings);
  connect(settings, &TextEditorSettings::storageSettingsChanged, q, &TextEditorWidget::setStorageSettings);
  connect(settings, &TextEditorSettings::behaviorSettingsChanged, q, &TextEditorWidget::setBehaviorSettings);
  connect(settings, &TextEditorSettings::marginSettingsChanged, q, &TextEditorWidget::setMarginSettings);
  connect(settings, &TextEditorSettings::displaySettingsChanged, q, &TextEditorWidget::setDisplaySettings);
  connect(settings, &TextEditorSettings::completionSettingsChanged, q, &TextEditorWidget::setCompletionSettings);
  connect(settings, &TextEditorSettings::extraEncodingSettingsChanged, q, &TextEditorWidget::setExtraEncodingSettings);

  // Apply current settings
  m_document->setFontSettings(TextEditorSettings::fontSettings());
  m_document->setTabSettings(TextEditorSettings::codeStyle()->tabSettings()); // also set through code style ???
  q->setTypingSettings(TextEditorSettings::typingSettings());
  q->setStorageSettings(TextEditorSettings::storageSettings());
  q->setBehaviorSettings(TextEditorSettings::behaviorSettings());
  q->setMarginSettings(TextEditorSettings::marginSettings());
  q->setDisplaySettings(TextEditorSettings::displaySettings());
  q->setCompletionSettings(TextEditorSettings::completionSettings());
  q->setExtraEncodingSettings(TextEditorSettings::extraEncodingSettings());
  q->setCodeStyle(TextEditorSettings::codeStyle(m_tabSettingsId));
}

auto TextEditorWidgetPrivate::snippetCheckCursor(const QTextCursor &cursor) -> bool
{
  if (!m_snippetOverlay->isVisible() || m_snippetOverlay->isEmpty())
    return false;

  auto start = cursor;
  start.setPosition(cursor.selectionStart());
  auto end = cursor;
  end.setPosition(cursor.selectionEnd());
  if (!m_snippetOverlay->hasCursorInSelection(start) || !m_snippetOverlay->hasCursorInSelection(end) || m_snippetOverlay->hasFirstSelectionBeginMoved()) {
    m_snippetOverlay->accept();
    return false;
  }
  return true;
}

auto TextEditorWidgetPrivate::snippetTabOrBacktab(bool forward) -> void
{
  if (!m_snippetOverlay->isVisible() || m_snippetOverlay->isEmpty())
    return;
  const auto cursor = forward ? m_snippetOverlay->nextSelectionCursor(q->textCursor()) : m_snippetOverlay->previousSelectionCursor(q->textCursor());
  q->setTextCursor(cursor);
  if (m_snippetOverlay->isFinalSelection(cursor))
    m_snippetOverlay->accept();
}

// Calculate global position for a tooltip considering the left extra area.
auto TextEditorWidget::toolTipPosition(const QTextCursor &c) const -> QPoint
{
  const auto cursorPos = mapToGlobal(cursorRect(c).bottomRight() + QPoint(1, 1));
  return cursorPos + QPoint(d->m_extraArea->width(), HostOsInfo::isWindowsHost() ? -24 : -16);
}

auto TextEditorWidget::showTextMarksToolTip(const QPoint &pos, const TextMarks &marks, const TextMark *mainTextMark) const -> void
{
  d->showTextMarksToolTip(pos, marks, mainTextMark);
}

auto TextEditorWidgetPrivate::processTooltipRequest(const QTextCursor &c) -> void
{
  const auto toolTipPoint = q->toolTipPosition(c);
  auto handled = false;
  emit q->tooltipOverrideRequested(q, toolTipPoint, c.position(), &handled);
  if (handled)
    return;

  if (m_hoverHandlers.isEmpty()) {
    emit q->tooltipRequested(toolTipPoint, c.position());
    return;
  }

  const auto callback = [toolTipPoint](TextEditorWidget *widget, BaseHoverHandler *handler, int) {
    handler->showToolTip(widget, toolTipPoint);
  };
  m_hoverHandlerRunner.startChecking(c, callback);
}

auto TextEditorWidgetPrivate::processAnnotaionTooltipRequest(const QTextBlock &block, const QPoint &pos) const -> bool
{
  const auto blockUserData = TextDocumentLayout::textUserData(block);
  if (!blockUserData)
    return false;

  for (const auto &annotationRect : m_annotationRects[block.blockNumber()]) {
    if (!annotationRect.rect.contains(pos))
      continue;
    showTextMarksToolTip(q->mapToGlobal(pos), blockUserData->marks(), annotationRect.mark);
    return true;
  }
  return false;
}

auto TextEditorWidget::viewportEvent(QEvent *event) -> bool
{
  d->m_contentsChanged = false;
  if (event->type() == QEvent::ToolTip) {
    if (QApplication::keyboardModifiers() & Qt::ControlModifier || !(QApplication::keyboardModifiers() & Qt::ShiftModifier) && d->m_behaviorSettings.m_constrainHoverTooltips) {
      // Tooltips should be eaten when either control is pressed (so they don't get in the
      // way of code navigation) or if they are in constrained mode and shift is not pressed.
      return true;
    }
    const QHelpEvent *he = static_cast<QHelpEvent*>(event);
    const auto &pos = he->pos();

    const auto refactorMarker = d->m_refactorOverlay->markerAt(pos);
    if (refactorMarker.isValid() && !refactorMarker.tooltip.isEmpty()) {
      ToolTip::show(he->globalPos(), refactorMarker.tooltip, viewport(), {}, refactorMarker.rect);
      return true;
    }

    const auto tc = cursorForPosition(pos);
    const auto block = tc.block();
    const auto line = block.layout()->lineForTextPosition(tc.positionInBlock());
    QTC_CHECK(line.isValid());
    // Only handle tool tip for text cursor if mouse is within the block for the text cursor,
    // and not if the mouse is e.g. in the empty space behind a short line.
    if (line.isValid()) {
      if (pos.x() <= blockBoundingGeometry(block).left() + line.naturalTextRect().right()) {
        d->processTooltipRequest(tc);
        return true;
      }
      if (d->processAnnotaionTooltipRequest(block, pos)) {
        return true;
      }
      ToolTip::hide();
    }
  }
  return QPlainTextEdit::viewportEvent(event);
}

auto TextEditorWidget::resizeEvent(QResizeEvent *e) -> void
{
  QPlainTextEdit::resizeEvent(e);
  const auto cr = rect();
  d->m_extraArea->setGeometry(QStyle::visualRect(layoutDirection(), cr, QRect(cr.left() + frameWidth(), cr.top() + frameWidth(), extraAreaWidth(), cr.height() - 2 * frameWidth())));
  d->adjustScrollBarRanges();
  d->updateCurrentLineInScrollbar();
}

auto TextEditorWidgetPrivate::foldBox() -> QRect
{
  if (m_highlightBlocksInfo.isEmpty() || extraAreaHighlightFoldedBlockNumber < 0)
    return {};

  const auto begin = q->document()->findBlockByNumber(m_highlightBlocksInfo.open.last());

  const auto end = q->document()->findBlockByNumber(m_highlightBlocksInfo.close.first());
  if (!begin.isValid() || !end.isValid())
    return {};
  const auto br = q->blockBoundingGeometry(begin).translated(q->contentOffset());
  const auto er = q->blockBoundingGeometry(end).translated(q->contentOffset());

  return QRect(m_extraArea->width() - foldBoxWidth(q->fontMetrics()), int(br.top()), foldBoxWidth(q->fontMetrics()), int(er.bottom() - br.top()));
}

auto TextEditorWidgetPrivate::foldedBlockAt(const QPoint &pos, QRect *box) const -> QTextBlock
{
  const auto offset = q->contentOffset();
  auto block = q->firstVisibleBlock();
  auto top = q->blockBoundingGeometry(block).translated(offset).top();
  auto bottom = top + q->blockBoundingRect(block).height();

  const auto viewportHeight = q->viewport()->height();

  while (block.isValid() && top <= viewportHeight) {
    auto nextBlock = block.next();
    if (block.isVisible() && bottom >= 0 && q->replacementVisible(block.blockNumber())) {
      if (nextBlock.isValid() && !nextBlock.isVisible()) {
        const auto layout = block.layout();
        auto line = layout->lineAt(layout->lineCount() - 1);
        auto lineRect = line.naturalTextRect().translated(offset.x(), top);
        lineRect.adjust(0, 0, -1, -1);

        QString replacement = QLatin1String(" {") + q->foldReplacementText(block) + QLatin1String("}; ");

        QRectF collapseRect(lineRect.right() + 12, lineRect.top(), q->fontMetrics().horizontalAdvance(replacement), lineRect.height());
        if (collapseRect.contains(pos)) {
          auto result = block;
          if (box)
            *box = collapseRect.toAlignedRect();
          return result;
        }
        block = nextBlock;
        while (nextBlock.isValid() && !nextBlock.isVisible()) {
          block = nextBlock;
          nextBlock = block.next();
        }
      }
    }

    block = nextBlock;
    top = bottom;
    bottom = top + q->blockBoundingRect(block).height();
  }
  return QTextBlock();
}

auto TextEditorWidgetPrivate::highlightSearchResults(const QTextBlock &block, const PaintEventData &data) const -> void
{
  if (m_searchExpr.pattern().isEmpty())
    return;

  const auto blockPosition = block.position();

  const auto cursor = q->textCursor();
  auto text = block.text();
  text.replace(QChar::Nbsp, QLatin1Char(' '));
  auto idx = -1;
  auto l = 0;

  const auto left = data.viewportRect.left() - int(data.offset.x());
  const auto right = data.viewportRect.right() - int(data.offset.x());
  const auto top = data.viewportRect.top() - int(data.offset.y());
  const auto bottom = data.viewportRect.bottom() - int(data.offset.y());
  const auto &searchResultColor = m_document->fontSettings().toTextCharFormat(C_SEARCH_RESULT).background().color().darker(120);

  while (idx < text.length()) {
    const auto match = m_searchExpr.match(text, idx + l + 1);
    if (!match.hasMatch())
      break;
    idx = match.capturedStart();
    l = match.capturedLength();
    if (l == 0)
      break;
    if (m_findFlags & FindWholeWords
            && (idx && text.at(idx - 1).isLetterOrNumber() || idx + l < text.length() && text.at(idx + l).isLetterOrNumber()))
      continue;

    const auto start = blockPosition + idx;
    const auto end = start + l;
    auto result = cursor;
    result.setPosition(start);
    result.setPosition(end, QTextCursor::KeepAnchor);
    if (!q->inFindScope(result))
      continue;

    // check if the result is inside the visibale area for long blocks
    const auto &startLine = block.layout()->lineForTextPosition(idx);
    const auto &endLine = block.layout()->lineForTextPosition(idx + l);

    if (startLine.isValid() && endLine.isValid() && startLine.lineNumber() == endLine.lineNumber()) {
      const auto lineY = int(endLine.y() + q->blockBoundingGeometry(block).y());
      if (startLine.cursorToX(idx) > right) {
        // result is behind the visible area
        if (endLine.lineNumber() >= block.lineCount() - 1)
          break; // this is the last line in the block, nothing more to add

        // skip to the start of the next line
        idx = block.layout()->lineAt(endLine.lineNumber() + 1).textStart();
        l = 0;
        continue;
      }
      if (endLine.cursorToX(idx + l, QTextLine::Trailing) < left) {
        // result is in front of the visible area skip it
        continue;
      }
      if (lineY + endLine.height() < top) {
        if (endLine.lineNumber() >= block.lineCount() - 1)
          break; // this is the last line in the block, nothing more to add
        // before visible area, skip to the start of the next line
        idx = block.layout()->lineAt(endLine.lineNumber() + 1).textStart();
        l = 0;
        continue;
      }
      if (lineY > bottom) {
        break; // under the visible area, nothing more to add
      }
    }

    const uint flag = idx == cursor.selectionStart() - blockPosition && idx + l == cursor.selectionEnd() - blockPosition ? TextEditorOverlay::DropShadow : 0;
    m_searchResultOverlay->addOverlaySelection(start, end, searchResultColor, QColor(), flag);
  }
}

auto TextEditorWidgetPrivate::startCursorFlashTimer() -> void
{
  const auto flashTime = QApplication::cursorFlashTime();
  if (flashTime > 0) {
    m_cursorFlashTimer.stop();
    m_cursorFlashTimer.start(flashTime / 2, q);
  }
  if (!m_cursorVisible) {
    m_cursorVisible = true;
    q->viewport()->update(cursorUpdateRect(m_cursors));
  }
}

auto TextEditorWidgetPrivate::resetCursorFlashTimer() -> void
{
  if (!m_cursorFlashTimer.isActive())
    return;
  const auto flashTime = QApplication::cursorFlashTime();
  if (flashTime > 0) {
    m_cursorFlashTimer.stop();
    m_cursorFlashTimer.start(flashTime / 2, q);
  }
  if (!m_cursorVisible) {
    m_cursorVisible = true;
    q->viewport()->update(cursorUpdateRect(m_cursors));
  }
}

auto TextEditorWidgetPrivate::updateCursorSelections() -> void
{
  const auto selectionFormat = TextEditorSettings::fontSettings().toTextCharFormat(C_SELECTION);
  QList<QTextEdit::ExtraSelection> selections;
  for (const auto &cursor : m_cursors) {
    if (cursor.hasSelection())
      selections << QTextEdit::ExtraSelection{cursor, selectionFormat};
  }
  q->setExtraSelections(TextEditorWidget::CursorSelection, selections);
}

auto TextEditorWidgetPrivate::moveCursor(QTextCursor::MoveOperation operation, QTextCursor::MoveMode mode) -> void
{
  auto cursor = m_cursors;
  cursor.movePosition(operation, mode);
  q->setMultiTextCursor(cursor);
}

auto TextEditorWidgetPrivate::cursorUpdateRect(const MultiTextCursor &cursor) -> QRect
{
  QRect result(0, 0, 0, 0);
  for (const auto &c : cursor)
    result |= q->cursorRect(c);
  return result;
}

auto TextEditorWidgetPrivate::moveCursorVisible(bool ensureVisible) -> void
{
  auto cursor = q->textCursor();
  if (!cursor.block().isVisible()) {
    cursor.setVisualNavigation(true);
    cursor.movePosition(QTextCursor::Up);
    q->setTextCursor(cursor);
  }
  if (ensureVisible)
    q->ensureCursorVisible();
}

static auto blendColors(const QColor &a, const QColor &b, int alpha) -> QColor
{
  return QColor((a.red() * (256 - alpha) + b.red() * alpha) / 256, (a.green() * (256 - alpha) + b.green() * alpha) / 256, (a.blue() * (256 - alpha) + b.blue() * alpha) / 256);
}

static auto calcBlendColor(const QColor &baseColor, int level, int count) -> QColor
{
  QColor color80;
  QColor color90;

  if (baseColor.value() > 128) {
    const auto f90 = 15;
    const auto f80 = 30;
    color80.setRgb(qMax(0, baseColor.red() - f80), qMax(0, baseColor.green() - f80), qMax(0, baseColor.blue() - f80));
    color90.setRgb(qMax(0, baseColor.red() - f90), qMax(0, baseColor.green() - f90), qMax(0, baseColor.blue() - f90));
  } else {
    const auto f90 = 20;
    const auto f80 = 40;
    color80.setRgb(qMin(255, baseColor.red() + f80), qMin(255, baseColor.green() + f80), qMin(255, baseColor.blue() + f80));
    color90.setRgb(qMin(255, baseColor.red() + f90), qMin(255, baseColor.green() + f90), qMin(255, baseColor.blue() + f90));
  }

  if (level == count)
    return baseColor;
  if (level == 0)
    return color80;
  if (level == count - 1)
    return color90;

  const auto blendFactor = level * (256 / (count - 2));

  return blendColors(color80, color90, blendFactor);
}

static auto createBlockCursorCharFormatRange(int pos, const QColor &textColor, const QColor &baseColor) -> QTextLayout::FormatRange
{
  QTextLayout::FormatRange o;
  o.start = pos;
  o.length = 1;
  o.format.setForeground(baseColor);
  o.format.setBackground(textColor);
  return o;
}

static auto availableMarks(const TextMarks &marks, QRectF &boundingRect, const QFontMetrics &fm, const qreal itemOffset) -> TextMarks
{
  TextMarks ret;
  auto first = true;
  for (const auto mark : marks) {
    const auto &rects = mark->annotationRects(boundingRect, fm, first ? 0 : itemOffset, 0);
    if (rects.annotationRect.isEmpty())
      break;
    boundingRect.setLeft(rects.fadeOutRect.right());
    ret.append(mark);
    if (boundingRect.isEmpty())
      break;
    first = false;
  }
  return ret;
}

auto TextEditorWidgetPrivate::getLastLineLineRect(const QTextBlock &block) -> QRectF
{
  const QTextLayout *layout = block.layout();
  const auto lineCount = layout->lineCount();
  if (lineCount < 1)
    return {};
  const auto line = layout->lineAt(lineCount - 1);
  const auto contentOffset = q->contentOffset();
  const auto top = q->blockBoundingGeometry(block).translated(contentOffset).top();
  return line.naturalTextRect().translated(contentOffset.x(), top).adjusted(0, 0, -1, -1);
}

auto TextEditorWidgetPrivate::updateAnnotationBounds(TextBlockUserData *blockUserData, TextDocumentLayout *layout, bool annotationsVisible) -> bool
{
  const auto additionalHeightNeeded = annotationsVisible && m_displaySettings.m_annotationAlignment == AnnotationAlignment::BetweenLines;
  const auto additionalHeight = additionalHeightNeeded ? q->fontMetrics().lineSpacing() : 0;
  if (blockUserData->additionalAnnotationHeight() == additionalHeight)
    return false;
  blockUserData->setAdditionalAnnotationHeight(additionalHeight);
  q->viewport()->update();
  layout->emitDocumentSizeChanged();
  return true;
}

auto TextEditorWidgetPrivate::updateLineAnnotation(const PaintEventData &data, const PaintEventBlockData &blockData, QPainter &painter) -> void
{
  m_annotationRects.remove(data.block.blockNumber());

  if (!m_displaySettings.m_displayAnnotations)
    return;

  const auto blockUserData = TextDocumentLayout::textUserData(data.block);
  if (!blockUserData)
    return;

  auto marks = filtered(blockUserData->marks(), [](const TextMark *mark) {
    return !mark->lineAnnotation().isEmpty();
  });

  const auto annotationsVisible = !marks.isEmpty();

  if (updateAnnotationBounds(blockUserData, data.documentLayout, annotationsVisible) || !annotationsVisible) {
    return;
  }

  const auto lineRect = getLastLineLineRect(data.block);
  if (lineRect.isNull())
    return;

  sort(marks, [](const TextMark *mark1, const TextMark *mark2) {
    return mark1->priority() > mark2->priority();
  });

  const qreal itemOffset = q->fontMetrics().lineSpacing();
  const auto initialOffset = m_displaySettings.m_annotationAlignment == AnnotationAlignment::BetweenLines ? itemOffset / 2 : itemOffset * 2;
  const qreal minimalContentWidth = q->fontMetrics().horizontalAdvance('X') * m_displaySettings.m_minimalAnnotationContent;
  auto offset = initialOffset;
  qreal x = 0;
  if (marks.isEmpty())
    return;
  QRectF boundingRect;
  if (m_displaySettings.m_annotationAlignment == AnnotationAlignment::BetweenLines) {
    boundingRect = QRectF(lineRect.bottomLeft(), blockData.boundingRect.bottomRight());
  } else {
    boundingRect = QRectF(lineRect.topLeft().x(), lineRect.topLeft().y(), q->viewport()->width() - lineRect.right(), lineRect.height());
    x = lineRect.right();
    if (m_displaySettings.m_annotationAlignment == AnnotationAlignment::NextToMargin && data.rightMargin > lineRect.right() + offset && q->viewport()->width() > data.rightMargin + minimalContentWidth) {
      offset = data.rightMargin - lineRect.right();
    } else if (m_displaySettings.m_annotationAlignment != AnnotationAlignment::NextToContent) {
      marks = availableMarks(marks, boundingRect, q->fontMetrics(), itemOffset);
      if (boundingRect.width() > 0)
        offset = qMax(boundingRect.width(), initialOffset);
    }
  }

  for (const TextMark *mark : qAsConst(marks)) {
    if (!mark->isVisible())
      continue;
    boundingRect = QRectF(x, boundingRect.top(), q->viewport()->width() - x, boundingRect.height());
    if (boundingRect.isEmpty())
      break;
    if (data.eventRect.intersects(boundingRect.toRect()))
      mark->paintAnnotation(painter, &boundingRect, offset, itemOffset / 2, q->contentOffset());

    x = boundingRect.right();
    offset = itemOffset / 2;
    m_annotationRects[data.block.blockNumber()].append({boundingRect, mark});
  }

  QRect updateRect(lineRect.toRect().topRight(), boundingRect.toRect().bottomRight());
  updateRect.setLeft(qBound(0, updateRect.left(), q->viewport()->width() - 1));
  updateRect.setRight(qBound(0, updateRect.right(), q->viewport()->width() - 1));
  if (!updateRect.isEmpty() && !data.eventRect.contains(q->viewport()->rect() & updateRect))
    q->viewport()->update(updateRect);
}

auto blendRightMarginColor(const FontSettings &settings, bool areaColor) -> QColor
{
  const auto baseColor = settings.toTextCharFormat(C_TEXT).background().color();
  const QColor col = baseColor.value() > 128 ? Qt::black : Qt::white;
  return blendColors(baseColor, col, areaColor ? 16 : 32);
}

auto TextEditorWidgetPrivate::paintRightMarginArea(PaintEventData &data, QPainter &painter) const -> void
{
  if (m_visibleWrapColumn <= 0)
    return;
  // Don't use QFontMetricsF::averageCharWidth here, due to it returning
  // a fractional size even when this is not supported by the platform.
  data.rightMargin = QFontMetricsF(q->font()).horizontalAdvance(QLatin1Char('x')) * m_visibleWrapColumn + data.offset.x() + 4;
  if (data.rightMargin<data.viewportRect.width()) {
const QRectF behindMargin(data.rightMargin, data.eventRect.top(), data.viewportRect.width() - data.rightMargin, data.eventRect.height());
                         painter.fillRect(behindMargin, blendRightMarginColor(m_document->fontSettings(), true));
    
                       }
}

auto TextEditorWidgetPrivate::paintRightMarginLine(const PaintEventData &data, QPainter &painter) const -> void
{
  if (m_visibleWrapColumn <= 0 || data.rightMargin >= data.viewportRect.width())
    return;

  const auto pen = painter.pen();
  painter.setPen(blendRightMarginColor(m_document->fontSettings(), false));
  painter.drawLine(QPointF(data.rightMargin, data.eventRect.top()), QPointF(data.rightMargin, data.eventRect.bottom()));
  painter.setPen(pen);
}

static auto nextVisibleBlock(const QTextBlock &block, const QTextDocument *doc) -> QTextBlock
{
  auto nextVisibleBlock = block.next();
  if (!nextVisibleBlock.isVisible()) {
    // invisible blocks do have zero line count
    nextVisibleBlock = doc->findBlockByLineNumber(nextVisibleBlock.firstLineNumber());
    // paranoia in case our code somewhere did not set the line count
    // of the invisible block to 0
    while (nextVisibleBlock.isValid() && !nextVisibleBlock.isVisible())
      nextVisibleBlock = nextVisibleBlock.next();
  }
  return nextVisibleBlock;
}

auto TextEditorWidgetPrivate::paintBlockHighlight(const PaintEventData &data, QPainter &painter) const -> void
{
  if (m_highlightBlocksInfo.isEmpty())
    return;

  const auto baseColor = m_document->fontSettings().toTextCharFormat(C_TEXT).background().color();

  // extra pass for the block highlight

  const auto margin = 5;
  auto block = data.block;
  auto offset = data.offset;
  while (block.isValid()) {
    auto blockBoundingRect = q->blockBoundingRect(block).translated(offset);

    const auto n = block.blockNumber();
    auto depth = 0;
    foreach(int i, m_highlightBlocksInfo.open) if (n >= i)
      ++depth;
    foreach(int i, m_highlightBlocksInfo.close) if (n > i)
      --depth;

    const auto count = m_highlightBlocksInfo.count();
    if (count) {
      for (auto i = 0; i <= depth; ++i) {
        const auto &blendedColor = calcBlendColor(baseColor, i, count);
        const auto vi = i > 0 ? m_highlightBlocksInfo.visualIndent.at(i - 1) : 0;
        auto oneRect = blockBoundingRect;
        oneRect.setWidth(qMax(data.viewportRect.width(), data.documentWidth));
        oneRect.adjust(vi, 0, 0, 0);
        if (oneRect.left() >= oneRect.right())
          continue;
        if (data.rightMargin > 0 && oneRect.left() < data.rightMargin && oneRect.right() > data.rightMargin) {
          auto otherRect = blockBoundingRect;
          otherRect.setLeft(data.rightMargin + 1);
          otherRect.setRight(oneRect.right());
          oneRect.setRight(data.rightMargin - 1);
          painter.fillRect(otherRect, blendedColor);
        }
        painter.fillRect(oneRect, blendedColor);
      }
    }
    offset.ry() += blockBoundingRect.height();

    if (offset.y() > data.viewportRect.height() + margin)
      break;

    block = TextEditor::nextVisibleBlock(block, data.doc);
  }
}

auto TextEditorWidgetPrivate::paintSearchResultOverlay(const PaintEventData &data, QPainter &painter) const -> void
{
  m_searchResultOverlay->clear();
  if (m_searchExpr.pattern().isEmpty() || !m_searchExpr.isValid())
    return;

  const auto margin = 5;
  auto block = data.block;
  auto offset = data.offset;
  while (block.isValid()) {
    auto blockBoundingRect = q->blockBoundingRect(block).translated(offset);

    if (blockBoundingRect.bottom() >= data.eventRect.top() - margin && blockBoundingRect.top() <= data.eventRect.bottom() + margin) {
      highlightSearchResults(block, data);
    }
    offset.ry() += blockBoundingRect.height();

    if (offset.y() > data.viewportRect.height() + margin)
      break;

    block = TextEditor::nextVisibleBlock(block, data.doc);
  }

  m_searchResultOverlay->fill(&painter, data.searchResultFormat.background().color(), data.eventRect);
}

auto TextEditorWidgetPrivate::paintIfDefedOutBlocks(const PaintEventData &data, QPainter &painter) const -> void
{
  auto block = data.block;
  auto offset = data.offset;
  while (block.isValid()) {

    auto r = q->blockBoundingRect(block).translated(offset);

    if (r.bottom() >= data.eventRect.top() && r.top() <= data.eventRect.bottom()) {
      if (TextDocumentLayout::ifdefedOut(block)) {
        auto rr = r;
        rr.setRight(data.viewportRect.width() - offset.x());
        if (data.rightMargin > 0)
          rr.setRight(qMin(data.rightMargin, rr.right()));
        painter.fillRect(rr, data.ifdefedOutFormat.background());
      }
    }
    offset.ry() += r.height();

    if (offset.y() > data.viewportRect.height())
      break;

    block = TextEditor::nextVisibleBlock(block, data.doc);
  }
}

auto TextEditorWidgetPrivate::paintFindScope(const PaintEventData &data, QPainter &painter) const -> void
{
  if (m_findScope.isNull())
    return;
  const auto overlay = new TextEditorOverlay(q);
  for (const auto &c : m_findScope) {
    overlay->addOverlaySelection(c.selectionStart(), c.selectionEnd(), data.searchScopeFormat.foreground().color(), data.searchScopeFormat.background().color(), TextEditorOverlay::ExpandBegin);
  }
  overlay->setAlpha(false);
  overlay->paint(&painter, data.eventRect);
  delete overlay;
}

auto TextEditorWidgetPrivate::paintCurrentLineHighlight(const PaintEventData &data, QPainter &painter) const -> void
{
  if (!m_highlightCurrentLine)
    return;

  QList<QTextCursor> cursorsForBlock;
  for (const auto &c : m_cursors) {
    if (c.block() == data.block)
      cursorsForBlock << c;
  }
  if (cursorsForBlock.isEmpty())
    return;

  const auto blockRect = q->blockBoundingRect(data.block).translated(data.offset);
  auto color = m_document->fontSettings().toTextCharFormat(C_CURRENT_LINE).background().color();
  color.setAlpha(128);
  QSet<int> seenLines;
  for (const auto &cursor : cursorsForBlock) {
    auto line = data.block.layout()->lineForTextPosition(cursor.positionInBlock());
    if (seenLines.contains(line.lineNumber()))
      continue;
    seenLines << line.lineNumber();
    auto lineRect = line.rect();
    lineRect.moveTop(lineRect.top() + blockRect.top());
    lineRect.setLeft(0);
    lineRect.setRight(data.viewportRect.width());
    // set alpha, otherwise we cannot see block highlighting and find scope underneath
    if (!data.eventRect.contains(lineRect.toAlignedRect()))
      q->viewport()->update(lineRect.toAlignedRect());
    painter.fillRect(lineRect, color);
  }
}

auto TextEditorWidgetPrivate::paintCursorAsBlock(const PaintEventData &data, QPainter &painter, PaintEventBlockData &blockData, int cursorPosition) const -> void
{
  const QFontMetricsF fontMetrics(blockData.layout->font());
  const auto relativePos = cursorPosition - blockData.position;
  auto doSelection = true;
  const auto line = blockData.layout->lineForTextPosition(relativePos);
  auto x = line.cursorToX(relativePos);
  qreal w = 0;
  if (relativePos < line.textLength() - line.textStart()) {
    w = line.cursorToX(relativePos + 1) - x;
    if (data.doc->characterAt(cursorPosition) == QLatin1Char('\t')) {
      doSelection = false;
      const auto space = fontMetrics.horizontalAdvance(QLatin1Char(' '));
      if (w > space) {
        x += w - space;
        w = space;
      }
    }
  } else
    w = fontMetrics.horizontalAdvance(QLatin1Char(' ')); // in sync with QTextLine::draw()

  auto lineRect = line.rect();
  lineRect.moveTop(lineRect.top() + blockData.boundingRect.top());
  lineRect.moveLeft(blockData.boundingRect.left() + x);
  lineRect.setWidth(w);
  const auto textFormat = data.fontSettings.toTextCharFormat(C_TEXT);
  painter.fillRect(lineRect, textFormat.foreground());
  if (doSelection) {
    blockData.selections.append(createBlockCursorCharFormatRange(relativePos, textFormat.foreground().color(), textFormat.background().color()));
  }
}

auto TextEditorWidgetPrivate::paintAdditionalVisualWhitespaces(PaintEventData &data, QPainter &painter, qreal top) const -> void
{
  if (!m_displaySettings.m_visualizeWhitespace)
    return;

  const auto layout = data.block.layout();
  const auto nextBlockIsValid = data.block.next().isValid();
  const auto lineCount = layout->lineCount();
  if (lineCount >= 2 || !nextBlockIsValid) {
    painter.save();
    painter.setPen(data.visualWhitespaceFormat.foreground().color());
    for (auto i = 0; i < lineCount - 1; ++i) {
      // paint line wrap indicator
      auto line = layout->lineAt(i);
      auto lineRect = line.naturalTextRect().translated(data.offset.x(), top);
      const QChar visualArrow(ushort(0x21b5));
      painter.drawText(QPointF(lineRect.right(), lineRect.top() + line.ascent()), visualArrow);
    }
    if (!nextBlockIsValid) {
      // paint EOF symbol
      const auto line = layout->lineAt(lineCount - 1);
      auto lineRect = line.naturalTextRect().translated(data.offset.x(), top);
      const auto h = 4;
      lineRect.adjust(0, 0, -1, -1);
      QPainterPath path;
      const auto pos(lineRect.topRight() + QPointF(h + 4, line.ascent()));
      path.moveTo(pos);
      path.lineTo(pos + QPointF(-h, -h));
      path.lineTo(pos + QPointF(0, -2 * h));
      path.lineTo(pos + QPointF(h, -h));
      path.closeSubpath();
      painter.setBrush(painter.pen().color());
      painter.drawPath(path);
    }
    painter.restore();
  }
}

auto TextEditorWidgetPrivate::paintReplacement(PaintEventData &data, QPainter &painter, qreal top) const -> void
{
  const auto nextBlock = data.block.next();

  if (nextBlock.isValid() && !nextBlock.isVisible() && q->replacementVisible(data.block.blockNumber())) {
    const auto selectThis = data.textCursor.hasSelection() && nextBlock.position() >= data.textCursor.selectionStart() && nextBlock.position() < data.textCursor.selectionEnd();

    const auto selectionFormat = data.fontSettings.toTextCharFormat(C_SELECTION);

    painter.save();
    if (selectThis) {
      painter.setBrush(selectionFormat.background().style() != Qt::NoBrush ? selectionFormat.background() : QApplication::palette().brush(QPalette::Highlight));
    } else {
      const auto rc = q->replacementPenColor(data.block.blockNumber());
      if (rc.isValid())
        painter.setPen(rc);
    }

    const auto layout = data.block.layout();
    const auto line = layout->lineAt(layout->lineCount() - 1);
    auto lineRect = line.naturalTextRect().translated(data.offset.x(), top);
    lineRect.adjust(0, 0, -1, -1);

    auto replacement = q->foldReplacementText(data.block);
    const QString rectReplacement = QLatin1String(" {") + replacement + QLatin1String("}; ");

    const QRectF collapseRect(lineRect.right() + 12, lineRect.top(), q->fontMetrics().horizontalAdvance(rectReplacement), lineRect.height());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.translate(.5, .5);
    painter.drawRoundedRect(collapseRect.adjusted(0, 0, 0, -1), 3, 3);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.translate(-.5, -.5);

    if (const auto nextBlockUserData = TextDocumentLayout::textUserData(nextBlock)) {
      if (nextBlockUserData->foldingStartIncluded())
        replacement.prepend(nextBlock.text().trimmed().at(0));
    }

    auto lastInvisibleBlock = TextEditor::nextVisibleBlock(data.block, data.doc).previous();
    if (!lastInvisibleBlock.isValid())
      lastInvisibleBlock = data.doc->lastBlock();

    if (const auto blockUserData = TextDocumentLayout::textUserData(lastInvisibleBlock)) {
      if (blockUserData->foldingEndIncluded()) {
        auto right = lastInvisibleBlock.text().trimmed();
        if (right.endsWith(QLatin1Char(';'))) {
          right.chop(1);
          right = right.trimmed();
          replacement.append(right.right(right.endsWith('/') ? 2 : 1));
          replacement.append(QLatin1Char(';'));
        } else {
          replacement.append(right.right(right.endsWith('/') ? 2 : 1));
        }
      }
    }

    if (selectThis)
      painter.setPen(selectionFormat.foreground().color());
    painter.drawText(collapseRect, Qt::AlignCenter, replacement);
    painter.restore();
  }
}

auto TextEditorWidgetPrivate::paintWidgetBackground(const PaintEventData &data, QPainter &painter) const -> void
{
  painter.fillRect(data.eventRect, data.fontSettings.toTextCharFormat(C_TEXT).background());
}

auto TextEditorWidgetPrivate::paintOverlays(const PaintEventData &data, QPainter &painter) const -> void
{
  // draw the overlays, but only if we do not have a find scope, otherwise the
  // view becomes too noisy.
  if (m_findScope.isNull()) {
    if (m_overlay->isVisible())
      m_overlay->paint(&painter, data.eventRect);

    if (m_snippetOverlay->isVisible())
      m_snippetOverlay->paint(&painter, data.eventRect);

    if (!m_refactorOverlay->isEmpty())
      m_refactorOverlay->paint(&painter, data.eventRect);
  }

  if (!m_searchResultOverlay->isEmpty()) {
    m_searchResultOverlay->paint(&painter, data.eventRect);
    m_searchResultOverlay->clear();
  }
}

auto TextEditorWidgetPrivate::paintCursor(const PaintEventData &data, QPainter &painter) const -> void
{
  for (const auto &cursor : data.cursors) {
    painter.setPen(cursor.pen);
    cursor.layout->drawCursor(&painter, cursor.offset, cursor.pos, q->cursorWidth());
  }
}

auto TextEditorWidgetPrivate::setupBlockLayout(const PaintEventData &data, QPainter &painter, PaintEventBlockData &blockData) const -> void
{
  blockData.layout = data.block.layout();

  auto option = blockData.layout->textOption();
  if (data.suppressSyntaxInIfdefedOutBlock && TextDocumentLayout::ifdefedOut(data.block)) {
    option.setFlags(option.flags() | QTextOption::SuppressColors);
    painter.setPen(data.ifdefedOutFormat.foreground().color());
  } else {
    option.setFlags(option.flags() & ~QTextOption::SuppressColors);
    painter.setPen(data.context.palette.text().color());
  }
  blockData.layout->setTextOption(option);
  blockData.layout->setFont(data.doc->defaultFont());
}

auto TextEditorWidgetPrivate::setupSelections(const PaintEventData &data, PaintEventBlockData &blockData) const -> void
{
  QVector<QTextLayout::FormatRange> prioritySelections;
  for (auto i = 0; i < data.context.selections.size(); ++i) {
    const auto &range = data.context.selections.at(i);
    const auto selStart = range.cursor.selectionStart() - blockData.position;
    const auto selEnd = range.cursor.selectionEnd() - blockData.position;
    if (selStart < blockData.length && selEnd >= 0 && selEnd >= selStart) {
      QTextLayout::FormatRange o;
      o.start = selStart;
      o.length = selEnd - selStart;
      o.format = range.format;
      if (data.textCursor.hasSelection() && data.textCursor == range.cursor && data.textCursor.anchor() == range.cursor.anchor()) {
        const auto selectionFormat = data.fontSettings.toTextCharFormat(C_SELECTION);
        if (selectionFormat.background().style() != Qt::NoBrush)
          o.format.setBackground(selectionFormat.background());
        o.format.setForeground(selectionFormat.foreground());
      }
      if (data.textCursor.hasSelection() && i == data.context.selections.size() - 1 || o.format.foreground().style() == Qt::NoBrush && o.format.underlineStyle() != QTextCharFormat::NoUnderline && o.format.background() == Qt::NoBrush) {
        if (q->selectionVisible(data.block.blockNumber()))
          prioritySelections.append(o);
      } else {
        blockData.selections.append(o);
      }
    }
  }
  blockData.selections.append(prioritySelections);
}

static auto generateCursorData(const int cursorPos, const PaintEventData &data, const PaintEventBlockData &blockData, QPainter &painter) -> CursorData
{
  CursorData cursorData;
  cursorData.layout = blockData.layout;
  cursorData.offset = data.offset;
  cursorData.pos = cursorPos;
  cursorData.pen = painter.pen();
  return cursorData;
}

static auto blockContainsCursor(const PaintEventBlockData &blockData, const QTextCursor &cursor) -> bool
{
  const auto pos = cursor.position();
  return pos >= blockData.position && pos < blockData.position + blockData.length;
}

auto TextEditorWidgetPrivate::addCursorsPosition(PaintEventData &data, QPainter &painter, const PaintEventBlockData &blockData) const -> void
{
  if (!m_dndCursor.isNull()) {
    if (blockContainsCursor(blockData, m_dndCursor)) {
      data.cursors.append(generateCursorData(m_dndCursor.positionInBlock(), data, blockData, painter));
    }
  } else {
    for (const auto &cursor : m_cursors) {
      if (blockContainsCursor(blockData, cursor)) {
        data.cursors.append(generateCursorData(cursor.positionInBlock(), data, blockData, painter));
      }
    }
  }
}

auto TextEditorWidgetPrivate::nextVisibleBlock(const QTextBlock &block) const -> QTextBlock
{
  return TextEditor::nextVisibleBlock(block, q->document());
}

auto TextEditorWidgetPrivate::cleanupAnnotationCache() -> void
{
  const auto firstVisibleBlock = q->firstVisibleBlockNumber();
  const auto lastVisibleBlock = q->lastVisibleBlockNumber();
  auto lineIsVisble = [&](int blockNumber) {
    auto behindFirstVisibleBlock = [&]() {
      return firstVisibleBlock >= 0 && blockNumber >= firstVisibleBlock;
    };
    auto beforeLastVisibleBlock = [&]() {
      return lastVisibleBlock < 0 || lastVisibleBlock >= 0 && blockNumber <= lastVisibleBlock;
    };
    return behindFirstVisibleBlock() && beforeLastVisibleBlock();
  };
  auto it = m_annotationRects.begin();
  const auto end = m_annotationRects.end();
  while (it != end) {
    if (!lineIsVisble(it.key()))
      it = m_annotationRects.erase(it);
    else
      ++it;
  }
}

auto TextEditorWidget::paintEvent(QPaintEvent *e) -> void
{
  PaintEventData data(this, e, contentOffset());
  QTC_ASSERT(data.documentLayout, return);

  QPainter painter(viewport());
  // Set a brush origin so that the WaveUnderline knows where the wave started
  painter.setBrushOrigin(data.offset);

  data.block = firstVisibleBlock();
  data.context = getPaintContext();
  const auto textFormat = textDocument()->fontSettings().toTextCharFormat(C_TEXT);
  data.context.palette.setBrush(QPalette::Text, textFormat.foreground());
  data.context.palette.setBrush(QPalette::Base, textFormat.background());

  {
    // paint background
    d->paintWidgetBackground(data, painter);
    // draw backgrond to the right of the wrap column before everything else
    d->paintRightMarginArea(data, painter);
    // paint a blended background color depending on scope depth
    d->paintBlockHighlight(data, painter);
    // paint background of if defed out blocks in bigger chunks
    d->paintIfDefedOutBlocks(data, painter);
    d->paintRightMarginLine(data, painter);
    // paint find scope on top of ifdefed out blocks and right margin
    d->paintFindScope(data, painter);
    // paint search results on top of the find scope
    d->paintSearchResultOverlay(data, painter);
  }

  while (data.block.isValid()) {

    PaintEventBlockData blockData;
    blockData.boundingRect = blockBoundingRect(data.block).translated(data.offset);

    if (blockData.boundingRect.bottom() >= data.eventRect.top() && blockData.boundingRect.top() <= data.eventRect.bottom()) {

      d->setupBlockLayout(data, painter, blockData);
      blockData.position = data.block.position();
      blockData.length = data.block.length();
      d->setupSelections(data, blockData);

      d->paintCurrentLineHighlight(data, painter);

      auto drawCursor = false;
      auto drawCursorAsBlock = false;
      if (d->m_dndCursor.isNull()) {
        drawCursor = d->m_cursorVisible && anyOf(d->m_cursors, [&](const QTextCursor &cursor) {
          return blockContainsCursor(blockData, cursor);
        });
        drawCursorAsBlock = drawCursor && overwriteMode();
      } else {
        drawCursor = blockContainsCursor(blockData, d->m_dndCursor);
      }

      if (drawCursorAsBlock) {
        for (const auto &cursor : multiTextCursor()) {
          if (blockContainsCursor(blockData, cursor))
            d->paintCursorAsBlock(data, painter, blockData, cursor.position());
        }
      }

      paintBlock(&painter, data.block, data.offset, blockData.selections, data.eventRect);

      if (data.isEditable && data.context.cursorPosition<-1 && !blockData.layout->preeditAreaText().isEmpty()) {
const auto cursorPos = blockData.layout->preeditAreaPosition() - (data.context.cursorPosition + 2);
                                                           data.cursors.append(generateCursorData(cursorPos, data, blockData, painter));
            
                                                         }

            if (drawCursor && !drawCursorAsBlock) d->addCursorsPosition(data, painter, blockData); d->paintAdditionalVisualWhitespaces(data, painter, blockData.boundingRect.top());
      d->paintReplacement(data, painter, blockData.boundingRect.top());
    }
    d->updateLineAnnotation(data, blockData, painter);

    data.offset.ry() += blockData.boundingRect.height();

    if (data.offset.y() > data.viewportRect.height())
      break;

    data.block = data.block.next();

    if (!data.block.isVisible()) {
      if (data.block.blockNumber() == d->visibleFoldedBlockNumber) {
        data.visibleCollapsedBlock = data.block;
        data.visibleCollapsedBlockOffset = data.offset;
      }

      // invisible blocks do have zero line count
      data.block = data.doc->findBlockByLineNumber(data.block.firstLineNumber());
    }
  }

  d->cleanupAnnotationCache();

  painter.setPen(data.context.palette.text().color());

  d->updateAnimator(d->m_bracketsAnimator, painter);
  d->updateAnimator(d->m_autocompleteAnimator, painter);

  d->paintOverlays(data, painter);

  // draw the cursor last, on top of everything
  d->paintCursor(data, painter);

  // paint a popup with the content of the collapsed block
  drawCollapsedBlockPopup(painter, data.visibleCollapsedBlock, data.visibleCollapsedBlockOffset, data.eventRect);
}

auto TextEditorWidget::paintBlock(QPainter *painter, const QTextBlock &block, const QPointF &offset, const QVector<QTextLayout::FormatRange> &selections, const QRect &clipRect) const -> void
{
  block.layout()->draw(painter, offset, selections, clipRect);
}

auto TextEditorWidget::visibleFoldedBlockNumber() const -> int
{
  return d->visibleFoldedBlockNumber;
}

auto TextEditorWidget::drawCollapsedBlockPopup(QPainter &painter, const QTextBlock &block, QPointF offset, const QRect &clip) -> void
{
  if (!block.isValid())
    return;

  const auto margin = int(block.document()->documentMargin());
  qreal maxWidth = 0;
  qreal blockHeight = 0;
  auto b = block;

  while (!b.isVisible()) {
    b.setVisible(true); // make sure block bounding rect works
    auto r = blockBoundingRect(b).translated(offset);

    const auto layout = b.layout();
    for (auto i = layout->lineCount() - 1; i >= 0; --i)
      maxWidth = qMax(maxWidth, layout->lineAt(i).naturalTextWidth() + 2 * margin);

    blockHeight += r.height();

    b.setVisible(false); // restore previous state
    b.setLineCount(0);   // restore 0 line count for invisible block
    b = b.next();
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(.5, .5);
  auto brush = textDocument()->fontSettings().toTextCharFormat(C_TEXT).background();
  const auto ifdefedOutFormat = textDocument()->fontSettings().toTextCharFormat(C_DISABLED_CODE);
  if (ifdefedOutFormat.hasProperty(QTextFormat::BackgroundBrush))
    brush = ifdefedOutFormat.background();
  painter.setBrush(brush);
  painter.drawRoundedRect(QRectF(offset.x(), offset.y(), maxWidth, blockHeight).adjusted(0, 0, 0, 0), 3, 3);
  painter.restore();

  const auto end = b;
  b = block;
  while (b != end) {
    b.setVisible(true); // make sure block bounding rect works
    auto r = blockBoundingRect(b).translated(offset);
    const auto layout = b.layout();
    QVector<QTextLayout::FormatRange> selections;
    layout->draw(&painter, offset, selections, clip);

    b.setVisible(false); // restore previous state
    b.setLineCount(0);   // restore 0 line count for invisible block
    offset.ry() += r.height();
    b = b.next();
  }
}

auto TextEditorWidget::extraArea() const -> QWidget*
{
  return d->m_extraArea;
}

auto TextEditorWidget::extraAreaWidth(int *markWidthPtr) const -> int
{
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(document()->documentLayout());
  if (!documentLayout)
    return 0;

  if (!d->m_marksVisible && documentLayout->hasMarks)
    d->m_marksVisible = true;

  if (!d->m_marksVisible && !d->m_lineNumbersVisible && !d->m_codeFoldingVisible)
    return 0;

  auto space = 0;
  const auto fm(d->m_extraArea->fontMetrics());

  if (d->m_lineNumbersVisible) {
    auto fnt = d->m_extraArea->font();
    // this works under the assumption that bold or italic
    // can only make a font wider
    const auto currentLineNumberFormat = textDocument()->fontSettings().toTextCharFormat(C_CURRENT_LINE_NUMBER);
    fnt.setBold(currentLineNumberFormat.font().bold());
    fnt.setItalic(currentLineNumberFormat.font().italic());
    const QFontMetrics linefm(fnt);

    space += linefm.horizontalAdvance(QLatin1Char('9')) * lineNumberDigits();
  }
  auto markWidth = 0;

  if (d->m_marksVisible) {
    markWidth += documentLayout->maxMarkWidthFactor * fm.lineSpacing() + 2;

    //     if (documentLayout->doubleMarkCount)
    //         markWidth += fm.lineSpacing() / 3;
    space += markWidth;
  } else {
    space += 2;
  }

  if (markWidthPtr)
    *markWidthPtr = markWidth;

  space += 4;

  if (d->m_codeFoldingVisible)
    space += foldBoxWidth(fm);

  if (viewportMargins() != QMargins{isLeftToRight() ? space : 0, 0, isLeftToRight() ? 0 : space, 0})
    d->slotUpdateExtraAreaWidth(space);

  return space;
}

auto TextEditorWidgetPrivate::slotUpdateExtraAreaWidth(optional<int> width) -> void
{
  if (!width.has_value())
    width = q->extraAreaWidth();
  if (q->isLeftToRight())
    q->setViewportMargins(*width, 0, 0, 0);
  else
    q->setViewportMargins(0, 0, *width, 0);
}

struct Internal::ExtraAreaPaintEventData {
  ExtraAreaPaintEventData(const TextEditorWidget *editor, TextEditorWidgetPrivate *d) : doc(editor->document()), documentLayout(qobject_cast<TextDocumentLayout*>(doc->documentLayout())), selectionStart(editor->textCursor().selectionStart()), selectionEnd(editor->textCursor().selectionEnd()), fontMetrics(d->m_extraArea->font()), lineSpacing(fontMetrics.lineSpacing()), markWidth(d->m_marksVisible ? lineSpacing : 0), collapseColumnWidth(d->m_codeFoldingVisible ? foldBoxWidth(fontMetrics) : 0), extraAreaWidth(d->m_extraArea->width() - collapseColumnWidth), currentLineNumberFormat(editor->textDocument()->fontSettings().toTextCharFormat(C_CURRENT_LINE_NUMBER)), palette(d->m_extraArea->palette())
  {
    palette.setCurrentColorGroup(QPalette::Active);
  }

  QTextBlock block;
  const QTextDocument *doc;
  const TextDocumentLayout *documentLayout;
  const int selectionStart;
  const int selectionEnd;
  const QFontMetrics fontMetrics;
  const int lineSpacing;
  const int markWidth;
  const int collapseColumnWidth;
  const int extraAreaWidth;
  const QTextCharFormat currentLineNumberFormat;
  QPalette palette;
};

auto TextEditorWidgetPrivate::paintLineNumbers(QPainter &painter, const ExtraAreaPaintEventData &data, const QRectF &blockBoundingRect) const -> void
{
  if (!m_lineNumbersVisible)
    return;

  const auto &number = q->lineNumber(data.block.blockNumber());
  const auto selected = data.selectionStart<data.block.position() + data.block.length() && data.selectionEnd>
  data.block.position() || data.selectionStart == data.selectionEnd && data.selectionEnd == data.block.position();
  if (selected) {
    painter.save();
    auto f = painter.font();
    f.setBold(data.currentLineNumberFormat.font().bold());
    f.setItalic(data.currentLineNumberFormat.font().italic());
    painter.setFont(f);
    painter.setPen(data.currentLineNumberFormat.foreground().color());
    if (data.currentLineNumberFormat.background() != Qt::NoBrush) {
      painter.fillRect(QRectF(0, blockBoundingRect.top(), data.extraAreaWidth, blockBoundingRect.height()), data.currentLineNumberFormat.background().color());
    }
  }
  painter.drawText(QRectF(data.markWidth, blockBoundingRect.top(), data.extraAreaWidth - data.markWidth - 4, blockBoundingRect.height()), Qt::AlignRight, number);
  if (selected)
    painter.restore();
}

auto TextEditorWidgetPrivate::paintTextMarks(QPainter &painter, const ExtraAreaPaintEventData &data, const QRectF &blockBoundingRect) const -> void
{
  const auto userData = static_cast<TextBlockUserData*>(data.block.userData());
  if (!userData || !m_marksVisible)
    return;
  auto xoffset = 0;
  const auto marks = userData->marks();
  auto it = marks.constBegin();
  if (marks.size() > 3) {
    // We want the 3 with the highest priority that have an icon so iterate from the back
    auto count = 0;
    it = marks.constEnd() - 1;
    while (it != marks.constBegin()) {
      if ((*it)->isVisible() && !(*it)->icon().isNull())
        ++count;
      if (count == 3)
        break;
      --it;
    }
  }
  const auto end = marks.constEnd();
  for (; it != end; ++it) {
    const auto mark = *it;
    if (!mark->isVisible() && !mark->icon().isNull())
      continue;
    const auto height = data.lineSpacing - 1;
    const auto width = int(.5 + height * mark->widthFactor());
    const QRect r(xoffset, int(blockBoundingRect.top()), width, height);
    mark->paintIcon(&painter, r);
    xoffset += 2;
  }
}

static auto drawRectBox(QPainter *painter, const QRect &rect, const QPalette &pal) -> void
{
  painter->save();
  painter->setOpacity(0.5);
  painter->fillRect(rect, pal.brush(QPalette::Highlight));
  painter->restore();
}

auto TextEditorWidgetPrivate::paintCodeFolding(QPainter &painter, const ExtraAreaPaintEventData &data, const QRectF &blockBoundingRect) const -> void
{
  if (!m_codeFoldingVisible)
    return;

  auto extraAreaHighlightFoldBlockNumber = -1;
  auto extraAreaHighlightFoldEndBlockNumber = -1;
  if (!m_highlightBlocksInfo.isEmpty()) {
    extraAreaHighlightFoldBlockNumber = m_highlightBlocksInfo.open.last();
    extraAreaHighlightFoldEndBlockNumber = m_highlightBlocksInfo.close.first();
  }

  const auto &nextBlock = data.block.next();
  const auto nextBlockUserData = TextDocumentLayout::textUserData(nextBlock);

  const auto drawBox = nextBlockUserData && TextDocumentLayout::foldingIndent(data.block) < nextBlockUserData->foldingIndent();

  const auto blockNumber = data.block.blockNumber();
  const auto active = blockNumber == extraAreaHighlightFoldBlockNumber;
  const auto hovered = blockNumber >= extraAreaHighlightFoldBlockNumber && blockNumber <= extraAreaHighlightFoldEndBlockNumber;

  const auto boxWidth = foldBoxWidth(data.fontMetrics);
  if (hovered) {
    const auto itop = qRound(blockBoundingRect.top());
    const auto ibottom = qRound(blockBoundingRect.bottom());
    const auto box = QRect(data.extraAreaWidth + 1, itop, boxWidth - 2, ibottom - itop);
    drawRectBox(&painter, box, data.palette);
  }

  if (drawBox) {
    const auto expanded = nextBlock.isVisible();
    const auto size = boxWidth / 4;
    const QRect box(data.extraAreaWidth + size, int(blockBoundingRect.top()) + size, 2 * size + 1, 2 * size + 1);
    drawFoldingMarker(&painter, data.palette, box, expanded, active, hovered);
  }
}

auto TextEditorWidgetPrivate::paintRevisionMarker(QPainter &painter, const ExtraAreaPaintEventData &data, const QRectF &blockBoundingRect) const -> void
{
  if (m_revisionsVisible &&data
  .
  block.revision() != data.documentLayout->lastSaveRevision
  )
  {
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    if (data.block.revision() < 0)
      painter.setPen(QPen(Qt::darkGreen, 2));
    else
      painter.setPen(QPen(Qt::red, 2));
    painter.drawLine(data.extraAreaWidth - 1, int(blockBoundingRect.top()), data.extraAreaWidth - 1, int(blockBoundingRect.bottom()) - 1);
    painter.restore();
  }
}

auto TextEditorWidget::extraAreaPaintEvent(QPaintEvent *e) -> void
{
  ExtraAreaPaintEventData data(this, d);
  QTC_ASSERT(data.documentLayout, return);

  QPainter painter(d->m_extraArea);

  painter.fillRect(e->rect(), data.palette.color(QPalette::Window));

  data.block = firstVisibleBlock();
  auto offset = contentOffset();
  auto boundingRect = blockBoundingRect(data.block).translated(offset);

  while (data.block.isValid() && boundingRect.top() <= e->rect().bottom()) {
    if (boundingRect.bottom() >= e->rect().top()) {

      painter.setPen(data.palette.color(QPalette::Dark));

      d->paintLineNumbers(painter, data, boundingRect);

      if (d->m_codeFoldingVisible || d->m_marksVisible) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, false);

        d->paintTextMarks(painter, data, boundingRect);
        d->paintCodeFolding(painter, data, boundingRect);

        painter.restore();
      }

      d->paintRevisionMarker(painter, data, boundingRect);
    }

    offset.ry() += boundingRect.height();
    data.block = d->nextVisibleBlock(data.block);
    boundingRect = blockBoundingRect(data.block).translated(offset);
  }
}

auto TextEditorWidgetPrivate::drawFoldingMarker(QPainter *painter, const QPalette &pal, const QRect &rect, bool expanded, bool active, bool hovered) const -> void
{
  auto s = q->style();
  if (auto ms = qobject_cast<ManhattanStyle*>(s))
    s = ms->baseStyle();

  QStyleOptionViewItem opt;
  opt.rect = rect;
  opt.state = QStyle::State_Active | QStyle::State_Item | QStyle::State_Children;
  if (expanded)
    opt.state |= QStyle::State_Open;
  if (active)
    opt.state |= QStyle::State_MouseOver | QStyle::State_Enabled | QStyle::State_Selected;
  if (hovered)
    opt.palette.setBrush(QPalette::Window, pal.highlight());

  auto className = s->metaObject()->className();

  // Do not use the windows folding marker since we cannot style them and the default hover color
  // is a blue which does not guarantee an high contrast on all themes.
  static QPointer<QStyle> fusionStyleOverwrite = nullptr;
  if (!qstrcmp(className, "QWindowsVistaStyle")) {
    if (fusionStyleOverwrite.isNull())
      fusionStyleOverwrite = QStyleFactory::create("fusion");
    if (!fusionStyleOverwrite.isNull()) {
      s = fusionStyleOverwrite.data();
      className = s->metaObject()->className();
    }
  }

  if (!qstrcmp(className, "OxygenStyle")) {
    const auto direction = expanded ? QStyle::PE_IndicatorArrowDown : QStyle::PE_IndicatorArrowRight;
    StyleHelper::drawArrow(direction, painter, &opt);
  } else {
    // QGtkStyle needs a small correction to draw the marker in the right place
    if (!qstrcmp(className, "QGtkStyle"))
      opt.rect.translate(-2, 0);
    else if (!qstrcmp(className, "QMacStyle"))
      opt.rect.translate(-2, 0);
    else if (!qstrcmp(className, "QFusionStyle"))
      opt.rect.translate(0, -1);

    s->drawPrimitive(QStyle::PE_IndicatorBranch, &opt, painter, q);
  }
}

auto TextEditorWidgetPrivate::slotUpdateRequest(const QRect &r, int dy) -> void
{
  if (dy) {
    m_extraArea->scroll(0, dy);
  } else if (r.width() > 4) {
    // wider than cursor width, not just cursor blinking
    m_extraArea->update(0, r.y(), m_extraArea->width(), r.height());
    if (!m_searchExpr.pattern().isEmpty()) {
      const auto m = m_searchResultOverlay->dropShadowWidth();
      q->viewport()->update(r.adjusted(-m, -m, m, m));
    }
  }

  if (r.contains(q->viewport()->rect()))
    slotUpdateExtraAreaWidth();
}

auto TextEditorWidgetPrivate::saveCurrentCursorPositionForNavigation() -> void
{
  m_lastCursorChangeWasInteresting = true;
  m_tempNavigationState = q->saveState();
}

auto TextEditorWidgetPrivate::updateCurrentLineHighlight() -> void
{
  QList<QTextEdit::ExtraSelection> extraSelections;

  if (m_highlightCurrentLine) {
    for (const auto &c : m_cursors) {
      QTextEdit::ExtraSelection sel;
      sel.format.setBackground(m_document->fontSettings().toTextCharFormat(C_CURRENT_LINE).background());
      sel.format.setProperty(QTextFormat::FullWidthSelection, true);
      sel.cursor = c;
      sel.cursor.clearSelection();
      extraSelections.append(sel);
    }
  }
  updateCurrentLineInScrollbar();

  q->setExtraSelections(TextEditorWidget::CurrentLineSelection, extraSelections);

  // the extra area shows information for the entire current block, not just the currentline.
  // This is why we must force a bigger update region.
  QList<int> cursorBlockNumbers;
  const auto offset = q->contentOffset();
  for (const auto &c : m_cursors) {
    auto cursorBlockNumber = c.blockNumber();
    if (!m_cursorBlockNumbers.contains(cursorBlockNumber)) {
      auto block = c.block();
      if (block.isValid() && block.isVisible())
        m_extraArea->update(q->blockBoundingGeometry(block).translated(offset).toAlignedRect());
    }
    if (!cursorBlockNumbers.contains(c.blockNumber()))
      cursorBlockNumbers << c.blockNumber();
  }
  if (m_cursorBlockNumbers != cursorBlockNumbers) {
    for (auto oldBlock : m_cursorBlockNumbers) {
      if (cursorBlockNumbers.contains(oldBlock))
        continue;
      auto block = m_document->document()->findBlockByNumber(oldBlock);
      if (block.isValid() && block.isVisible())
        m_extraArea->update(q->blockBoundingGeometry(block).translated(offset).toAlignedRect());
    }
    m_cursorBlockNumbers = cursorBlockNumbers;
  }
}

auto TextEditorWidget::slotCursorPositionChanged() -> void
{
  #if 0
    qDebug() << "block" << textCursor().blockNumber()+1
            << "brace depth:" << BaseTextDocumentLayout::braceDepth(textCursor().block())
            << "indent:" << BaseTextDocumentLayout::userData(textCursor().block())->foldingIndent();
  #endif
  if (!d->m_contentsChanged && d->m_lastCursorChangeWasInteresting) {
    if (EditorManager::currentEditor() && EditorManager::currentEditor()->widget() == this)
      EditorManager::addCurrentPositionToNavigationHistory(d->m_tempNavigationState);
    d->m_lastCursorChangeWasInteresting = false;
  } else if (d->m_contentsChanged) {
    d->saveCurrentCursorPositionForNavigation();
    if (EditorManager::currentEditor() && EditorManager::currentEditor()->widget() == this)
      EditorManager::setLastEditLocation(EditorManager::currentEditor());
  }
  auto cursor = multiTextCursor();
  cursor.replaceMainCursor(textCursor());
  setMultiTextCursor(cursor);
  d->updateCursorSelections();
  d->updateHighlights();
}

auto TextEditorWidgetPrivate::updateHighlights() -> void
{
  if (m_parenthesesMatchingEnabled &&q
  ->
  hasFocus()
  )
  {
    // Delay update when no matching is displayed yet, to avoid flicker
    if (q->extraSelections(TextEditorWidget::ParenthesesMatchingSelection).isEmpty() && m_bracketsAnimator == nullptr) {
      m_parenthesesMatchingTimer.start();
    } else {
      // when we uncheck "highlight matching parentheses"
      // we need clear current selection before viewport update
      // otherwise we get sticky highlighted parentheses
      if (!m_displaySettings.m_highlightMatchingParentheses)
        q->setExtraSelections(TextEditorWidget::ParenthesesMatchingSelection, QList<QTextEdit::ExtraSelection>());

      // use 0-timer, not direct call, to give the syntax highlighter a chance
      // to update the parentheses information
      m_parenthesesMatchingTimer.start(0);
    }
  }

  if (m_highlightAutoComplete && !m_autoCompleteHighlightPos.isEmpty()) {
    QMetaObject::invokeMethod(this, [this]() {
      const auto &cursor = q->textCursor();
      auto popAutoCompletion = [&]() {
        return !m_autoCompleteHighlightPos.isEmpty() && m_autoCompleteHighlightPos.last() != cursor;
      };
      if (!m_keepAutoCompletionHighlight && !q->hasFocus() || popAutoCompletion()) {
        while (popAutoCompletion())
          m_autoCompleteHighlightPos.pop_back();
        updateAutoCompleteHighlight();
      }
    }, Qt::QueuedConnection);
  }

  updateCurrentLineHighlight();

  if (m_displaySettings.m_highlightBlocks) {
    const auto cursor = q->textCursor();
    extraAreaHighlightFoldedBlockNumber = cursor.blockNumber();
    m_highlightBlocksTimer.start(100);
  }
}

auto TextEditorWidgetPrivate::updateCurrentLineInScrollbar() -> void
{
  if (m_highlightCurrentLine && m_highlightScrollBarController) {
    m_highlightScrollBarController->removeHighlights(Constants::SCROLL_BAR_CURRENT_LINE);
    for (const auto &tc : m_cursors) {
      if (const auto layout = tc.block().layout()) {
        const auto pos = tc.block().firstLineNumber() + layout->lineForTextPosition(tc.positionInBlock()).lineNumber();
        m_highlightScrollBarController->addHighlight({Constants::SCROLL_BAR_CURRENT_LINE, pos, Theme::TextEditor_CurrentLine_ScrollBarColor, Highlight::HighestPriority});
      }
    }
  }
}

auto TextEditorWidgetPrivate::slotUpdateBlockNotify(const QTextBlock &block) -> void
{
  static auto blockRecursion = false;
  if (blockRecursion)
    return;
  blockRecursion = true;
  if (m_overlay->isVisible()) {
    /* an overlay might draw outside the block bounderies, force
       complete viewport update */
    q->viewport()->update();
  } else {
    if (block.previous().isValid() && block.userState() != block.previous().userState()) {
      /* The syntax highlighting state changes. This opens up for
         the possibility that the paragraph has braces that support
         code folding. In this case, do the Save thing and also
         update the previous block, which might contain a fold
         box which now is Invalid.*/
      emit q->requestBlockUpdate(block.previous());
    }

    for (const auto &scope : m_findScope) {
      QSet<int> updatedBlocks;
      const auto blockContainsFindScope = block.position() < scope.selectionEnd() && block.position() + block.length() >= scope.selectionStart();
      if (blockContainsFindScope) {
        auto b = block.document()->findBlock(scope.selectionStart());
        do {
          if (!updatedBlocks.contains(b.blockNumber())) {
            updatedBlocks << b.blockNumber();
            emit q->requestBlockUpdate(b);
          }
          b = b.next();
        } while (b.isValid() && b.position() < scope.selectionEnd());
      }
    }
  }
  blockRecursion = false;
}

auto TextEditorWidget::timerEvent(QTimerEvent *e) -> void
{
  if (e->timerId() == d->autoScrollTimer.timerId()) {
    const auto globalPos = QCursor::pos();
    const auto pos = d->m_extraArea->mapFromGlobal(globalPos);
    const auto visible = d->m_extraArea->rect();
    verticalScrollBar()->triggerAction(pos.y() < visible.center().y() ? QAbstractSlider::SliderSingleStepSub : QAbstractSlider::SliderSingleStepAdd);
    QMouseEvent ev(QEvent::MouseMove, pos, globalPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    extraAreaMouseEvent(&ev);
    auto delta = qMax(pos.y() - visible.top(), visible.bottom() - pos.y()) - visible.height();
    if (delta < 7)
      delta = 7;
    const auto timeout = 4900 / (delta * delta);
    d->autoScrollTimer.start(timeout, this);

  } else if (e->timerId() == d->foldedBlockTimer.timerId()) {
    d->visibleFoldedBlockNumber = d->suggestedVisibleFoldedBlockNumber;
    d->suggestedVisibleFoldedBlockNumber = -1;
    d->foldedBlockTimer.stop();
    viewport()->update();
  } else if (e->timerId() == d->m_cursorFlashTimer.timerId()) {
    d->m_cursorVisible = !d->m_cursorVisible;
    viewport()->update(d->cursorUpdateRect(d->m_cursors));
  }
  QPlainTextEdit::timerEvent(e);
}

auto TextEditorWidgetPrivate::clearVisibleFoldedBlock() -> void
{
  if (suggestedVisibleFoldedBlockNumber) {
    suggestedVisibleFoldedBlockNumber = -1;
    foldedBlockTimer.stop();
  }
  if (visibleFoldedBlockNumber >= 0) {
    visibleFoldedBlockNumber = -1;
    q->viewport()->update();
  }
}

auto TextEditorWidget::mouseMoveEvent(QMouseEvent *e) -> void
{
  d->requestUpdateLink(e);

  auto onLink = false;
  if (d->m_linkPressed && d->m_currentLink.hasValidTarget()) {
    const auto eventCursorPosition = cursorForPosition(e->pos()).position();
    if (eventCursorPosition < d->m_currentLink.linkTextStart || eventCursorPosition > d->m_currentLink.linkTextEnd) {
      d->m_linkPressed = false;
    } else {
      onLink = true;
    }
  }

  static optional<MultiTextCursor> startMouseMoveCursor;
  if (e->buttons() == Qt::LeftButton && e->modifiers() & Qt::AltModifier) {
    if (!startMouseMoveCursor.has_value()) {
      startMouseMoveCursor = multiTextCursor();
      auto c = startMouseMoveCursor->takeMainCursor();
      if (!startMouseMoveCursor->hasMultipleCursors() && !startMouseMoveCursor->hasSelection()) {
        startMouseMoveCursor.emplace(MultiTextCursor());
      }
      c.setPosition(c.anchor());
      startMouseMoveCursor->addCursor(c);
    }
    auto cursor = *startMouseMoveCursor;
    const auto anchorCursor = cursor.takeMainCursor();
    const auto eventCursor = cursorForPosition(e->pos());

    const auto tabSettings = d->m_document->tabSettings();
    auto eventColumn = tabSettings.columnAt(eventCursor.block().text(), eventCursor.positionInBlock());
    if (eventCursor.positionInBlock() == eventCursor.block().length() - 1) {
      eventColumn += int((e->pos().x() - cursorRect(eventCursor).center().x()) / QFontMetricsF(font()).horizontalAdvance(' '));
    }

    const auto anchorColumn = tabSettings.columnAt(anchorCursor.block().text(), anchorCursor.positionInBlock());
    const TextEditorWidgetPrivate::BlockSelection blockSelection = {eventCursor.blockNumber(), eventColumn, anchorCursor.blockNumber(), anchorColumn};

    cursor.setCursors(d->generateCursorsForBlockSelection(blockSelection));
    if (!cursor.isNull())
      setMultiTextCursor(cursor);
  } else {
    if (startMouseMoveCursor.has_value())
      startMouseMoveCursor.reset();
    if (e->buttons() == Qt::NoButton) {
      const auto collapsedBlock = d->foldedBlockAt(e->pos());
      const auto blockNumber = collapsedBlock.next().blockNumber();
      if (blockNumber < 0) {
        d->clearVisibleFoldedBlock();
      } else if (blockNumber != d->visibleFoldedBlockNumber) {
        d->suggestedVisibleFoldedBlockNumber = blockNumber;
        d->foldedBlockTimer.start(40, this);
      }

      const auto refactorMarker = d->m_refactorOverlay->markerAt(e->pos());

      // Update the mouse cursor
      if ((collapsedBlock.isValid() || refactorMarker.isValid()) && !d->m_mouseOnFoldedMarker) {
        d->m_mouseOnFoldedMarker = true;
        viewport()->setCursor(Qt::PointingHandCursor);
      } else if (!collapsedBlock.isValid() && !refactorMarker.isValid() && d->m_mouseOnFoldedMarker) {
        d->m_mouseOnFoldedMarker = false;
        viewport()->setCursor(Qt::IBeamCursor);
      }
    } else if (!onLink || e->buttons() != Qt::LeftButton || e->modifiers() != Qt::ControlModifier) {
      QPlainTextEdit::mouseMoveEvent(e);
    }
  }

  if (viewport()->cursor().shape() == Qt::BlankCursor)
    viewport()->setCursor(Qt::IBeamCursor);
}

static auto handleForwardBackwardMouseButtons(QMouseEvent *e) -> bool
{
  if (e->button() == Qt::XButton1) {
    EditorManager::goBackInNavigationHistory();
    return true;
  }
  if (e->button() == Qt::XButton2) {
    EditorManager::goForwardInNavigationHistory();
    return true;
  }

  return false;
}

auto TextEditorWidget::mousePressEvent(QMouseEvent *e) -> void
{
  if (e->button() == Qt::LeftButton) {
    auto multiCursor = multiTextCursor();
    const auto &cursor = cursorForPosition(e->pos());
    if (e->modifiers() & Qt::AltModifier && !(e->modifiers() & Qt::ControlModifier)) {
      if (e->modifiers() & Qt::ShiftModifier) {
        auto c = multiCursor.mainCursor();
        c.setPosition(cursor.position(), QTextCursor::KeepAnchor);
        multiCursor.replaceMainCursor(c);
      } else {
        multiCursor.addCursor(cursor);
      }
      setMultiTextCursor(multiCursor);
      return;
    }
    if (multiCursor.hasMultipleCursors())
      setMultiTextCursor(MultiTextCursor({cursor}));

    const auto foldedBlock = d->foldedBlockAt(e->pos());
    if (foldedBlock.isValid()) {
      d->toggleBlockVisible(foldedBlock);
      viewport()->setCursor(Qt::IBeamCursor);
    }

    const auto refactorMarker = d->m_refactorOverlay->markerAt(e->pos());
    if (refactorMarker.isValid()) {
      if (refactorMarker.callback)
        refactorMarker.callback(this);
    } else {
      d->m_linkPressed = d->isMouseNavigationEvent(e);
    }
  } else if (e->button() == Qt::RightButton) {
    const auto eventCursorPosition = cursorForPosition(e->pos()).position();
    if (eventCursorPosition < textCursor().selectionStart() || eventCursorPosition > textCursor().selectionEnd()) {
      setTextCursor(cursorForPosition(e->pos()));
    }
  }

  if (HostOsInfo::isLinuxHost() && handleForwardBackwardMouseButtons(e))
    return;

  QPlainTextEdit::mousePressEvent(e);
}

auto TextEditorWidget::mouseReleaseEvent(QMouseEvent *e) -> void
{
  const auto button = e->button();
  if (d->m_linkPressed && d->isMouseNavigationEvent(e) && button == Qt::LeftButton) {
    EditorManager::addCurrentPositionToNavigationHistory();
    auto inNextSplit = e->modifiers() & Qt::AltModifier && !alwaysOpenLinksInNextSplit() || alwaysOpenLinksInNextSplit() && !(e->modifiers() & Qt::AltModifier);

    findLinkAt(textCursor(), [inNextSplit, self = QPointer(this)](const Link &symbolLink) {
      if (self && self->openLink(symbolLink, inNextSplit))
        self->d->clearLink();
    }, true, inNextSplit);
  } else if (button == Qt::MiddleButton && !isReadOnly() && QGuiApplication::clipboard()->supportsSelection()) {
    if (!(e->modifiers() & Qt::AltModifier))
      doSetTextCursor(cursorForPosition(e->pos()));
    if (const auto md = QGuiApplication::clipboard()->mimeData(QClipboard::Selection))
      insertFromMimeData(md);
    e->accept();
    return;
  }

  if (!HostOsInfo::isLinuxHost() && handleForwardBackwardMouseButtons(e))
    return;

  QPlainTextEdit::mouseReleaseEvent(e);

  d->setClipboardSelection();
  const auto plainTextEditCursor = textCursor();
  const auto multiMainCursor = multiTextCursor().mainCursor();
  if (multiMainCursor.position() != plainTextEditCursor.position() || multiMainCursor.anchor() != plainTextEditCursor.anchor()) {
    doSetTextCursor(plainTextEditCursor, true);
  }
}

auto TextEditorWidget::mouseDoubleClickEvent(QMouseEvent *e) -> void
{
  if (e->button() == Qt::LeftButton) {
    auto cursor = textCursor();
    const auto position = cursor.position();
    if (TextBlockUserData::findPreviousOpenParenthesis(&cursor, false, true)) {
      if (position - cursor.position() == 1 && selectBlockUp())
        return;
    }
  }

  QPlainTextEdit::mouseDoubleClickEvent(e);
}

auto TextEditorWidgetPrivate::setClipboardSelection() -> void
{
  const auto clipboard = QGuiApplication::clipboard();
  if (m_cursors.hasSelection() && clipboard->supportsSelection())
    clipboard->setMimeData(q->createMimeDataFromSelection(), QClipboard::Selection);
}

auto TextEditorWidget::leaveEvent(QEvent *e) -> void
{
  // Clear link emulation when the mouse leaves the editor
  d->clearLink();
  QPlainTextEdit::leaveEvent(e);
}

auto TextEditorWidget::keyReleaseEvent(QKeyEvent *e) -> void
{
  if (e->key() == Qt::Key_Control) {
    d->clearLink();
  } else if (e->key() == Qt::Key_Shift && d->m_behaviorSettings.m_constrainHoverTooltips && ToolTip::isVisible()) {
    ToolTip::hide();
  } else if (e->key() == Qt::Key_Alt && d->m_maybeFakeTooltipEvent) {
    d->m_maybeFakeTooltipEvent = false;
    d->processTooltipRequest(textCursor());
  }

  QPlainTextEdit::keyReleaseEvent(e);
}

auto TextEditorWidget::dragEnterEvent(QDragEnterEvent *e) -> void
{
  // If the drag event contains URLs, we don't want to insert them as text
  if (e->mimeData()->hasUrls()) {
    e->ignore();
    return;
  }

  QPlainTextEdit::dragEnterEvent(e);
}

static auto appendMenuActionsFromContext(QMenu *menu, Id menuContextId) -> void
{
  ActionContainer *mcontext = ActionManager::actionContainer(menuContextId);
  const QMenu *contextMenu = mcontext->menu();

  foreach(QAction *action, contextMenu->actions())
    menu->addAction(action);
}

auto TextEditorWidget::showDefaultContextMenu(QContextMenuEvent *e, Id menuContextId) -> void
{
  QMenu menu;
  if (menuContextId.isValid())
    appendMenuActionsFromContext(&menu, menuContextId);
  appendStandardContextMenuActions(&menu);
  menu.exec(e->globalPos());
}

auto TextEditorWidget::addHoverHandler(BaseHoverHandler *handler) -> void
{
  if (!d->m_hoverHandlers.contains(handler))
    d->m_hoverHandlers.append(handler);
}

auto TextEditorWidget::removeHoverHandler(BaseHoverHandler *handler) -> void
{
  d->m_hoverHandlers.removeAll(handler);
  d->m_hoverHandlerRunner.handlerRemoved(handler);
}

#ifdef WITH_TESTS
void TextEditorWidget::processTooltipRequest(const QTextCursor &c)
{
    d->processTooltipRequest(c);
}
#endif

auto TextEditorWidget::extraAreaLeaveEvent(QEvent *) -> void
{
  d->extraAreaPreviousMarkTooltipRequestedLine = -1;
  ToolTip::hide();

  // fake missing mouse move event from Qt
  QMouseEvent me(QEvent::MouseMove, QPoint(-1, -1), Qt::NoButton, {}, {});
  extraAreaMouseEvent(&me);
}

auto TextEditorWidget::extraAreaContextMenuEvent(QContextMenuEvent *e) -> void
{
  if (d->m_marksVisible) {
    const auto cursor = cursorForPosition(QPoint(0, e->pos().y()));
    const auto contextMenu = new QMenu(this);
    emit markContextMenuRequested(this, cursor.blockNumber() + 1, contextMenu);
    if (!contextMenu->isEmpty())
      contextMenu->exec(e->globalPos());
    delete contextMenu;
    e->accept();
  }
}

auto TextEditorWidget::updateFoldingHighlight(const QPoint &pos) -> void
{
  if (!d->m_codeFoldingVisible)
    return;

  const auto cursor = cursorForPosition(QPoint(0, pos.y()));

  // Update which folder marker is highlighted
  const auto highlightBlockNumber = d->extraAreaHighlightFoldedBlockNumber;
  d->extraAreaHighlightFoldedBlockNumber = -1;

  if (pos.x() > extraArea()->width() - foldBoxWidth(fontMetrics())) {
    d->extraAreaHighlightFoldedBlockNumber = cursor.blockNumber();
  } else if (d->m_displaySettings.m_highlightBlocks) {
    const auto cursor = textCursor();
    d->extraAreaHighlightFoldedBlockNumber = cursor.blockNumber();
  }

  if (highlightBlockNumber != d->extraAreaHighlightFoldedBlockNumber)
    d->m_highlightBlocksTimer.start(d->m_highlightBlocksInfo.isEmpty() ? 120 : 0);
}

auto TextEditorWidget::extraAreaMouseEvent(QMouseEvent *e) -> void
{
  auto cursor = cursorForPosition(QPoint(0, e->pos().y()));

  int markWidth;
  extraAreaWidth(&markWidth);
  const auto inMarkArea = e->pos().x() <= markWidth && e->pos().x() >= 0;

  if (d->m_codeFoldingVisible && e->type() == QEvent::MouseMove && e->buttons() == 0) {
    // mouse tracking
    updateFoldingHighlight(e->pos());
  }

  // Set whether the mouse cursor is a hand or normal arrow
  if (e->type() == QEvent::MouseMove) {
    if (inMarkArea) {
      const auto line = cursor.blockNumber() + 1;
      if (d->extraAreaPreviousMarkTooltipRequestedLine != line) {
        if (const auto data = static_cast<TextBlockUserData*>(cursor.block().userData())) {
          if (data->marks().isEmpty())
            ToolTip::hide();
          else
            d->showTextMarksToolTip(mapToGlobal(e->pos()), data->marks());
        }
      }
      d->extraAreaPreviousMarkTooltipRequestedLine = line;
    }

    if (!d->m_markDragging && e->buttons() & Qt::LeftButton && !d->m_markDragStart.isNull()) {
      const auto dist = (e->pos() - d->m_markDragStart).manhattanLength();
      if (dist > QApplication::startDragDistance()) {
        d->m_markDragging = true;
        const auto height = fontMetrics().lineSpacing() - 1;
        const auto width = int(.5 + height * d->m_dragMark->widthFactor());
        d->m_markDragCursor = QCursor(d->m_dragMark->icon().pixmap({height, width}));
        d->m_dragMark->setVisible(false);
        QGuiApplication::setOverrideCursor(d->m_markDragCursor);
      }
    }

    if (d->m_markDragging) {
      QGuiApplication::changeOverrideCursor(inMarkArea ? d->m_markDragCursor : QCursor(Qt::ForbiddenCursor));
    } else if (inMarkArea != (d->m_extraArea->cursor().shape() == Qt::PointingHandCursor)) {
      d->m_extraArea->setCursor(inMarkArea ? Qt::PointingHandCursor : Qt::ArrowCursor);
    }
  }

  if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonDblClick) {
    if (e->button() == Qt::LeftButton) {
      const auto boxWidth = foldBoxWidth(fontMetrics());
      if (d->m_codeFoldingVisible && e->pos().x() > extraArea()->width() - boxWidth) {
        if (!cursor.block().next().isVisible()) {
          d->toggleBlockVisible(cursor.block());
          d->moveCursorVisible(false);
        } else if (d->foldBox().contains(e->pos())) {
          cursor.setPosition(document()->findBlockByNumber(d->m_highlightBlocksInfo.open.last()).position());
          const auto c = cursor.block();
          d->toggleBlockVisible(c);
          d->moveCursorVisible(false);
        }
      } else if (d->m_lineNumbersVisible && !inMarkArea) {
        auto selection = cursor;
        selection.setVisualNavigation(true);
        d->extraAreaSelectionAnchorBlockNumber = selection.blockNumber();
        selection.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        selection.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
        setTextCursor(selection);
      } else {
        d->extraAreaToggleMarkBlockNumber = cursor.blockNumber();
        d->m_markDragging = false;
        const auto block = cursor.document()->findBlockByNumber(d->extraAreaToggleMarkBlockNumber);
        if (const auto data = static_cast<TextBlockUserData*>(block.userData())) {
          const auto marks = data->marks();
          for (int i = marks.size(); --i >= 0;) {
            const auto mark = marks.at(i);
            if (mark->isDraggable()) {
              d->m_markDragStart = e->pos();
              d->m_dragMark = mark;
              break;
            }
          }
        }
      }
    }
  } else if (d->extraAreaSelectionAnchorBlockNumber >= 0) {
    auto selection = cursor;
    selection.setVisualNavigation(true);
    if (e->type() == QEvent::MouseMove) {
      const auto anchorBlock = document()->findBlockByNumber(d->extraAreaSelectionAnchorBlockNumber);
      selection.setPosition(anchorBlock.position());
      if (cursor.blockNumber() < d->extraAreaSelectionAnchorBlockNumber) {
        selection.movePosition(QTextCursor::EndOfBlock);
        selection.movePosition(QTextCursor::Right);
      }
      selection.setPosition(cursor.block().position(), QTextCursor::KeepAnchor);
      if (cursor.blockNumber() >= d->extraAreaSelectionAnchorBlockNumber) {
        selection.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        selection.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor);
      }

      if (e->pos().y() >= 0 && e->pos().y() <= d->m_extraArea->height())
        d->autoScrollTimer.stop();
      else if (!d->autoScrollTimer.isActive())
        d->autoScrollTimer.start(100, this);

    } else {
      d->autoScrollTimer.stop();
      d->extraAreaSelectionAnchorBlockNumber = -1;
      return;
    }
    setTextCursor(selection);
  } else if (d->extraAreaToggleMarkBlockNumber >= 0 && d->m_marksVisible && d->m_requestMarkEnabled) {
    if (e->type() == QEvent::MouseButtonRelease && e->button() == Qt::LeftButton) {
      const auto n = d->extraAreaToggleMarkBlockNumber;
      d->extraAreaToggleMarkBlockNumber = -1;
      const auto sameLine = cursor.blockNumber() == n;
      const auto wasDragging = d->m_markDragging;
      const auto dragMark = d->m_dragMark;
      d->m_dragMark = nullptr;
      d->m_markDragging = false;
      d->m_markDragStart = QPoint();
      if (dragMark)
        dragMark->setVisible(true);
      QGuiApplication::restoreOverrideCursor();
      if (wasDragging && dragMark) {
        dragMark->dragToLine(cursor.blockNumber() + 1);
        return;
      }
      if (sameLine) {
        const auto block = cursor.document()->findBlockByNumber(n);
        if (const auto data = static_cast<TextBlockUserData*>(block.userData())) {
          const auto marks = data->marks();
          for (int i = marks.size(); --i >= 0;) {
            const auto mark = marks.at(i);
            if (mark->isClickable()) {
              mark->clicked();
              return;
            }
          }
        }
      }
      const auto line = n + 1;
      TextMarkRequestKind kind;
      if (QApplication::keyboardModifiers() & Qt::ShiftModifier)
        kind = BookmarkRequest;
      else
        kind = BreakpointRequest;

      emit markRequested(this, line, kind);
    }
  }
}

auto TextEditorWidget::ensureCursorVisible() -> void
{
  ensureBlockIsUnfolded(textCursor().block());
  QPlainTextEdit::ensureCursorVisible();
}

auto TextEditorWidget::ensureBlockIsUnfolded(QTextBlock block) -> void
{
  if (!block.isVisible()) {
    const auto documentLayout = qobject_cast<TextDocumentLayout*>(document()->documentLayout());
    QTC_ASSERT(documentLayout, return);

    // Open all parent folds of current line.
    auto indent = TextDocumentLayout::foldingIndent(block);
    block = block.previous();
    while (block.isValid()) {
      const auto indent2 = TextDocumentLayout::foldingIndent(block);
      if (TextDocumentLayout::canFold(block) && indent2 < indent) {
        TextDocumentLayout::doFoldOrUnfold(block, /* unfold = */ true);
        if (block.isVisible())
          break;
        indent = indent2;
      }
      block = block.previous();
    }

    documentLayout->requestUpdate();
    documentLayout->emitDocumentSizeChanged();
  }
}

auto TextEditorWidgetPrivate::toggleBlockVisible(const QTextBlock &block) -> void
{
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(q->document()->documentLayout());
  QTC_ASSERT(documentLayout, return);

  TextDocumentLayout::doFoldOrUnfold(block, TextDocumentLayout::isFolded(block));
  documentLayout->requestUpdate();
  documentLayout->emitDocumentSizeChanged();
}

auto TextEditorWidget::setLanguageSettingsId(Id settingsId) -> void
{
  d->m_tabSettingsId = settingsId;
  setCodeStyle(TextEditorSettings::codeStyle(settingsId));
}

auto TextEditorWidget::languageSettingsId() const -> Id
{
  return d->m_tabSettingsId;
}

auto TextEditorWidget::setCodeStyle(ICodeStylePreferences *preferences) -> void
{
  const auto document = d->m_document.data();
  // Not fully initialized yet... wait for TextEditorWidgetPrivate::setupDocumentSignals
  if (!document)
    return;
  document->indenter()->setCodeStylePreferences(preferences);
  if (d->m_codeStylePreferences) {
    disconnect(d->m_codeStylePreferences, &ICodeStylePreferences::currentTabSettingsChanged, document, &TextDocument::setTabSettings);
    disconnect(d->m_codeStylePreferences, &ICodeStylePreferences::currentValueChanged, this, &TextEditorWidget::slotCodeStyleSettingsChanged);
  }
  d->m_codeStylePreferences = preferences;
  if (d->m_codeStylePreferences) {
    connect(d->m_codeStylePreferences, &ICodeStylePreferences::currentTabSettingsChanged, document, &TextDocument::setTabSettings);
    connect(d->m_codeStylePreferences, &ICodeStylePreferences::currentValueChanged, this, &TextEditorWidget::slotCodeStyleSettingsChanged);
    document->setTabSettings(d->m_codeStylePreferences->currentTabSettings());
    slotCodeStyleSettingsChanged(d->m_codeStylePreferences->currentValue());
  }
}

auto TextEditorWidget::slotCodeStyleSettingsChanged(const QVariant &) -> void {}

auto TextEditorWidget::displaySettings() const -> const DisplaySettings&
{
  return d->m_displaySettings;
}

auto TextEditorWidget::marginSettings() const -> const MarginSettings&
{
  return d->m_marginSettings;
}

auto TextEditorWidget::behaviorSettings() const -> const BehaviorSettings&
{
  return d->m_behaviorSettings;
}

auto TextEditorWidgetPrivate::handleHomeKey(bool anchor, bool block) -> void
{
  const auto mode = anchor ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor;

  auto cursor = q->multiTextCursor();
  for (auto &c : cursor) {
    const auto initpos = c.position();
    auto pos = c.block().position();

    if (!block) {
      // only go to the first non space if we are in the first line of the layout
      if (const auto layout = c.block().layout(); layout->lineForTextPosition(initpos - pos).lineNumber() != 0) {
        c.movePosition(QTextCursor::StartOfLine, mode);
      }
    }

    auto character = q->document()->characterAt(pos);
    const auto tab = QLatin1Char('\t');

    while (character == tab || character.category() == QChar::Separator_Space) {
      ++pos;
      if (pos == initpos)
        break;
      character = q->document()->characterAt(pos);
    }

    // Go to the start of the block when we're already at the start of the text
    if (pos == initpos)
      pos = c.block().position();

    c.setPosition(pos, mode);
  }
  q->setMultiTextCursor(cursor);
}

auto TextEditorWidgetPrivate::handleBackspaceKey() -> void
{
  QTC_ASSERT(!q->multiTextCursor().hasSelection(), return);
  auto cursor = m_cursors;
  cursor.beginEditBlock();
  for (auto &c : cursor) {
    const auto pos = c.position();
    if (!pos)
      continue;

    auto cursorWithinSnippet = false;
    if (m_snippetOverlay->isVisible()) {
      auto snippetCursor = c;
      snippetCursor.movePosition(QTextCursor::Left);
      cursorWithinSnippet = snippetCheckCursor(snippetCursor);
    }

    const auto tabSettings = m_document->tabSettings();
    const auto &typingSettings = m_document->typingSettings();

    if (typingSettings.m_autoIndent && !m_autoCompleteHighlightPos.isEmpty() && m_autoCompleteHighlightPos.last() == c && m_removeAutoCompletedText && m_autoCompleter->autoBackspace(c)) {
      continue;
    }

    auto handled = false;
    if (typingSettings.m_smartBackspaceBehavior == TypingSettings::BackspaceNeverIndents) {
      if (cursorWithinSnippet)
        c.beginEditBlock();
      c.deletePreviousChar();
      handled = true;
    } else if (typingSettings.m_smartBackspaceBehavior == TypingSettings::BackspaceFollowsPreviousIndents) {
      auto currentBlock = c.block();
      const auto positionInBlock = pos - currentBlock.position();
      const auto blockText = currentBlock.text();
      if (c.atBlockStart() || TabSettings::firstNonSpace(blockText) < positionInBlock) {
        if (cursorWithinSnippet)
          c.beginEditBlock();
        c.deletePreviousChar();
        handled = true;
      } else {
        if (cursorWithinSnippet)
          m_snippetOverlay->accept();
        cursorWithinSnippet = false;
        auto previousIndent = 0;
        const auto indent = tabSettings.columnAt(blockText, positionInBlock);
        for (auto previousNonEmptyBlock = currentBlock.previous(); previousNonEmptyBlock.isValid(); previousNonEmptyBlock = previousNonEmptyBlock.previous()) {
          auto previousNonEmptyBlockText = previousNonEmptyBlock.text();
          if (previousNonEmptyBlockText.trimmed().isEmpty())
            continue;
          previousIndent = tabSettings.columnAt(previousNonEmptyBlockText, TabSettings::firstNonSpace(previousNonEmptyBlockText));
          if (previousIndent < indent) {
            c.beginEditBlock();
            c.setPosition(currentBlock.position(), QTextCursor::KeepAnchor);
            c.insertText(tabSettings.indentationString(previousNonEmptyBlockText));
            c.endEditBlock();
            handled = true;
            break;
          }
        }
      }
    } else if (typingSettings.m_smartBackspaceBehavior == TypingSettings::BackspaceUnindents) {
      const auto previousChar = q->document()->characterAt(pos - 1);
      if (!(previousChar == QLatin1Char(' ') || previousChar == QLatin1Char('\t'))) {
        if (cursorWithinSnippet)
          c.beginEditBlock();
        c.deletePreviousChar();
      } else {
        if (cursorWithinSnippet)
          m_snippetOverlay->accept();
        cursorWithinSnippet = false;
        q->unindent();
      }
      handled = true;
    }

    if (!handled) {
      if (cursorWithinSnippet)
        c.beginEditBlock();
      c.deletePreviousChar();
    }

    if (cursorWithinSnippet) {
      c.endEditBlock();
      m_snippetOverlay->updateEquivalentSelections(c);
    }
  }
  cursor.endEditBlock();
  q->setMultiTextCursor(cursor);
}

auto TextEditorWidget::wheelEvent(QWheelEvent *e) -> void
{
  d->clearVisibleFoldedBlock();
  if (e->modifiers() & Qt::ControlModifier) {
    if (!scrollWheelZoomingEnabled()) {
      // When the setting is disabled globally,
      // we have to skip calling QPlainTextEdit::wheelEvent()
      // that changes zoom in it.
      return;
    }

    const auto deltaY = e->angleDelta().y();
    if (deltaY != 0)
      zoomF(deltaY / 120.f);
    return;
  }
  QPlainTextEdit::wheelEvent(e);
}

static auto showZoomIndicator(QWidget *editor, const int newZoom) -> void
{
  showText(editor, QCoreApplication::translate("TextEditor::TextEditorWidget", "Zoom: %1%").arg(newZoom), FadingIndicator::SmallText);
}

auto TextEditorWidget::zoomF(float delta) -> void
{
  d->clearVisibleFoldedBlock();
  auto step = 10.f * delta;
  // Ensure we always zoom a minimal step in-case the resolution is more than 16x
  if (step > 0 && step < 1)
    step = 1;
  else if (step < 0 && step > -1)
    step = -1;

  const auto newZoom = TextEditorSettings::increaseFontZoom(int(step));
  showZoomIndicator(this, newZoom);
}

auto TextEditorWidget::zoomReset() -> void
{
  TextEditorSettings::resetFontZoom();
  showZoomIndicator(this, 100);
}

auto TextEditorWidget::findLinkAt(const QTextCursor &cursor, ProcessLinkCallback &&callback, bool resolveTarget, bool inNextSplit) -> void
{
  emit requestLinkAt(cursor, callback, resolveTarget, inNextSplit);
}

auto TextEditorWidget::openLink(const Link &link, bool inNextSplit) -> bool
{
  #ifdef WITH_TESTS
    struct Signaller { ~Signaller() { emit EditorManager::instance()->linkOpened(); } } s;
  #endif

  if (!link.hasValidTarget())
    return false;

  if (!inNextSplit && textDocument()->filePath() == link.targetFilePath) {
    EditorManager::addCurrentPositionToNavigationHistory();
    gotoLine(link.targetLine, link.targetColumn, true, true);
    setFocus();
    return true;
  }
  EditorManager::OpenEditorFlags flags;
  if (inNextSplit)
    flags |= EditorManager::OpenInOtherSplit;

  return EditorManager::openEditorAt(link, Id(), flags);
}

auto TextEditorWidgetPrivate::isMouseNavigationEvent(QMouseEvent *e) const -> bool
{
  return q->mouseNavigationEnabled() && e->modifiers() & Qt::ControlModifier && !(e->modifiers() & Qt::ShiftModifier);
}

auto TextEditorWidgetPrivate::requestUpdateLink(QMouseEvent *e) -> void
{
  if (!isMouseNavigationEvent(e))
    return;
  // Link emulation behaviour for 'go to definition'
  const auto cursor = q->cursorForPosition(e->pos());

  // Avoid updating the link we already found
  if (cursor.position() >= m_currentLink.linkTextStart && cursor.position() <= m_currentLink.linkTextEnd)
    return;

  // Check that the mouse was actually on the text somewhere
  auto onText = q->cursorRect(cursor).right() >= e->x();
  if (!onText) {
    auto nextPos = cursor;
    nextPos.movePosition(QTextCursor::Right);
    onText = q->cursorRect(nextPos).right() >= e->x();
  }

  if (onText) {
    m_pendingLinkUpdate = cursor;
    QMetaObject::invokeMethod(this, &TextEditorWidgetPrivate::updateLink, Qt::QueuedConnection);
    return;
  }

  clearLink();
}

auto TextEditorWidgetPrivate::updateLink() -> void
{
  if (m_pendingLinkUpdate.isNull())
    return;
  if (m_pendingLinkUpdate == m_lastLinkUpdate)
    return;

  m_lastLinkUpdate = m_pendingLinkUpdate;
  q->findLinkAt(m_pendingLinkUpdate, [parent = QPointer(q), this](const Link &link) {
    if (!parent)
      return;

    if (link.hasValidLinkText())
      showLink(link);
    else
      clearLink();
  }, false);
}

auto TextEditorWidgetPrivate::showLink(const Link &link) -> void
{
  if (m_currentLink == link)
    return;

  QTextEdit::ExtraSelection sel;
  sel.cursor = q->textCursor();
  sel.cursor.setPosition(link.linkTextStart);
  sel.cursor.setPosition(link.linkTextEnd, QTextCursor::KeepAnchor);
  sel.format = m_document->fontSettings().toTextCharFormat(C_LINK);
  sel.format.setFontUnderline(true);
  q->setExtraSelections(TextEditorWidget::OtherSelection, QList<QTextEdit::ExtraSelection>() << sel);
  q->viewport()->setCursor(Qt::PointingHandCursor);
  m_currentLink = link;
}

auto TextEditorWidgetPrivate::clearLink() -> void
{
  m_pendingLinkUpdate = QTextCursor();
  m_lastLinkUpdate = QTextCursor();
  if (!m_currentLink.hasValidLinkText())
    return;

  q->setExtraSelections(TextEditorWidget::OtherSelection, QList<QTextEdit::ExtraSelection>());
  q->viewport()->setCursor(Qt::IBeamCursor);
  m_currentLink = Link();
}

auto TextEditorWidgetPrivate::highlightSearchResultsSlot(const QString &txt, FindFlags findFlags) -> void
{
  const QString pattern = findFlags & FindRegularExpression ? txt : QRegularExpression::escape(txt);
  const QRegularExpression::PatternOptions options = findFlags & FindCaseSensitively ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption;
  if (m_searchExpr.pattern() == pattern && m_searchExpr.patternOptions() == options)
    return;
  m_searchExpr.setPattern(pattern);
  m_searchExpr.setPatternOptions(options);
  m_findText = txt;
  m_findFlags = findFlags;

  m_delayedUpdateTimer.start(50);

  if (m_highlightScrollBarController)
    m_scrollBarUpdateTimer.start(50);
}

auto TextEditorWidgetPrivate::searchResultsReady(int beginIndex, int endIndex) -> void
{
  QVector<SearchResult> results;
  for (auto index = beginIndex; index < endIndex; ++index) {
    foreach(Utils::FileSearchResult result, m_searchWatcher->resultAt(index)) {
      const auto &block = q->document()->findBlockByNumber(result.lineNumber - 1);
      const auto matchStart = block.position() + result.matchStart;
      QTextCursor cursor(block);
      cursor.setPosition(matchStart);
      cursor.setPosition(matchStart + result.matchLength, QTextCursor::KeepAnchor);
      if (!q->inFindScope(cursor))
        continue;
      results << SearchResult{matchStart, result.matchLength};
    }
  }
  m_searchResults << results;
  addSearchResultsToScrollBar(results);
}

auto TextEditorWidgetPrivate::searchFinished() -> void
{
  delete m_searchWatcher;
  m_searchWatcher = nullptr;
}

auto TextEditorWidgetPrivate::adjustScrollBarRanges() -> void
{
  if (!m_highlightScrollBarController)
    return;
  const auto lineSpacing = QFontMetricsF(q->font()).lineSpacing();
  if (lineSpacing == 0)
    return;

  m_highlightScrollBarController->setLineHeight(lineSpacing);
  m_highlightScrollBarController->setVisibleRange(q->viewport()->rect().height());
  m_highlightScrollBarController->setMargin(q->textDocument()->document()->documentMargin());
}

auto TextEditorWidgetPrivate::highlightSearchResultsInScrollBar() -> void
{
  if (!m_highlightScrollBarController)
    return;
  m_highlightScrollBarController->removeHighlights(Constants::SCROLL_BAR_SEARCH_RESULT);
  m_searchResults.clear();

  if (m_searchWatcher) {
    m_searchWatcher->disconnect();
    m_searchWatcher->cancel();
    m_searchWatcher->deleteLater();
    m_searchWatcher = nullptr;
  }

  const auto &txt = m_findText;
  if (txt.isEmpty())
    return;

  adjustScrollBarRanges();

  m_searchWatcher = new QFutureWatcher<FileSearchResultList>();
  connect(m_searchWatcher, &QFutureWatcher<FileSearchResultList>::resultsReadyAt, this, &TextEditorWidgetPrivate::searchResultsReady);
  connect(m_searchWatcher, &QFutureWatcher<FileSearchResultList>::finished, this, &TextEditorWidgetPrivate::searchFinished);
  m_searchWatcher->setPendingResultsLimit(10);

  const QTextDocument::FindFlags findFlags = textDocumentFlagsForFindFlags(m_findFlags);

  const QString &fileName = m_document->filePath().toString();
  const auto it = new FileListIterator({fileName}, {const_cast<QTextCodec*>(m_document->codec())});
  QMap<QString, QString> fileToContentsMap;
  fileToContentsMap[fileName] = m_document->plainText();

  if (m_findFlags & FindRegularExpression)
    m_searchWatcher->setFuture(findInFilesRegExp(txt, it, findFlags, fileToContentsMap));
  else
    m_searchWatcher->setFuture(findInFiles(txt, it, findFlags, fileToContentsMap));
}

auto TextEditorWidgetPrivate::scheduleUpdateHighlightScrollBar() -> void
{
  if (m_scrollBarUpdateScheduled)
    return;

  m_scrollBarUpdateScheduled = true;
  QMetaObject::invokeMethod(this, &TextEditorWidgetPrivate::updateHighlightScrollBarNow, Qt::QueuedConnection);
}

auto textMarkPrioToScrollBarPrio(const TextMark::Priority &prio) -> Highlight::Priority
{
  switch (prio) {
  case TextMark::LowPriority:
    return Highlight::LowPriority;
  case TextMark::NormalPriority:
    return Highlight::NormalPriority;
  case TextMark::HighPriority:
    return Highlight::HighPriority;
  default:
    return Highlight::NormalPriority;
  }
}

auto TextEditorWidgetPrivate::addSearchResultsToScrollBar(QVector<SearchResult> results) -> void
{
  if (!m_highlightScrollBarController)
    return;
  foreach(SearchResult result, results) {
    const auto &block = q->document()->findBlock(result.start);
    if (block.isValid() && block.isVisible()) {
      const auto firstLine = block.layout()->lineForTextPosition(result.start - block.position()).lineNumber();
      const auto lastLine = block.layout()->lineForTextPosition(result.start - block.position() + result.length).lineNumber();
      for (auto line = firstLine; line <= lastLine; ++line) {
        m_highlightScrollBarController->addHighlight({Constants::SCROLL_BAR_SEARCH_RESULT, block.firstLineNumber() + line, Theme::TextEditor_SearchResult_ScrollBarColor, Highlight::HighPriority});
      }
    }
  }
}

auto markToHighlight(TextMark *mark, int lineNumber) -> Highlight
{
  return Highlight(mark->category(), lineNumber, mark->color().value_or(Theme::TextColorNormal), textMarkPrioToScrollBarPrio(mark->priority()));
}

auto TextEditorWidgetPrivate::updateHighlightScrollBarNow() -> void
{
  m_scrollBarUpdateScheduled = false;
  if (!m_highlightScrollBarController)
    return;

  m_highlightScrollBarController->removeAllHighlights();

  updateCurrentLineInScrollbar();

  // update search results
  addSearchResultsToScrollBar(m_searchResults);

  // update text marks
  const auto marks = m_document->marks();
  for (auto mark : marks) {
    if (!mark->isVisible() || !mark->color().has_value())
      continue;
    const auto &block = q->document()->findBlockByNumber(mark->lineNumber() - 1);
    if (block.isVisible())
      m_highlightScrollBarController->addHighlight(markToHighlight(mark, block.firstLineNumber()));
  }
}

auto TextEditorWidget::multiTextCursor() const -> MultiTextCursor
{
  return d->m_cursors;
}

auto TextEditorWidget::setMultiTextCursor(const MultiTextCursor &cursor) -> void
{
  const auto oldCursor = d->m_cursors;
  const_cast<MultiTextCursor&>(d->m_cursors) = cursor;
  if (oldCursor == d->m_cursors)
    return;
  doSetTextCursor(d->m_cursors.mainCursor(), /*keepMultiSelection*/ true);
  auto updateRect = d->cursorUpdateRect(oldCursor);
  if (d->m_highlightCurrentLine)
    updateRect = QRect(0, updateRect.y(), viewport()->rect().width(), updateRect.height());
  updateRect |= d->cursorUpdateRect(d->m_cursors);
  viewport()->update(updateRect);
  emit cursorPositionChanged();
}

auto TextEditorWidget::translatedLineRegion(int lineStart, int lineEnd) const -> QRegion
{
  QRegion region;
  for (auto i = lineStart; i <= lineEnd; i++) {
    auto block = document()->findBlockByNumber(i);
    auto topLeft = blockBoundingGeometry(block).translated(contentOffset()).topLeft().toPoint();

    if (block.isValid()) {
      const auto layout = block.layout();

      for (auto i = 0; i < layout->lineCount(); i++) {
        auto line = layout->lineAt(i);
        region += line.naturalTextRect().translated(topLeft).toRect();
      }
    }
  }
  return region;
}

auto TextEditorWidgetPrivate::setFindScope(const MultiTextCursor &scope) -> void
{
  if (m_findScope != scope) {
    m_findScope = scope;
    q->viewport()->update();
    highlightSearchResultsInScrollBar();
  }
}

auto TextEditorWidgetPrivate::_q_animateUpdate(const QTextCursor &cursor, QPointF lastPos, QRectF rect) -> void
{
  q->viewport()->update(QRectF(q->cursorRect(cursor).topLeft() + rect.topLeft(), rect.size()).toAlignedRect());
  if (!lastPos.isNull())
    q->viewport()->update(QRectF(lastPos + rect.topLeft(), rect.size()).toAlignedRect());
}

TextEditorAnimator::TextEditorAnimator(QObject *parent) : QObject(parent), m_timeline(256)
{
  m_value = 0;
  m_timeline.setEasingCurve(QEasingCurve::SineCurve);
  connect(&m_timeline, &QTimeLine::valueChanged, this, &TextEditorAnimator::step);
  connect(&m_timeline, &QTimeLine::finished, this, &QObject::deleteLater);
  m_timeline.start();
}

auto TextEditorAnimator::init(const QTextCursor &cursor, const QFont &f, const QPalette &pal) -> void
{
  m_cursor = cursor;
  m_font = f;
  m_palette = pal;
  m_text = cursor.selectedText();
  const QFontMetrics fm(m_font);
  m_size = QSizeF(fm.horizontalAdvance(m_text), fm.height());
}

auto TextEditorAnimator::draw(QPainter *p, const QPointF &pos) -> void
{
  m_lastDrawPos = pos;
  p->setPen(m_palette.text().color());
  auto f = m_font;
  f.setPointSizeF(f.pointSizeF() * (1.0 + m_value / 2));
  const QFontMetrics fm(f);
  const auto width = fm.horizontalAdvance(m_text);
  QRectF r((m_size.width() - width) / 2, (m_size.height() - fm.height()) / 2, width, fm.height());
  r.translate(pos);
  p->fillRect(r, m_palette.base());
  p->setFont(f);
  p->drawText(r, m_text);
}

auto TextEditorAnimator::isRunning() const -> bool
{
  return m_timeline.state() == QTimeLine::Running;
}

auto TextEditorAnimator::rect() const -> QRectF
{
  auto f = m_font;
  f.setPointSizeF(f.pointSizeF() * (1.0 + m_value / 2));
  const QFontMetrics fm(f);
  const auto width = fm.horizontalAdvance(m_text);
  return QRectF((m_size.width() - width) / 2, (m_size.height() - fm.height()) / 2, width, fm.height());
}

auto TextEditorAnimator::step(qreal v) -> void
{
  const auto before = rect();
  m_value = v;
  const auto after = rect();
  emit updateRequest(m_cursor, m_lastDrawPos, before.united(after));
}

auto TextEditorAnimator::finish() -> void
{
  m_timeline.stop();
  step(0);
  deleteLater();
}

auto TextEditorWidgetPrivate::_q_matchParentheses() -> void
{
  if (q->isReadOnly() || !(m_displaySettings.m_highlightMatchingParentheses || m_displaySettings.m_animateMatchingParentheses))
    return;

  auto backwardMatch = q->textCursor();
  auto forwardMatch = q->textCursor();
  if (q->overwriteMode())
    backwardMatch.movePosition(QTextCursor::Right);
  const auto backwardMatchType = TextBlockUserData::matchCursorBackward(&backwardMatch);
  const auto forwardMatchType = TextBlockUserData::matchCursorForward(&forwardMatch);

  QList<QTextEdit::ExtraSelection> extraSelections;

  if (backwardMatchType == TextBlockUserData::NoMatch && forwardMatchType == TextBlockUserData::NoMatch) {
    q->setExtraSelections(TextEditorWidget::ParenthesesMatchingSelection, extraSelections); // clear
    return;
  }

  const auto matchFormat = m_document->fontSettings().toTextCharFormat(C_PARENTHESES);
  const auto mismatchFormat = m_document->fontSettings().toTextCharFormat(C_PARENTHESES_MISMATCH);
  auto animatePosition = -1;
  if (backwardMatch.hasSelection()) {
    QTextEdit::ExtraSelection sel;
    if (backwardMatchType == TextBlockUserData::Mismatch) {
      sel.cursor = backwardMatch;
      sel.format = mismatchFormat;
      extraSelections.append(sel);
    } else {

      sel.cursor = backwardMatch;
      sel.format = matchFormat;

      sel.cursor.setPosition(backwardMatch.selectionStart());
      sel.cursor.setPosition(sel.cursor.position() + 1, QTextCursor::KeepAnchor);
      extraSelections.append(sel);

      if (m_displaySettings.m_animateMatchingParentheses && sel.cursor.block().isVisible())
        animatePosition = backwardMatch.selectionStart();

      sel.cursor.setPosition(backwardMatch.selectionEnd());
      sel.cursor.setPosition(sel.cursor.position() - 1, QTextCursor::KeepAnchor);
      extraSelections.append(sel);
    }
  }

  if (forwardMatch.hasSelection()) {
    QTextEdit::ExtraSelection sel;
    if (forwardMatchType == TextBlockUserData::Mismatch) {
      sel.cursor = forwardMatch;
      sel.format = mismatchFormat;
      extraSelections.append(sel);
    } else {

      sel.cursor = forwardMatch;
      sel.format = matchFormat;

      sel.cursor.setPosition(forwardMatch.selectionStart());
      sel.cursor.setPosition(sel.cursor.position() + 1, QTextCursor::KeepAnchor);
      extraSelections.append(sel);

      sel.cursor.setPosition(forwardMatch.selectionEnd());
      sel.cursor.setPosition(sel.cursor.position() - 1, QTextCursor::KeepAnchor);
      extraSelections.append(sel);

      if (m_displaySettings.m_animateMatchingParentheses && sel.cursor.block().isVisible())
        animatePosition = forwardMatch.selectionEnd() - 1;
    }
  }

  if (animatePosition >= 0) {
    foreach(const QTextEdit::ExtraSelection &sel, q->extraSelections(TextEditorWidget::ParenthesesMatchingSelection)) {
      if (sel.cursor.selectionStart() == animatePosition || sel.cursor.selectionEnd() - 1 == animatePosition) {
        animatePosition = -1;
        break;
      }
    }
  }

  if (animatePosition >= 0) {
    cancelCurrentAnimations(); // one animation is enough
    QPalette pal;
    pal.setBrush(QPalette::Text, matchFormat.foreground());
    pal.setBrush(QPalette::Base, matchFormat.background());
    auto cursor = q->textCursor();
    cursor.setPosition(animatePosition + 1);
    cursor.setPosition(animatePosition, QTextCursor::KeepAnchor);
    m_bracketsAnimator = new TextEditorAnimator(this);
    m_bracketsAnimator->init(cursor, q->font(), pal);
    connect(m_bracketsAnimator.data(), &TextEditorAnimator::updateRequest, this, &TextEditorWidgetPrivate::_q_animateUpdate);
  }
  if (m_displaySettings.m_highlightMatchingParentheses)
    q->setExtraSelections(TextEditorWidget::ParenthesesMatchingSelection, extraSelections);
}

auto TextEditorWidgetPrivate::_q_highlightBlocks() -> void
{
  TextEditorPrivateHighlightBlocks highlightBlocksInfo;

  QTextBlock block;
  if (extraAreaHighlightFoldedBlockNumber >= 0) {
    block = q->document()->findBlockByNumber(extraAreaHighlightFoldedBlockNumber);
    if (block.isValid() && block.next().isValid() && TextDocumentLayout::foldingIndent(block.next()) > TextDocumentLayout::foldingIndent(block))
      block = block.next();
  }

  auto closeBlock = block;
  while (block.isValid()) {
    const auto foldingIndent = TextDocumentLayout::foldingIndent(block);

    while (block.previous().isValid() && TextDocumentLayout::foldingIndent(block) >= foldingIndent)
      block = block.previous();
    const auto nextIndent = TextDocumentLayout::foldingIndent(block);
    if (nextIndent == foldingIndent)
      break;
    highlightBlocksInfo.open.prepend(block.blockNumber());
    while (closeBlock.next().isValid() && TextDocumentLayout::foldingIndent(closeBlock.next()) >= foldingIndent)
      closeBlock = closeBlock.next();
    highlightBlocksInfo.close.append(closeBlock.blockNumber());
    const auto indent = qMin(visualIndent(block), visualIndent(closeBlock));
    highlightBlocksInfo.visualIndent.prepend(indent);
  }

  #if 0
    if (block.isValid()) {
        QTextCursor cursor(block);
        if (extraAreaHighlightCollapseColumn >= 0)
            cursor.setPosition(cursor.position() + qMin(extraAreaHighlightCollapseColumn,
                                                        block.length()-1));
        QTextCursor closeCursor;
        bool firstRun = true;
        while (TextBlockUserData::findPreviousBlockOpenParenthesis(&cursor, firstRun)) {
            firstRun = false;
            highlightBlocksInfo.open.prepend(cursor.blockNumber());
            int visualIndent = visualIndent(cursor.block());
            if (closeCursor.isNull())
                closeCursor = cursor;
            if (TextBlockUserData::findNextBlockClosingParenthesis(&closeCursor)) {
                highlightBlocksInfo.close.append(closeCursor.blockNumber());
                visualIndent = qMin(visualIndent, visualIndent(closeCursor.block()));
            }
            highlightBlocksInfo.visualIndent.prepend(visualIndent);
        }
    }
  #endif
  if (m_highlightBlocksInfo != highlightBlocksInfo) {
    m_highlightBlocksInfo = highlightBlocksInfo;
    q->viewport()->update();
    m_extraArea->update();
  }
}

auto TextEditorWidgetPrivate::autocompleterHighlight(const QTextCursor &cursor) -> void
{
  if (!m_animateAutoComplete && !m_highlightAutoComplete || q->isReadOnly() || !cursor.hasSelection()) {
    m_autoCompleteHighlightPos.clear();
  } else if (m_highlightAutoComplete) {
    m_autoCompleteHighlightPos.push_back(cursor);
  }
  if (m_animateAutoComplete) {
    const auto matchFormat = m_document->fontSettings().toTextCharFormat(C_AUTOCOMPLETE);
    cancelCurrentAnimations(); // one animation is enough
    QPalette pal;
    pal.setBrush(QPalette::Text, matchFormat.foreground());
    pal.setBrush(QPalette::Base, matchFormat.background());
    m_autocompleteAnimator = new TextEditorAnimator(this);
    m_autocompleteAnimator->init(cursor, q->font(), pal);
    connect(m_autocompleteAnimator.data(), &TextEditorAnimator::updateRequest, this, &TextEditorWidgetPrivate::_q_animateUpdate);
  }
  updateAutoCompleteHighlight();
}

auto TextEditorWidgetPrivate::updateAnimator(QPointer<TextEditorAnimator> animator, QPainter &painter) -> void
{
  if (animator && animator->isRunning())
    animator->draw(&painter, q->cursorRect(animator->cursor()).topLeft());
}

auto TextEditorWidgetPrivate::cancelCurrentAnimations() -> void
{
  if (m_autocompleteAnimator)
    m_autocompleteAnimator->finish();
  if (m_bracketsAnimator)
    m_bracketsAnimator->finish();
}

auto TextEditorWidget::changeEvent(QEvent *e) -> void
{
  QPlainTextEdit::changeEvent(e);
  if (e->type() == QEvent::ApplicationFontChange || e->type() == QEvent::FontChange) {
    if (d->m_extraArea) {
      auto f = d->m_extraArea->font();
      f.setPointSizeF(font().pointSizeF());
      d->m_extraArea->setFont(f);
      d->slotUpdateExtraAreaWidth();
      d->m_extraArea->update();
    }
  } else if (e->type() == QEvent::PaletteChange) {
    applyFontSettings();
  }
}

auto TextEditorWidget::focusInEvent(QFocusEvent *e) -> void
{
  QPlainTextEdit::focusInEvent(e);
  d->startCursorFlashTimer();
  d->updateHighlights();
}

auto TextEditorWidget::focusOutEvent(QFocusEvent *e) -> void
{
  QPlainTextEdit::focusOutEvent(e);
  if (viewport()->cursor().shape() == Qt::BlankCursor)
    viewport()->setCursor(Qt::IBeamCursor);
  d->m_cursorFlashTimer.stop();
  if (d->m_cursorVisible) {
    d->m_cursorVisible = false;
    viewport()->update(d->cursorUpdateRect(d->m_cursors));
  }
  d->updateHighlights();
}

auto TextEditorWidgetPrivate::maybeSelectLine() -> void
{
  auto cursor = m_cursors;
  if (cursor.hasSelection())
    return;
  for (auto &c : cursor) {
    const auto &block = m_document->document()->findBlock(c.selectionStart());
    const auto &end = m_document->document()->findBlock(c.selectionEnd()).next();
    c.setPosition(block.position());
    if (!end.isValid()) {
      c.movePosition(QTextCursor::PreviousCharacter);
      c.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    } else {
      c.setPosition(end.position(), QTextCursor::KeepAnchor);
    }
  }
  cursor.mergeCursors();
  q->setMultiTextCursor(cursor);
}

// shift+del
auto TextEditorWidget::cutLine() -> void
{
  d->maybeSelectLine();
  cut();
}

// ctrl+ins
auto TextEditorWidget::copyLine() -> void
{
  d->maybeSelectLine();
  copy();
}

auto TextEditorWidgetPrivate::duplicateSelection(bool comment) -> void
{
  if (comment && !m_commentDefinition.hasMultiLineStyle())
    return;

  auto cursor = q->multiTextCursor();
  cursor.beginEditBlock();
  for (auto &c : cursor) {
    if (c.hasSelection()) {
      // Cannot "duplicate and comment" files without multi-line comment

      auto dupText = c.selectedText().replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
      if (comment) {
        dupText = m_commentDefinition.multiLineStart + dupText + m_commentDefinition.multiLineEnd;
      }
      const auto selStart = c.selectionStart();
      const auto selEnd = c.selectionEnd();
      const auto cursorAtStart = c.position() == selStart;
      c.setPosition(selEnd);
      c.insertText(dupText);
      c.setPosition(cursorAtStart ? selEnd : selStart);
      c.setPosition(cursorAtStart ? selStart : selEnd, QTextCursor::KeepAnchor);
    } else if (!m_cursors.hasMultipleCursors()) {
      const auto curPos = c.position();
      const auto &block = c.block();
      QString dupText = block.text() + QLatin1Char('\n');
      if (comment && m_commentDefinition.hasSingleLineStyle())
        dupText.append(m_commentDefinition.singleLine);
      c.setPosition(block.position());
      c.insertText(dupText);
      c.setPosition(curPos);
    }
  }
  cursor.endEditBlock();
  q->setMultiTextCursor(cursor);
}

auto TextEditorWidget::duplicateSelection() -> void
{
  d->duplicateSelection(false);
}

auto TextEditorWidget::duplicateSelectionAndComment() -> void
{
  d->duplicateSelection(true);
}

auto TextEditorWidget::deleteLine() -> void
{
  d->maybeSelectLine();
  textCursor().removeSelectedText();
}

auto TextEditorWidget::deleteEndOfLine() -> void
{
  d->moveCursor(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  auto cursor = multiTextCursor();
  cursor.removeSelectedText();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::deleteEndOfWord() -> void
{
  d->moveCursor(QTextCursor::NextWord, QTextCursor::KeepAnchor);
  auto cursor = multiTextCursor();
  cursor.removeSelectedText();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::deleteEndOfWordCamelCase() -> void
{
  auto cursor = multiTextCursor();
  CamelCaseCursor::right(&cursor, this, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::deleteStartOfLine() -> void
{
  d->moveCursor(QTextCursor::StartOfLine, QTextCursor::KeepAnchor);
  auto cursor = multiTextCursor();
  cursor.removeSelectedText();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::deleteStartOfWord() -> void
{
  d->moveCursor(QTextCursor::PreviousWord, QTextCursor::KeepAnchor);
  auto cursor = multiTextCursor();
  cursor.removeSelectedText();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::deleteStartOfWordCamelCase() -> void
{
  auto cursor = multiTextCursor();
  CamelCaseCursor::left(&cursor, this, QTextCursor::KeepAnchor);
  cursor.removeSelectedText();
  setMultiTextCursor(cursor);
}

auto TextEditorWidgetPrivate::setExtraSelections(Id kind, const QList<QTextEdit::ExtraSelection> &selections) -> void
{
  if (selections.isEmpty() && m_extraSelections[kind].isEmpty())
    return;
  m_extraSelections[kind] = selections;

  if (kind == TextEditorWidget::CodeSemanticsSelection) {
    m_overlay->clear();
    foreach(const QTextEdit::ExtraSelection &selection, m_extraSelections[kind]) {
      m_overlay->addOverlaySelection(selection.cursor, selection.format.background().color(), selection.format.background().color(), TextEditorOverlay::LockSize);
    }
    m_overlay->setVisible(!m_overlay->isEmpty());
  } else {
    QList<QTextEdit::ExtraSelection> all;
    for (auto i = m_extraSelections.constBegin(); i != m_extraSelections.constEnd(); ++i) {
      if (i.key() == TextEditorWidget::CodeSemanticsSelection || i.key() == TextEditorWidget::SnippetPlaceholderSelection)
        continue;
      all += i.value();
    }
    q->QPlainTextEdit::setExtraSelections(all);
  }
}

auto TextEditorWidget::setExtraSelections(Id kind, const QList<QTextEdit::ExtraSelection> &selections) -> void
{
  d->setExtraSelections(kind, selections);
}

auto TextEditorWidget::extraSelections(Id kind) const -> QList<QTextEdit::ExtraSelection>
{
  return d->m_extraSelections.value(kind);
}

auto TextEditorWidget::extraSelectionTooltip(int pos) const -> QString
{
  foreach(const QList<QTextEdit::ExtraSelection> &sel, d->m_extraSelections) {
    for (const auto &s : sel) {
      if (s.cursor.selectionStart() <= pos && s.cursor.selectionEnd() >= pos && !s.format.toolTip().isEmpty())
        return s.format.toolTip();
    }
  }
  return QString();
}

auto TextEditorWidget::autoIndent() -> void
{
  auto cursor = multiTextCursor();
  cursor.beginEditBlock();
  // The order is important, since some indenter refer to previous indent positions.
  auto cursors = cursor.cursors();
  sort(cursors, [](const QTextCursor &lhs, const QTextCursor &rhs) {
    return lhs.selectionStart() < rhs.selectionStart();
  });
  for (const auto &c : cursors)
    d->m_document->autoFormatOrIndent(c);
  cursor.mergeCursors();
  cursor.endEditBlock();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::rewrapParagraph() -> void
{
  const auto paragraphWidth = marginSettings().m_marginColumn;
  const QRegularExpression anyLettersOrNumbers("\\w");
  const auto tabSize = d->m_document->tabSettings().m_tabSize;

  auto cursor = textCursor();
  cursor.beginEditBlock();

  // Find start of paragraph.

  while (cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::MoveAnchor)) {
    auto block = cursor.block();
    auto text = block.text();

    // If this block is empty, move marker back to previous and terminate.
    if (!text.contains(anyLettersOrNumbers)) {
      cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor);
      break;
    }
  }

  cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::MoveAnchor);

  // Find indent level of current block.

  auto indentLevel = 0;
  const auto text = cursor.block().text();

  for (const auto &ch : text) {
    if (ch == QLatin1Char(' '))
      indentLevel++;
    else if (ch == QLatin1Char('\t'))
      indentLevel += tabSize - indentLevel % tabSize;
    else
      break;
  }

  // If there is a common prefix, it should be kept and expanded to all lines.
  // this allows nice reflowing of doxygen style comments.
  auto nextBlock = cursor;
  QString commonPrefix;

  if (nextBlock.movePosition(QTextCursor::NextBlock)) {
    auto nText = nextBlock.block().text();
    const int maxLength = qMin(text.length(), nText.length());

    for (auto i = 0; i < maxLength; ++i) {
      const auto ch = text.at(i);

      if (ch != nText[i] || ch.isLetterOrNumber())
        break;
      commonPrefix.append(ch);
    }
  }

  // Find end of paragraph.
  while (cursor.movePosition(QTextCursor::NextBlock, QTextCursor::KeepAnchor)) {
    auto text = cursor.block().text();

    if (!text.contains(anyLettersOrNumbers))
      break;
  }

  auto selectedText = cursor.selectedText();

  // Preserve initial indent level.or common prefix.
  QString spacing;

  if (commonPrefix.isEmpty()) {
    spacing = d->m_document->tabSettings().indentationString(0, indentLevel, 0, textCursor().block());
  } else {
    spacing = commonPrefix;
    indentLevel = commonPrefix.length();
  }

  auto currentLength = indentLevel;
  QString result;
  result.append(spacing);

  // Remove existing instances of any common prefix from paragraph to
  // reflow.
  selectedText.remove(0, commonPrefix.length());
  commonPrefix.prepend(QChar::ParagraphSeparator);
  selectedText.replace(commonPrefix, QLatin1String("\n"));

  // remove any repeated spaces, trim lines to PARAGRAPH_WIDTH width and
  // keep the same indentation level as first line in paragraph.
  QString currentWord;

  for (const auto &ch : qAsConst(selectedText)) {
    if (ch.isSpace() && ch != QChar::Nbsp) {
      if (!currentWord.isEmpty()) {
        currentLength += currentWord.length() + 1;

        if (currentLength > paragraphWidth) {
          currentLength = currentWord.length() + 1 + indentLevel;
          result.chop(1); // remove trailing space
          result.append(QChar::ParagraphSeparator);
          result.append(spacing);
        }

        result.append(currentWord);
        result.append(QLatin1Char(' '));
        currentWord.clear();
      }

      continue;
    }

    currentWord.append(ch);
  }
  result.chop(1);
  result.append(QChar::ParagraphSeparator);

  cursor.insertText(result);
  cursor.endEditBlock();
}

auto TextEditorWidget::unCommentSelection() -> void
{
  const auto singleLine = d->m_document->typingSettings().m_preferSingleLineComments;
  const auto cursor = Utils::unCommentSelection(multiTextCursor(), d->m_commentDefinition, singleLine);
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::autoFormat() -> void
{
  auto cursor = textCursor();
  cursor.beginEditBlock();
  d->m_document->autoFormat(cursor);
  cursor.endEditBlock();
}

auto TextEditorWidget::encourageApply() -> void
{
  if (!d->m_snippetOverlay->isVisible() || d->m_snippetOverlay->isEmpty())
    return;
  d->m_snippetOverlay->updateEquivalentSelections(textCursor());
}

auto TextEditorWidget::showEvent(QShowEvent *e) -> void
{
  triggerPendingUpdates();
  // QPlainTextEdit::showEvent scrolls to make the cursor visible on first show
  // which we don't want, since we restore previous states when
  // opening editors, and when splitting/duplicating.
  // So restore the previous state after that.
  QByteArray state;
  if (d->m_wasNotYetShown)
    state = saveState();
  QPlainTextEdit::showEvent(e);
  if (d->m_wasNotYetShown) {
    restoreState(state);
    d->m_wasNotYetShown = false;
  }
}

auto TextEditorWidgetPrivate::applyFontSettingsDelayed() -> void
{
  m_fontSettingsNeedsApply = true;
  if (q->isVisible())
    q->triggerPendingUpdates();
}

auto TextEditorWidgetPrivate::markRemoved(TextMark *mark) -> void
{
  if (m_dragMark == mark) {
    m_dragMark = nullptr;
    m_markDragging = false;
    m_markDragStart = QPoint();
    QGuiApplication::restoreOverrideCursor();
  }

  const auto it = m_annotationRects.find(mark->lineNumber() - 1);
  if (it == m_annotationRects.end())
    return;

  Utils::erase(it.value(), [mark](AnnotationRect rect) {
    return rect.mark == mark;
  });
}

auto TextEditorWidget::triggerPendingUpdates() -> void
{
  if (d->m_fontSettingsNeedsApply)
    applyFontSettings();
  textDocument()->triggerPendingUpdates();
}

auto TextEditorWidget::applyFontSettings() -> void
{
  d->m_fontSettingsNeedsApply = false;
  const auto &fs = textDocument()->fontSettings();
  const auto textFormat = fs.toTextCharFormat(C_TEXT);
  const auto lineNumberFormat = fs.toTextCharFormat(C_LINE_NUMBER);
  const auto font(textFormat.font());

  if (font != this->font()) {
    setFont(font);
    d->updateTabStops(); // update tab stops, they depend on the font
  }

  // Line numbers
  QPalette ep;
  ep.setColor(QPalette::Dark, lineNumberFormat.foreground().color());
  ep.setColor(QPalette::Window, lineNumberFormat.background().style() != Qt::NoBrush ? lineNumberFormat.background().color() : textFormat.background().color());
  if (ep != d->m_extraArea->palette()) {
    d->m_extraArea->setPalette(ep);
    d->slotUpdateExtraAreaWidth(); // Adjust to new font width
  }

  d->updateHighlights();
}

auto TextEditorWidget::setDisplaySettings(const DisplaySettings &ds) -> void
{
  setLineWrapMode(ds.m_textWrapping ? WidgetWidth : NoWrap);
  setLineNumbersVisible(ds.m_displayLineNumbers);
  setHighlightCurrentLine(ds.m_highlightCurrentLine);
  setRevisionsVisible(ds.m_markTextChanges);
  setCenterOnScroll(ds.m_centerCursorOnScroll);
  setParenthesesMatchingEnabled(ds.m_highlightMatchingParentheses);
  d->m_fileEncodingLabelAction->setVisible(ds.m_displayFileEncoding);

  if (d->m_displaySettings.m_visualizeWhitespace != ds.m_visualizeWhitespace) {
    if (const auto highlighter = textDocument()->syntaxHighlighter())
      highlighter->rehighlight();
    auto option = document()->defaultTextOption();
    if (ds.m_visualizeWhitespace)
      option.setFlags(option.flags() | QTextOption::ShowTabsAndSpaces);
    else
      option.setFlags(option.flags() & ~QTextOption::ShowTabsAndSpaces);
    option.setFlags(option.flags() | QTextOption::AddSpaceForLineAndParagraphSeparators);
    document()->setDefaultTextOption(option);
  }

  d->m_displaySettings = ds;
  if (!ds.m_highlightBlocks) {
    d->extraAreaHighlightFoldedBlockNumber = -1;
    d->m_highlightBlocksInfo = TextEditorPrivateHighlightBlocks();
  }

  d->updateCodeFoldingVisible();
  d->updateHighlights();
  d->setupScrollBar();
  viewport()->update();
  extraArea()->update();
}

auto TextEditorWidget::setMarginSettings(const MarginSettings &ms) -> void
{
  d->m_marginSettings = ms;
  updateVisualWrapColumn();

  viewport()->update();
  extraArea()->update();
}

auto TextEditorWidget::setBehaviorSettings(const BehaviorSettings &bs) -> void
{
  d->m_behaviorSettings = bs;
}

auto TextEditorWidget::setTypingSettings(const TypingSettings &typingSettings) -> void
{
  d->m_document->setTypingSettings(typingSettings);
}

auto TextEditorWidget::setStorageSettings(const StorageSettings &storageSettings) -> void
{
  d->m_document->setStorageSettings(storageSettings);
}

auto TextEditorWidget::setCompletionSettings(const CompletionSettings &completionSettings) -> void
{
  d->m_autoCompleter->setAutoInsertBracketsEnabled(completionSettings.m_autoInsertBrackets);
  d->m_autoCompleter->setSurroundWithBracketsEnabled(completionSettings.m_surroundingAutoBrackets);
  d->m_autoCompleter->setAutoInsertQuotesEnabled(completionSettings.m_autoInsertQuotes);
  d->m_autoCompleter->setSurroundWithQuotesEnabled(completionSettings.m_surroundingAutoQuotes);
  d->m_autoCompleter->setOverwriteClosingCharsEnabled(completionSettings.m_overwriteClosingChars);
  d->m_animateAutoComplete = completionSettings.m_animateAutoComplete;
  d->m_highlightAutoComplete = completionSettings.m_highlightAutoComplete;
  d->m_skipAutoCompletedText = completionSettings.m_skipAutoCompletedText;
  d->m_removeAutoCompletedText = completionSettings.m_autoRemove;
}

auto TextEditorWidget::setExtraEncodingSettings(const ExtraEncodingSettings &extraEncodingSettings) -> void
{
  d->m_document->setExtraEncodingSettings(extraEncodingSettings);
}

auto TextEditorWidget::fold() -> void
{
  const auto doc = document();
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
  QTC_ASSERT(documentLayout, return);
  auto block = textCursor().block();
  if (!(TextDocumentLayout::canFold(block) && block.next().isVisible())) {
    // find the closest previous block which can fold
    const auto indent = TextDocumentLayout::foldingIndent(block);
    while (block.isValid() && (TextDocumentLayout::foldingIndent(block) >= indent || !block.isVisible()))
      block = block.previous();
  }
  if (block.isValid()) {
    TextDocumentLayout::doFoldOrUnfold(block, false);
    d->moveCursorVisible();
    documentLayout->requestUpdate();
    documentLayout->emitDocumentSizeChanged();
  }
}

auto TextEditorWidget::unfold() -> void
{
  const auto doc = document();
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
  QTC_ASSERT(documentLayout, return);
  auto block = textCursor().block();
  while (block.isValid() && !block.isVisible())
    block = block.previous();
  TextDocumentLayout::doFoldOrUnfold(block, true);
  d->moveCursorVisible();
  documentLayout->requestUpdate();
  documentLayout->emitDocumentSizeChanged();
}

auto TextEditorWidget::unfoldAll() -> void
{
  const auto doc = document();
  const auto documentLayout = qobject_cast<TextDocumentLayout*>(doc->documentLayout());
  QTC_ASSERT(documentLayout, return);

  auto block = doc->firstBlock();
  auto makeVisible = true;
  while (block.isValid()) {
    if (block.isVisible() && TextDocumentLayout::canFold(block) && block.next().isVisible()) {
      makeVisible = false;
      break;
    }
    block = block.next();
  }

  block = doc->firstBlock();

  while (block.isValid()) {
    if (TextDocumentLayout::canFold(block))
      TextDocumentLayout::doFoldOrUnfold(block, makeVisible);
    block = block.next();
  }

  d->moveCursorVisible();
  documentLayout->requestUpdate();
  documentLayout->emitDocumentSizeChanged();
  centerCursor();
}

auto TextEditorWidget::setReadOnly(bool b) -> void
{
  QPlainTextEdit::setReadOnly(b);
  emit readOnlyChanged();
  if (b)
    setTextInteractionFlags(textInteractionFlags() | Qt::TextSelectableByKeyboard);
}

auto TextEditorWidget::cut() -> void
{
  copy();
  auto cursor = multiTextCursor();
  cursor.removeSelectedText();
  setMultiTextCursor(cursor);
  d->collectToCircularClipboard();
}

auto TextEditorWidget::selectAll() -> void
{
  QPlainTextEdit::selectAll();
  // Directly update the internal multi text cursor here to prevent calling setTextCursor.
  // This would indirectly makes sure the cursor is visible which is not desired for select all.
  const_cast<MultiTextCursor&>(d->m_cursors).setCursors({textCursor()});
}

auto TextEditorWidget::copy() -> void
{
  QPlainTextEdit::copy();
  d->collectToCircularClipboard();
}

auto TextEditorWidget::paste() -> void
{
  QPlainTextEdit::paste();
  encourageApply();
}

auto TextEditorWidgetPrivate::collectToCircularClipboard() -> void
{
  const auto mimeData = QApplication::clipboard()->mimeData();
  if (!mimeData)
    return;
  const auto circularClipBoard = CircularClipboard::instance();
  circularClipBoard->collect(TextEditorWidget::duplicateMimeData(mimeData));
  // We want the latest copied content to be the first one to appear on circular paste.
  circularClipBoard->toLastCollect();
}

auto TextEditorWidget::circularPaste() -> void
{
  const auto circularClipBoard = CircularClipboard::instance();
  if (const auto clipboardData = QApplication::clipboard()->mimeData()) {
    circularClipBoard->collect(duplicateMimeData(clipboardData));
    circularClipBoard->toLastCollect();
  }

  if (circularClipBoard->size() > 1) {
    invokeAssist(QuickFix, d->m_clipboardAssistProvider.data());
    return;
  }

  if (const auto mimeData = circularClipBoard->next().data()) {
    QApplication::clipboard()->setMimeData(duplicateMimeData(mimeData));
    paste();
  }
}

auto TextEditorWidget::pasteWithoutFormat() -> void
{
  d->m_skipFormatOnPaste = true;
  paste();
  d->m_skipFormatOnPaste = false;
}

auto TextEditorWidget::switchUtf8bom() -> void
{
  textDocument()->switchUtf8Bom();
}

auto TextEditorWidget::createMimeDataFromSelection() const -> QMimeData*
{
  if (multiTextCursor().hasSelection()) {
    const auto mimeData = new QMimeData;

    auto text = plainTextFromSelection(multiTextCursor());
    mimeData->setText(text);

    // Copy the selected text as HTML
    {
      // Create a new document from the selected text document fragment
      const auto tempDocument = new QTextDocument;
      QTextCursor tempCursor(tempDocument);
      for (const auto &cursor : multiTextCursor()) {
        if (!cursor.hasSelection())
          continue;
        tempCursor.insertFragment(cursor.selection());

        // Apply the additional formats set by the syntax highlighter
        auto start = document()->findBlock(cursor.selectionStart());
        auto last = document()->findBlock(cursor.selectionEnd());
        auto end = last.next();

        const auto selectionStart = cursor.selectionStart();
        const auto endOfDocument = tempDocument->characterCount() - 1;
        auto removedCount = 0;
        for (auto current = start; current.isValid() && current != end; current = current.next()) {
          if (selectionVisible(current.blockNumber())) {
            const QTextLayout *layout = current.layout();
            foreach(const QTextLayout::FormatRange &range, layout->formats()) {
              const auto startPosition = current.position() + range.start - selectionStart - removedCount;
              const auto endPosition = startPosition + range.length;
              if (endPosition <= 0 || startPosition >= endOfDocument - removedCount)
                continue;
              tempCursor.setPosition(qMax(startPosition, 0));
              tempCursor.setPosition(qMin(endPosition, endOfDocument - removedCount), QTextCursor::KeepAnchor);
              tempCursor.setCharFormat(range.format);
            }
          } else {
            const auto startPosition = current.position() - start.position() - removedCount;
            int endPosition = startPosition + current.text().count();
            if (current != last)
              endPosition++;
            removedCount += endPosition - startPosition;
            tempCursor.setPosition(startPosition);
            tempCursor.setPosition(endPosition, QTextCursor::KeepAnchor);
            tempCursor.deleteChar();
          }
        }
      }

      // Reset the user states since they are not interesting
      for (auto block = tempDocument->begin(); block.isValid(); block = block.next())
        block.setUserState(-1);

      // Make sure the text appears pre-formatted
      tempCursor.setPosition(0);
      tempCursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
      auto blockFormat = tempCursor.blockFormat();
      blockFormat.setNonBreakableLines(true);
      tempCursor.setBlockFormat(blockFormat);

      mimeData->setHtml(tempCursor.selection().toHtml());
      delete tempDocument;
    }

    if (!multiTextCursor().hasMultipleCursors()) {
      /*
          Try to figure out whether we are copying an entire block, and store the
          complete block including indentation in the qtcreator.blocktext mimetype.
      */
      auto cursor = multiTextCursor().mainCursor();
      auto selstart = cursor;
      selstart.setPosition(cursor.selectionStart());
      auto selend = cursor;
      selend.setPosition(cursor.selectionEnd());

      const auto startOk = TabSettings::cursorIsAtBeginningOfLine(selstart);
      const auto multipleBlocks = selend.block() != selstart.block();

      if (startOk && multipleBlocks) {
        selstart.movePosition(QTextCursor::StartOfBlock);
        if (TabSettings::cursorIsAtBeginningOfLine(selend))
          selend.movePosition(QTextCursor::StartOfBlock);
        cursor.setPosition(selstart.position());
        cursor.setPosition(selend.position(), QTextCursor::KeepAnchor);
        text = plainTextFromSelection(cursor);
        mimeData->setData(QLatin1String(kTextBlockMimeType), text.toUtf8());
      }
    }
    return mimeData;
  }
  return nullptr;
}

auto TextEditorWidget::canInsertFromMimeData(const QMimeData *source) const -> bool
{
  return QPlainTextEdit::canInsertFromMimeData(source);
}

struct MappedText {
  MappedText(const QString text, MultiTextCursor &cursor) : text(text)
  {
    if (cursor.hasMultipleCursors()) {
      texts = text.split('\n');
      if (texts.last().isEmpty())
        texts.removeLast();
      if (texts.count() != cursor.cursorCount())
        texts.clear();
    }
  }

  auto textAt(int i) const -> QString
  {
    return texts.value(i, text);
  }

  QStringList texts;
  const QString text;
};

auto TextEditorWidget::insertFromMimeData(const QMimeData *source) -> void
{
  if (isReadOnly())
    return;

  auto text = source->text();
  if (text.isEmpty())
    return;

  if (d->m_codeAssistant.hasContext())
    d->m_codeAssistant.destroyContext();

  if (d->m_snippetOverlay->isVisible() && (text.contains('\n') || text.contains('\t')))
    d->m_snippetOverlay->accept();

  const auto selectInsertedText = source->property(dropProperty).toBool();
  const auto &tps = d->m_document->typingSettings();
  auto cursor = multiTextCursor();
  if (!tps.m_autoIndent) {
    cursor.insertText(text, selectInsertedText);
    setMultiTextCursor(cursor);
    return;
  }

  if (source->hasFormat(QLatin1String(kTextBlockMimeType))) {
    text = QString::fromUtf8(source->data(QLatin1String(kTextBlockMimeType)));
    if (text.isEmpty())
      return;
  }

  const MappedText mappedText(text, cursor);

  auto index = 0;
  cursor.beginEditBlock();
  for (QTextCursor &cursor : cursor) {
    const auto textForCursor = mappedText.textAt(index++);

    cursor.removeSelectedText();

    const auto insertAtBeginningOfLine = TabSettings::cursorIsAtBeginningOfLine(cursor);
    const auto reindentBlockStart = cursor.blockNumber() + (insertAtBeginningOfLine ? 0 : 1);

    const auto hasFinalNewline = textForCursor.endsWith(QLatin1Char('\n')) || textForCursor.endsWith(QChar::ParagraphSeparator) || textForCursor.endsWith(QLatin1Char('\r'));

    if (insertAtBeginningOfLine && hasFinalNewline) // since we'll add a final newline, preserve current line's indentation
      cursor.setPosition(cursor.block().position());

    const auto cursorPosition = cursor.position();
    cursor.insertText(textForCursor);
    const auto endCursor = cursor;
    auto startCursor = endCursor;
    startCursor.setPosition(cursorPosition);

    const auto reindentBlockEnd = cursor.blockNumber() - (hasFinalNewline ? 1 : 0);

    if (!d->m_skipFormatOnPaste && (reindentBlockStart < reindentBlockEnd || reindentBlockStart == reindentBlockEnd && (!insertAtBeginningOfLine || hasFinalNewline))) {
      if (insertAtBeginningOfLine && !hasFinalNewline) {
        auto unnecessaryWhitespace = cursor;
        unnecessaryWhitespace.setPosition(cursorPosition);
        unnecessaryWhitespace.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        unnecessaryWhitespace.removeSelectedText();
      }
      auto c = cursor;
      c.setPosition(cursor.document()->findBlockByNumber(reindentBlockStart).position());
      c.setPosition(cursor.document()->findBlockByNumber(reindentBlockEnd).position(), QTextCursor::KeepAnchor);
      d->m_document->autoReindent(c);
    }

    if (selectInsertedText) {
      cursor.setPosition(startCursor.position());
      cursor.setPosition(endCursor.position(), QTextCursor::KeepAnchor);
    }
  }
  cursor.endEditBlock();
  setMultiTextCursor(cursor);
}

auto TextEditorWidget::dragLeaveEvent(QDragLeaveEvent *) -> void
{
  const auto rect = cursorRect(d->m_dndCursor);
  d->m_dndCursor = QTextCursor();
  if (!rect.isNull())
    viewport()->update(rect);
}

auto TextEditorWidget::dragMoveEvent(QDragMoveEvent *e) -> void
{
  const auto rect = cursorRect(d->m_dndCursor);
  d->m_dndCursor = cursorForPosition(e->pos());
  if (!rect.isNull())
    viewport()->update(rect);
  viewport()->update(cursorRect(d->m_dndCursor));
}

auto TextEditorWidget::dropEvent(QDropEvent *e) -> void
{
  const auto rect = cursorRect(d->m_dndCursor);
  d->m_dndCursor = QTextCursor();
  if (!rect.isNull())
    viewport()->update(rect);
  auto mime = e->mimeData();
  if (!canInsertFromMimeData(mime))
    return;
  // Update multi text cursor before inserting data
  auto cursor = multiTextCursor();
  cursor.beginEditBlock();
  const auto eventCursor = cursorForPosition(e->pos());
  if (e->dropAction() == Qt::MoveAction)
    cursor.removeSelectedText();
  cursor.setCursors({eventCursor});
  setMultiTextCursor(cursor);
  QMimeData *mimeOverwrite = nullptr;
  if (mime && (mime->hasText() || mime->hasHtml())) {
    mimeOverwrite = duplicateMimeData(mime);
    mimeOverwrite->setProperty(dropProperty, true);
    mime = mimeOverwrite;
  }
  insertFromMimeData(mime);
  delete mimeOverwrite;
  cursor.endEditBlock();
}

auto TextEditorWidget::duplicateMimeData(const QMimeData *source) -> QMimeData*
{
  Q_ASSERT(source);

  const auto mimeData = new QMimeData;
  mimeData->setText(source->text());
  mimeData->setHtml(source->html());
  if (source->hasFormat(QLatin1String(kTextBlockMimeType))) {
    mimeData->setData(QLatin1String(kTextBlockMimeType), source->data(QLatin1String(kTextBlockMimeType)));
  }

  return mimeData;
}

auto TextEditorWidget::lineNumber(int blockNumber) const -> QString
{
  return QString::number(blockNumber + 1);
}

auto TextEditorWidget::lineNumberDigits() const -> int
{
  auto digits = 2;
  auto max = qMax(1, blockCount());
  while (max >= 100) {
    max /= 10;
    ++digits;
  }
  return digits;
}

auto TextEditorWidget::selectionVisible(int blockNumber) const -> bool
{
  Q_UNUSED(blockNumber)
  return true;
}

auto TextEditorWidget::replacementVisible(int blockNumber) const -> bool
{
  Q_UNUSED(blockNumber)
  return true;
}

auto TextEditorWidget::replacementPenColor(int blockNumber) const -> QColor
{
  Q_UNUSED(blockNumber)
  return {};
}

auto TextEditorWidget::setupFallBackEditor(Id id) -> void
{
  const TextDocumentPtr doc(new TextDocument(id));
  doc->setFontSettings(TextEditorSettings::fontSettings());
  setTextDocument(doc);
}

auto TextEditorWidget::appendStandardContextMenuActions(QMenu *menu) -> void
{
  menu->addSeparator();
  appendMenuActionsFromContext(menu, Constants::M_STANDARDCONTEXTMENU);
  Command *bomCmd = ActionManager::command(Constants::SWITCH_UTF8BOM);
  if (bomCmd) {
    QAction *a = bomCmd->action();
    auto doc = textDocument();
    if (doc->codec()->name() == QByteArray("UTF-8") && doc->supportsUtf8Bom()) {
      a->setVisible(true);
      a->setText(doc->format().hasUtf8Bom ? tr("Delete UTF-8 BOM on Save") : tr("Add UTF-8 BOM on Save"));
    } else {
      a->setVisible(false);
    }
  }
}

auto TextEditorWidget::optionalActions() -> uint
{
  return d->m_optionalActionMask;
}

auto TextEditorWidget::setOptionalActions(uint optionalActionMask) -> void
{
  if (d->m_optionalActionMask == optionalActionMask)
    return;
  d->m_optionalActionMask = optionalActionMask;
  emit optionalActionMaskChanged();
}

auto TextEditorWidget::addOptionalActions(uint optionalActionMask) -> void
{
  setOptionalActions(d->m_optionalActionMask | optionalActionMask);
}

BaseTextEditor::BaseTextEditor() : d(new BaseTextEditorPrivate)
{
  addContext(Constants::C_TEXTEDITOR);
}

BaseTextEditor::~BaseTextEditor()
{
  delete m_widget;
  delete d;
}

auto BaseTextEditor::textDocument() const -> TextDocument*
{
  const auto widget = editorWidget();
  QTC_CHECK(!widget->d->m_document.isNull());
  return widget->d->m_document.data();
}

auto BaseTextEditor::addContext(Id id) -> void
{
  m_context.add(id);
}

auto BaseTextEditor::document() const -> IDocument*
{
  return textDocument();
}

auto BaseTextEditor::toolBar() -> QWidget*
{
  return editorWidget()->d->m_toolBarWidget;
}

auto TextEditorWidget::insertExtraToolBarWidget(Side side, QWidget *widget) -> QAction*
{
  if (widget->sizePolicy().horizontalPolicy() & QSizePolicy::ExpandFlag) {
    if (d->m_stretchWidget)
      d->m_stretchWidget->deleteLater();
    d->m_stretchWidget = nullptr;
  }

  if (side == Left) {
    const auto before = findOr(d->m_toolBar->actions(), d->m_fileEncodingLabelAction, [this](QAction *action) {
      return d->m_toolBar->widgetForAction(action) != nullptr;
    });
    return d->m_toolBar->insertWidget(before, widget);
  }
  return d->m_toolBar->insertWidget(d->m_fileEncodingLabelAction, widget);
}

auto TextEditorWidget::keepAutoCompletionHighlight(bool keepHighlight) -> void
{
  d->m_keepAutoCompletionHighlight = keepHighlight;
}

auto TextEditorWidget::setAutoCompleteSkipPosition(const QTextCursor &cursor) -> void
{
  auto tc = cursor;
  // Create a selection of the next character but keep the current position, otherwise
  // the cursor would be removed from the list of automatically inserted text positions
  tc.movePosition(QTextCursor::NextCharacter);
  tc.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor);
  d->autocompleterHighlight(tc);
}

auto BaseTextEditor::currentLine() const -> int
{
  return editorWidget()->textCursor().blockNumber() + 1;
}

auto BaseTextEditor::currentColumn() const -> int
{
  const auto cursor = editorWidget()->textCursor();
  return cursor.position() - cursor.block().position() + 1;
}

auto BaseTextEditor::gotoLine(int line, int column, bool centerLine) -> void
{
  editorWidget()->gotoLine(line, column, centerLine);
}

auto BaseTextEditor::columnCount() const -> int
{
  return editorWidget()->columnCount();
}

auto BaseTextEditor::rowCount() const -> int
{
  return editorWidget()->rowCount();
}

auto BaseTextEditor::position(TextPositionOperation posOp, int at) const -> int
{
  return editorWidget()->position(posOp, at);
}

auto BaseTextEditor::convertPosition(int pos, int *line, int *column) const -> void
{
  editorWidget()->convertPosition(pos, line, column);
}

auto BaseTextEditor::selectedText() const -> QString
{
  return editorWidget()->selectedText();
}

auto BaseTextEditor::remove(int length) -> void
{
  editorWidget()->remove(length);
}

auto TextEditorWidget::remove(int length) -> void
{
  auto tc = textCursor();
  tc.setPosition(tc.position() + length, QTextCursor::KeepAnchor);
  tc.removeSelectedText();
}

auto BaseTextEditor::insert(const QString &string) -> void
{
  editorWidget()->insertPlainText(string);
}

auto BaseTextEditor::replace(int length, const QString &string) -> void
{
  editorWidget()->replace(length, string);
}

auto TextEditorWidget::replace(int length, const QString &string) -> void
{
  auto tc = textCursor();
  tc.setPosition(tc.position() + length, QTextCursor::KeepAnchor);
  tc.insertText(string);
}

auto BaseTextEditor::setCursorPosition(int pos) -> void
{
  editorWidget()->setCursorPosition(pos);
}

auto TextEditorWidget::setCursorPosition(int pos) -> void
{
  auto tc = textCursor();
  tc.setPosition(pos);
  setTextCursor(tc);
}

auto TextEditorWidget::toolBar() -> QToolBar*
{
  return d->m_toolBar;
}

auto BaseTextEditor::select(int toPos) -> void
{
  auto tc = editorWidget()->textCursor();
  tc.setPosition(toPos, QTextCursor::KeepAnchor);
  editorWidget()->setTextCursor(tc);
}

auto TextEditorWidgetPrivate::updateCursorPosition() -> void
{
  m_contextHelpItem = HelpItem();
  if (!q->textCursor().block().isVisible())
    q->ensureCursorVisible();
}

auto BaseTextEditor::contextHelp(const HelpCallback &callback) const -> void
{
  editorWidget()->contextHelpItem(callback);
}

auto BaseTextEditor::setContextHelp(const HelpItem &item) -> void
{
  IEditor::setContextHelp(item);
  editorWidget()->setContextHelpItem(item);
}

auto TextEditorWidget::contextHelpItem(const IContext::HelpCallback &callback) -> void
{
  if (!d->m_contextHelpItem.isEmpty()) {
    callback(d->m_contextHelpItem);
    return;
  }
  const auto fallbackWordUnderCursor = Text::wordUnderCursor(textCursor());
  if (d->m_hoverHandlers.isEmpty()) {
    callback(fallbackWordUnderCursor);
    return;
  }

  const auto hoverHandlerCallback = [fallbackWordUnderCursor, callback](TextEditorWidget *widget, BaseHoverHandler *handler, int position) {
    handler->contextHelpId(widget, position, [fallbackWordUnderCursor, callback](const HelpItem &item) {
      if (item.isEmpty())
        callback(fallbackWordUnderCursor);
      else
        callback(item);
    });

  };
  d->m_hoverHandlerRunner.startChecking(textCursor(), hoverHandlerCallback);
}

auto TextEditorWidget::setContextHelpItem(const HelpItem &item) -> void
{
  d->m_contextHelpItem = item;
}

auto TextEditorWidget::refactorMarkers() const -> RefactorMarkers
{
  return d->m_refactorOverlay->markers();
}

auto TextEditorWidget::setRefactorMarkers(const RefactorMarkers &markers) -> void
{
  foreach(const RefactorMarker &marker, d->m_refactorOverlay->markers()) emit requestBlockUpdate(marker.cursor.block());
  d->m_refactorOverlay->setMarkers(markers);
  foreach(const RefactorMarker &marker, markers) emit requestBlockUpdate(marker.cursor.block());
}

auto TextEditorWidget::inFindScope(const QTextCursor &cursor) const -> bool
{
  return d->m_find->inScope(cursor);
}

auto TextEditorWidget::updateVisualWrapColumn() -> void
{
  auto calcMargin = [this]() {
    const auto &ms = d->m_marginSettings;

    if (!ms.m_showMargin) {
      return 0;
    }
    if (ms.m_useIndenter) {
      if (const auto margin = d->m_document->indenter()->margin()) {
        return *margin;
      }
    }
    return ms.m_marginColumn;
  };
  setVisibleWrapColumn(calcMargin());
}

auto TextEditorWidgetPrivate::updateTabStops() -> void
{
  // Although the tab stop is stored as qreal the API from QPlainTextEdit only allows it
  // to be set as an int. A work around is to access directly the QTextOption.
  const auto charWidth = QFontMetricsF(q->font()).horizontalAdvance(QLatin1Char(' '));
  auto option = q->document()->defaultTextOption();
  option.setTabStopDistance(charWidth * m_document->tabSettings().m_tabSize);
  q->document()->setDefaultTextOption(option);
}

auto TextEditorWidget::columnCount() const -> int
{
  const QFontMetricsF fm(font());
  return int(viewport()->rect().width() / fm.horizontalAdvance(QLatin1Char('x')));
}

auto TextEditorWidget::rowCount() const -> int
{
  auto height = viewport()->rect().height();
  auto lineCount = 0;
  auto block = firstVisibleBlock();
  while (block.isValid()) {
    height -= blockBoundingRect(block).height();
    if (height < 0) {
      const auto blockLineCount = block.layout()->lineCount();
      for (auto i = 0; i < blockLineCount; ++i) {
        ++lineCount;
        const auto line = block.layout()->lineAt(i);
        height += line.rect().height();
        if (height >= 0)
          break;
      }
      return lineCount;
    }
    lineCount += block.layout()->lineCount();
    block = block.next();
  }
  return lineCount;
}

/**
  Helper function to transform a selected text. If nothing is selected at the moment
  the word under the cursor is used.
  The type of the transformation is determined by the function pointer given.

  @param method     pointer to the QString function to use for the transformation

  @see uppercaseSelection, lowercaseSelection
*/
auto TextEditorWidgetPrivate::transformSelection(TransformationMethod method) -> void
{
  auto cursor = m_cursors;
  cursor.beginEditBlock();
  for (auto &c : cursor) {
    const auto pos = c.position();
    const auto anchor = c.anchor();

    if (!c.hasSelection() && !m_cursors.hasMultipleCursors()) {
      // if nothing is selected, select the word under the cursor
      c.select(QTextCursor::WordUnderCursor);
    }

    auto text = c.selectedText();
    auto transformedText = method(text);

    if (transformedText == text)
      continue;

    c.insertText(transformedText);

    // (re)select the changed text
    // Note: this assumes the transformation did not change the length,
    c.setPosition(anchor);
    c.setPosition(pos, QTextCursor::KeepAnchor);
  }
  cursor.endEditBlock();
  q->setMultiTextCursor(cursor);
}

auto TextEditorWidgetPrivate::transformSelectedLines(ListTransformationMethod method) -> void
{
  if (!method || m_cursors.hasMultipleCursors())
    return;

  auto cursor = q->textCursor();
  if (!cursor.hasSelection())
    return;

  const auto downwardDirection = cursor.anchor() < cursor.position();
  auto startPosition = cursor.selectionStart();
  auto endPosition = cursor.selectionEnd();

  cursor.setPosition(startPosition);
  cursor.movePosition(QTextCursor::StartOfBlock);
  startPosition = cursor.position();

  cursor.setPosition(endPosition, QTextCursor::KeepAnchor);
  if (cursor.positionInBlock() == 0)
    cursor.movePosition(QTextCursor::PreviousBlock, QTextCursor::KeepAnchor);
  cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
  endPosition = qMax(cursor.position(), endPosition);

  const auto text = cursor.selectedText();
  auto lines = text.split(QChar::ParagraphSeparator);
  method(lines);
  cursor.insertText(lines.join(QChar::ParagraphSeparator));

  // (re)select the changed lines
  // Note: this assumes the transformation did not change the length
  cursor.setPosition(downwardDirection ? startPosition : endPosition);
  cursor.setPosition(downwardDirection ? endPosition : startPosition, QTextCursor::KeepAnchor);
  q->setTextCursor(cursor);
}

auto TextEditorWidget::inSnippetMode(bool *active) -> void
{
  *active = d->m_snippetOverlay->isVisible();
}

auto TextEditorWidget::blockForVisibleRow(int row) const -> QTextBlock
{
  const auto count = rowCount();
  if (row < 0 && row >= count)
    return QTextBlock();

  auto block = firstVisibleBlock();
  for (auto i = 0; i < count;) {
    if (!block.isValid() || i >= row)
      return block;

    i += block.lineCount();
    block = d->nextVisibleBlock(block);
  }
  return QTextBlock();
}

auto TextEditorWidget::blockForVerticalOffset(int offset) const -> QTextBlock
{
  auto block = firstVisibleBlock();
  while (block.isValid()) {
    offset -= blockBoundingRect(block).height();
    if (offset < 0)
      return block;
    block = block.next();
  }
  return block;
}

auto TextEditorWidget::invokeAssist(AssistKind kind, IAssistProvider *provider) -> void
{
  if (multiTextCursor().hasMultipleCursors())
    return;

  if (kind == QuickFix && d->m_snippetOverlay->isVisible())
    d->m_snippetOverlay->accept();

  const auto previousMode = overwriteMode();
  setOverwriteMode(false);
  ensureCursorVisible();
  d->m_codeAssistant.invoke(kind, provider);
  setOverwriteMode(previousMode);
}

auto TextEditorWidget::createAssistInterface(AssistKind kind, AssistReason reason) const -> AssistInterface*
{
  Q_UNUSED(kind)
  return new AssistInterface(document(), position(), d->m_document->filePath(), reason);
}

auto TextEditorWidget::foldReplacementText(const QTextBlock &) const -> QString
{
  return QLatin1String("...");
}

auto BaseTextEditor::saveState() const -> QByteArray
{
  return editorWidget()->saveState();
}

auto BaseTextEditor::restoreState(const QByteArray &state) -> void
{
  editorWidget()->restoreState(state);
}

auto BaseTextEditor::currentTextEditor() -> BaseTextEditor*
{
  return qobject_cast<BaseTextEditor*>(EditorManager::currentEditor());
}

auto BaseTextEditor::textEditorsForDocument(TextDocument *textDocument) -> QVector<BaseTextEditor*>
{
  QVector<BaseTextEditor*> ret;
  for (IEditor *editor : DocumentModel::editorsForDocument(textDocument)) {
    if (auto textEditor = qobject_cast<BaseTextEditor*>(editor))
      ret << textEditor;
  }
  return ret;
}

auto BaseTextEditor::editorWidget() const -> TextEditorWidget*
{
  const auto textEditorWidget = TextEditorWidget::fromEditor(this);
  QTC_CHECK(textEditorWidget);
  return textEditorWidget;
}

auto BaseTextEditor::setTextCursor(const QTextCursor &cursor) -> void
{
  editorWidget()->setTextCursor(cursor);
}

auto BaseTextEditor::textCursor() const -> QTextCursor
{
  return editorWidget()->textCursor();
}

auto BaseTextEditor::characterAt(int pos) const -> QChar
{
  return textDocument()->characterAt(pos);
}

auto BaseTextEditor::textAt(int from, int to) const -> QString
{
  return textDocument()->textAt(from, to);
}

auto TextEditorWidget::characterAt(int pos) const -> QChar
{
  return textDocument()->characterAt(pos);
}

auto TextEditorWidget::textAt(int from, int to) const -> QString
{
  return textDocument()->textAt(from, to);
}

auto TextEditorWidget::configureGenericHighlighter() -> void
{
  auto definitions = Highlighter::definitionsForDocument(textDocument());
  d->configureGenericHighlighter(definitions.isEmpty() ? Highlighter::Definition() : definitions.first());
  d->updateSyntaxInfoBar(definitions, textDocument()->filePath().fileName());
}

auto TextEditorWidget::blockNumberForVisibleRow(int row) const -> int
{
  const auto block = blockForVisibleRow(row);
  return block.isValid() ? block.blockNumber() : -1;
}

auto TextEditorWidget::firstVisibleBlockNumber() const -> int
{
  return blockNumberForVisibleRow(0);
}

auto TextEditorWidget::lastVisibleBlockNumber() const -> int
{
  auto block = blockForVerticalOffset(viewport()->height() - 1);
  if (!block.isValid()) {
    block = document()->lastBlock();
    while (block.isValid() && !block.isVisible())
      block = block.previous();
  }
  return block.isValid() ? block.blockNumber() : -1;
}

auto TextEditorWidget::centerVisibleBlockNumber() const -> int
{
  const auto block = blockForVerticalOffset(viewport()->height() / 2);
  if (!block.isValid())
    block.previous();
  return block.isValid() ? block.blockNumber() : -1;
}

auto TextEditorWidget::highlightScrollBarController() const -> HighlightScrollBarController*
{
  return d->m_highlightScrollBarController;
}

// The remnants of PlainTextEditor.
auto TextEditorWidget::setupGenericHighlighter() -> void
{
  setLineSeparatorsAllowed(true);

  connect(textDocument(), &IDocument::filePathChanged, d, &TextEditorWidgetPrivate::reconfigure);
}

//
// TextEditorLinkLabel
//
TextEditorLinkLabel::TextEditorLinkLabel(QWidget *parent) : ElidingLabel(parent)
{
  setElideMode(Qt::ElideMiddle);
}

auto TextEditorLinkLabel::setLink(Link link) -> void
{
  m_link = link;
}

auto TextEditorLinkLabel::link() const -> Link
{
  return m_link;
}

auto TextEditorLinkLabel::mousePressEvent(QMouseEvent *event) -> void
{
  if (event->button() == Qt::LeftButton)
    m_dragStartPosition = event->pos();
}

auto TextEditorLinkLabel::mouseMoveEvent(QMouseEvent *event) -> void
{
  if (!(event->buttons() & Qt::LeftButton))
    return;
  if ((event->pos() - m_dragStartPosition).manhattanLength() < QApplication::startDragDistance())
    return;

  const auto data = new DropMimeData;
  data->addFile(m_link.targetFilePath, m_link.targetLine, m_link.targetColumn);
  const auto drag = new QDrag(this);
  drag->setMimeData(data);
  drag->exec(Qt::CopyAction);
}

auto TextEditorLinkLabel::mouseReleaseEvent(QMouseEvent *event) -> void
{
  Q_UNUSED(event)
  if (!m_link.hasValidTarget())
    return;

  EditorManager::openEditorAt(m_link);
}

//
// BaseTextEditorFactory
//

namespace Internal {

class TextEditorFactoryPrivate {
public:
  TextEditorFactoryPrivate(TextEditorFactory *parent) : q(parent), m_widgetCreator([]() { return new TextEditorWidget; }) {}

  auto duplicateTextEditor(BaseTextEditor *other) -> BaseTextEditor*
  {
    const auto editor = createEditorHelper(other->editorWidget()->textDocumentPtr());
    editor->editorWidget()->finalizeInitializationAfterDuplication(other->editorWidget());
    return editor;
  }

  auto createEditorHelper(const TextDocumentPtr &doc) -> BaseTextEditor*;

  TextEditorFactory *q;
  TextEditorFactory::DocumentCreator m_documentCreator;
  TextEditorFactory::EditorWidgetCreator m_widgetCreator;
  TextEditorFactory::EditorCreator m_editorCreator;
  TextEditorFactory::AutoCompleterCreator m_autoCompleterCreator;
  TextEditorFactory::IndenterCreator m_indenterCreator;
  TextEditorFactory::SyntaxHighLighterCreator m_syntaxHighlighterCreator;
  CommentDefinition m_commentDefinition;
  QList<BaseHoverHandler*> m_hoverHandlers;                       // owned
  CompletionAssistProvider *m_completionAssistProvider = nullptr; // owned
  std::unique_ptr<TextEditorActionHandler> m_textEditorActionHandler;
  bool m_useGenericHighlighter = false;
  bool m_duplicatedSupported = true;
  bool m_codeFoldingSupported = false;
  bool m_paranthesesMatchinEnabled = false;
  bool m_marksVisible = true;
};

} /// namespace Internal

TextEditorFactory::TextEditorFactory() : d(new TextEditorFactoryPrivate(this))
{
  setEditorCreator([]() { return new BaseTextEditor; });
}

TextEditorFactory::~TextEditorFactory()
{
  qDeleteAll(d->m_hoverHandlers);
  delete d->m_completionAssistProvider;
  delete d;
}

auto TextEditorFactory::setDocumentCreator(const DocumentCreator &creator) -> void
{
  d->m_documentCreator = creator;
}

auto TextEditorFactory::setEditorWidgetCreator(const EditorWidgetCreator &creator) -> void
{
  d->m_widgetCreator = creator;
}

auto TextEditorFactory::setEditorCreator(const EditorCreator &creator) -> void
{
  d->m_editorCreator = creator;
  IEditorFactory::setEditorCreator([this] {
    static DocumentContentCompletionProvider basicSnippetProvider;
    const TextDocumentPtr doc(d->m_documentCreator());

    if (d->m_indenterCreator)
      doc->setIndenter(d->m_indenterCreator(doc->document()));

    if (d->m_syntaxHighlighterCreator)
      doc->setSyntaxHighlighter(d->m_syntaxHighlighterCreator());

    doc->setCompletionAssistProvider(d->m_completionAssistProvider ? d->m_completionAssistProvider : &basicSnippetProvider);

    return d->createEditorHelper(doc);
  });
}

auto TextEditorFactory::setIndenterCreator(const IndenterCreator &creator) -> void
{
  d->m_indenterCreator = creator;
}

auto TextEditorFactory::setSyntaxHighlighterCreator(const SyntaxHighLighterCreator &creator) -> void
{
  d->m_syntaxHighlighterCreator = creator;
}

auto TextEditorFactory::setUseGenericHighlighter(bool enabled) -> void
{
  d->m_useGenericHighlighter = enabled;
}

auto TextEditorFactory::setAutoCompleterCreator(const AutoCompleterCreator &creator) -> void
{
  d->m_autoCompleterCreator = creator;
}

auto TextEditorFactory::setEditorActionHandlers(uint optionalActions) -> void
{
  d->m_textEditorActionHandler.reset(new TextEditorActionHandler(id(), id(), optionalActions));
}

auto TextEditorFactory::addHoverHandler(BaseHoverHandler *handler) -> void
{
  d->m_hoverHandlers.append(handler);
}

auto TextEditorFactory::setCompletionAssistProvider(CompletionAssistProvider *provider) -> void
{
  d->m_completionAssistProvider = provider;
}

auto TextEditorFactory::setCommentDefinition(CommentDefinition definition) -> void
{
  d->m_commentDefinition = definition;
}

auto TextEditorFactory::setDuplicatedSupported(bool on) -> void
{
  d->m_duplicatedSupported = on;
}

auto TextEditorFactory::setMarksVisible(bool on) -> void
{
  d->m_marksVisible = on;
}

auto TextEditorFactory::setCodeFoldingSupported(bool on) -> void
{
  d->m_codeFoldingSupported = on;
}

auto TextEditorFactory::setParenthesesMatchingEnabled(bool on) -> void
{
  d->m_paranthesesMatchinEnabled = on;
}

auto TextEditorFactoryPrivate::createEditorHelper(const TextDocumentPtr &document) -> BaseTextEditor*
{
  auto widget = m_widgetCreator();
  auto textEditorWidget = Aggregation::query<TextEditorWidget>(widget);
  QTC_ASSERT(textEditorWidget, return nullptr);
  textEditorWidget->setMarksVisible(m_marksVisible);
  textEditorWidget->setParenthesesMatchingEnabled(m_paranthesesMatchinEnabled);
  textEditorWidget->setCodeFoldingSupported(m_codeFoldingSupported);
  if (m_textEditorActionHandler)
    textEditorWidget->setOptionalActions(m_textEditorActionHandler->optionalActions());

  auto editor = m_editorCreator();
  editor->setDuplicateSupported(m_duplicatedSupported);
  editor->addContext(q->id());
  editor->d->m_origin = this;

  editor->m_widget = widget;

  // Needs to go before setTextDocument as this copies the current settings.
  if (m_autoCompleterCreator)
    textEditorWidget->setAutoCompleter(m_autoCompleterCreator());

  textEditorWidget->setTextDocument(document);
  textEditorWidget->autoCompleter()->setTabSettings(document->tabSettings());
  textEditorWidget->d->m_hoverHandlers = m_hoverHandlers;

  textEditorWidget->d->m_codeAssistant.configure(textEditorWidget);
  textEditorWidget->d->m_commentDefinition = m_commentDefinition;

  QObject::connect(textEditorWidget, &TextEditorWidget::activateEditor, textEditorWidget, [editor](EditorManager::OpenEditorFlags flags) {
    EditorManager::activateEditor(editor, flags);
  });

  if (m_useGenericHighlighter)
    textEditorWidget->setupGenericHighlighter();
  textEditorWidget->finalizeInitialization();
  editor->finalizeInitialization();
  return editor;
}

auto BaseTextEditor::duplicate() -> IEditor*
{
  // Use new standard setup if that's available.
  if (d->m_origin) {
    IEditor *dup = d->m_origin->duplicateTextEditor(this);
    emit editorDuplicated(dup);
    return dup;
  }

  // If neither is sufficient, you need to implement 'YourEditor::duplicate'.
  QTC_CHECK(false);
  return nullptr;
}

} // namespace TextEditor

QT_BEGIN_NAMESPACE

auto qHash(const QColor &color) -> QHashValueType
{
    return color.rgba();
}

QT_END_NAMESPACE

#include "texteditor.moc"
