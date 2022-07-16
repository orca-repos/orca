// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "texteditoractionhandler.hpp"

#include "texteditor.hpp"
#include "displaysettings.hpp"
#include "linenumberfilter.hpp"
#include "texteditorconstants.hpp"
#include "texteditorplugin.hpp"

#include <core/core-locator-manager.hpp>
#include <core/core-interface.hpp>
#include <core/core-editor-manager.hpp>
#include <core/core-constants.hpp>
#include <core/core-action-manager.hpp>
#include <core/core-action-container.hpp>
#include <core/core-command.hpp>

#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>

#include <QAction>

#include <functional>

namespace TextEditor {
namespace Internal {

class TextEditorActionHandlerPrivate : public QObject {
  Q_DECLARE_TR_FUNCTIONS(TextEditor::Internal::TextEditorActionHandler)

public:
  TextEditorActionHandlerPrivate(Utils::Id editorId, Utils::Id contextId, uint optionalActions);

  auto registerActionHelper(Utils::Id id, bool scriptable, const QString &title, const QKeySequence &keySequence, Utils::Id menueGroup, Orca::Plugin::Core::ActionContainer *container, std::function<void(bool)> slot) -> QAction*
  {
    auto result = new QAction(title, this);
    Orca::Plugin::Core::Command *command = Orca::Plugin::Core::ActionManager::registerAction(result, id, Orca::Plugin::Core::Context(m_contextId), scriptable);
    if (!keySequence.isEmpty())
      command->setDefaultKeySequence(keySequence);

    if (container && menueGroup.isValid())
      container->addAction(command, menueGroup);

    connect(result, &QAction::triggered, slot);
    return result;
  }

  auto registerAction(Utils::Id id, std::function<void(TextEditorWidget *)> slot, bool scriptable = false, const QString &title = QString(), const QKeySequence &keySequence = QKeySequence(), Utils::Id menueGroup = Utils::Id(), Orca::Plugin::Core::ActionContainer *container = nullptr) -> QAction*
  {
    return registerActionHelper(id, scriptable, title, keySequence, menueGroup, container, [this, slot](bool) {
      if (m_currentEditorWidget)
        slot(m_currentEditorWidget);
    });
  }

  auto registerBoolAction(Utils::Id id, std::function<void(TextEditorWidget *, bool)> slot, bool scriptable = false, const QString &title = QString(), const QKeySequence &keySequence = QKeySequence(), Utils::Id menueGroup = Utils::Id(), Orca::Plugin::Core::ActionContainer *container = nullptr) -> QAction*
  {
    return registerActionHelper(id, scriptable, title, keySequence, menueGroup, container, [this, slot](bool on) {
      if (m_currentEditorWidget)
        slot(m_currentEditorWidget, on);
    });
  }

  auto registerIntAction(Utils::Id id, std::function<void(TextEditorWidget *, int)> slot, bool scriptable = false, const QString &title = QString(), const QKeySequence &keySequence = QKeySequence(), Utils::Id menueGroup = Utils::Id(), Orca::Plugin::Core::ActionContainer *container = nullptr) -> QAction*
  {
    return registerActionHelper(id, scriptable, title, keySequence, menueGroup, container, [this, slot](bool on) {
      if (m_currentEditorWidget)
        slot(m_currentEditorWidget, on);
    });
  }

  auto createActions() -> void;
  auto updateActions() -> void;
  auto updateOptionalActions() -> void;
  auto updateRedoAction(bool on) -> void;
  auto updateUndoAction(bool on) -> void;
  auto updateCopyAction(bool on) -> void;
  auto updateCurrentEditor(Orca::Plugin::Core::IEditor *editor) -> void;

  TextEditorActionHandler::TextEditorWidgetResolver m_findTextWidget;
  QAction *m_undoAction = nullptr;
  QAction *m_redoAction = nullptr;
  QAction *m_copyAction = nullptr;
  QAction *m_cutAction = nullptr;
  QAction *m_autoIndentAction = nullptr;
  QAction *m_autoFormatAction = nullptr;
  QAction *m_visualizeWhitespaceAction = nullptr;
  QAction *m_textWrappingAction = nullptr;
  QAction *m_unCommentSelectionAction = nullptr;
  QAction *m_unfoldAllAction = nullptr;
  QAction *m_followSymbolAction = nullptr;
  QAction *m_followSymbolInNextSplitAction = nullptr;
  QAction *m_renameSymbolAction = nullptr;
  QAction *m_jumpToFileAction = nullptr;
  QAction *m_jumpToFileInNextSplitAction = nullptr;
  QList<QAction*> m_modifyingActions;

  uint m_optionalActions = TextEditorActionHandler::None;
  QPointer<TextEditorWidget> m_currentEditorWidget;
  Utils::Id m_editorId;
  Utils::Id m_contextId;
};

TextEditorActionHandlerPrivate::TextEditorActionHandlerPrivate(Utils::Id editorId, Utils::Id contextId, uint optionalActions) : m_optionalActions(optionalActions), m_editorId(editorId), m_contextId(contextId)
{
  createActions();
  connect(Orca::Plugin::Core::EditorManager::instance(), &Orca::Plugin::Core::EditorManager::currentEditorChanged, this, &TextEditorActionHandlerPrivate::updateCurrentEditor);
}

auto TextEditorActionHandlerPrivate::createActions() -> void
{
  using namespace Orca::Plugin::Core;
  using namespace Constants;

  m_undoAction = registerAction(UNDO, [](TextEditorWidget *w) { w->undo(); }, true, tr("&Undo"));
  m_redoAction = registerAction(REDO, [](TextEditorWidget *w) { w->redo(); }, true, tr("&Redo"));
  m_copyAction = registerAction(COPY, [](TextEditorWidget *w) { w->copy(); }, true);
  m_cutAction = registerAction(CUT, [](TextEditorWidget *w) { w->cut(); }, true);
  m_modifyingActions << registerAction(PASTE, [](TextEditorWidget *w) { w->paste(); }, true);
  registerAction(SELECTALL, [](TextEditorWidget *w) { w->selectAll(); }, true);
  registerAction(GOTO, [](TextEditorWidget *) {
    QString locatorString = TextEditorPlugin::lineNumberFilter()->shortcutString();
    locatorString += QLatin1Char(' ');
    const int selectionStart = locatorString.size();
    locatorString += tr("<line>:<column>");
    Orca::Plugin::Core::LocatorManager::show(locatorString, selectionStart, locatorString.size() - selectionStart);
  });
  m_modifyingActions << registerAction(PRINT, [](TextEditorWidget *widget) { widget->print(Orca::Plugin::Core::ICore::printer()); });
  m_modifyingActions << registerAction(DELETE_LINE, [](TextEditorWidget *w) { w->deleteLine(); }, true, tr("Delete &Line"));
  m_modifyingActions << registerAction(DELETE_END_OF_LINE, [](TextEditorWidget *w) { w->deleteEndOfLine(); }, true, tr("Delete Line from Cursor On"));
  m_modifyingActions << registerAction(DELETE_END_OF_WORD, [](TextEditorWidget *w) { w->deleteEndOfWord(); }, true, tr("Delete Word from Cursor On"));
  m_modifyingActions << registerAction(DELETE_END_OF_WORD_CAMEL_CASE, [](TextEditorWidget *w) { w->deleteEndOfWordCamelCase(); }, true, tr("Delete Word Camel Case from Cursor On"));
  m_modifyingActions << registerAction(DELETE_START_OF_LINE, [](TextEditorWidget *w) { w->deleteStartOfLine(); }, true, tr("Delete Line up to Cursor"), Orca::Plugin::Core::use_mac_shortcuts ? QKeySequence(tr("Ctrl+Backspace")) : QKeySequence());
  m_modifyingActions << registerAction(DELETE_START_OF_WORD, [](TextEditorWidget *w) { w->deleteStartOfWord(); }, true, tr("Delete Word up to Cursor"));
  m_modifyingActions << registerAction(DELETE_START_OF_WORD_CAMEL_CASE, [](TextEditorWidget *w) { w->deleteStartOfWordCamelCase(); }, true, tr("Delete Word Camel Case up to Cursor"));
  registerAction(GOTO_BLOCK_START_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoBlockStartWithSelection(); }, true, tr("Go to Block Start with Selection"), QKeySequence(tr("Ctrl+{")));
  registerAction(GOTO_BLOCK_END_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoBlockEndWithSelection(); }, true, tr("Go to Block End with Selection"), QKeySequence(tr("Ctrl+}")));
  m_modifyingActions << registerAction(MOVE_LINE_UP, [](TextEditorWidget *w) { w->moveLineUp(); }, true, tr("Move Line Up"), QKeySequence(tr("Ctrl+Shift+Up")));
  m_modifyingActions << registerAction(MOVE_LINE_DOWN, [](TextEditorWidget *w) { w->moveLineDown(); }, true, tr("Move Line Down"), QKeySequence(tr("Ctrl+Shift+Down")));
  m_modifyingActions << registerAction(COPY_LINE_UP, [](TextEditorWidget *w) { w->copyLineUp(); }, true, tr("Copy Line Up"), QKeySequence(tr("Ctrl+Alt+Up")));
  m_modifyingActions << registerAction(COPY_LINE_DOWN, [](TextEditorWidget *w) { w->copyLineDown(); }, true, tr("Copy Line Down"), QKeySequence(tr("Ctrl+Alt+Down")));
  m_modifyingActions << registerAction(JOIN_LINES, [](TextEditorWidget *w) { w->joinLines(); }, true, tr("Join Lines"), QKeySequence(tr("Ctrl+J")));
  m_modifyingActions << registerAction(INSERT_LINE_ABOVE, [](TextEditorWidget *w) { w->insertLineAbove(); }, true, tr("Insert Line Above Current Line"), QKeySequence(tr("Ctrl+Shift+Return")));
  m_modifyingActions << registerAction(INSERT_LINE_BELOW, [](TextEditorWidget *w) { w->insertLineBelow(); }, true, tr("Insert Line Below Current Line"), QKeySequence(tr("Ctrl+Return")));
  m_modifyingActions << registerAction(SWITCH_UTF8BOM, [](TextEditorWidget *w) { w->switchUtf8bom(); }, true, tr("Toggle UTF-8 BOM"));
  m_modifyingActions << registerAction(INDENT, [](TextEditorWidget *w) { w->indent(); }, true, tr("Indent"));
  m_modifyingActions << registerAction(UNINDENT, [](TextEditorWidget *w) { w->unindent(); }, true, tr("Unindent"));
  m_followSymbolAction = registerAction(FOLLOW_SYMBOL_UNDER_CURSOR, [](TextEditorWidget *w) { w->openLinkUnderCursor(); }, true, tr("Follow Symbol Under Cursor"), QKeySequence(Qt::Key_F2));
  m_followSymbolInNextSplitAction = registerAction(FOLLOW_SYMBOL_UNDER_CURSOR_IN_NEXT_SPLIT, [](TextEditorWidget *w) { w->openLinkUnderCursorInNextSplit(); }, true, tr("Follow Symbol Under Cursor in Next Split"), QKeySequence(Utils::HostOsInfo::isMacHost() ? tr("Meta+E, F2") : tr("Ctrl+E, F2")));
  registerAction(FIND_USAGES, [](TextEditorWidget *w) { w->findUsages(); }, true, tr("Find References to Symbol Under Cursor"), QKeySequence(tr("Ctrl+Shift+U")));
  m_renameSymbolAction = registerAction(RENAME_SYMBOL, [](TextEditorWidget *w) { w->renameSymbolUnderCursor(); }, true, tr("Rename Symbol Under Cursor"), QKeySequence(tr("Ctrl+Shift+R")));
  m_jumpToFileAction = registerAction(JUMP_TO_FILE_UNDER_CURSOR, [](TextEditorWidget *w) { w->openLinkUnderCursor(); }, true, tr("Jump to File Under Cursor"), QKeySequence(Qt::Key_F2));
  m_jumpToFileInNextSplitAction = registerAction(JUMP_TO_FILE_UNDER_CURSOR_IN_NEXT_SPLIT, [](TextEditorWidget *w) { w->openLinkUnderCursorInNextSplit(); }, true, tr("Jump to File Under Cursor in Next Split"), QKeySequence(Utils::HostOsInfo::isMacHost() ? tr("Meta+E, F2") : tr("Ctrl+E, F2")).toString());

  registerAction(VIEW_PAGE_UP, [](TextEditorWidget *w) { w->viewPageUp(); }, true, tr("Move the View a Page Up and Keep the Cursor Position"), QKeySequence(tr("Ctrl+PgUp")));
  registerAction(VIEW_PAGE_DOWN, [](TextEditorWidget *w) { w->viewPageDown(); }, true, tr("Move the View a Page Down and Keep the Cursor Position"), QKeySequence(tr("Ctrl+PgDown")));
  registerAction(VIEW_LINE_UP, [](TextEditorWidget *w) { w->viewLineUp(); }, true, tr("Move the View a Line Up and Keep the Cursor Position"), QKeySequence(tr("Ctrl+Up")));
  registerAction(VIEW_LINE_DOWN, [](TextEditorWidget *w) { w->viewLineDown(); }, true, tr("Move the View a Line Down and Keep the Cursor Position"), QKeySequence(tr("Ctrl+Down")));

  // register "Edit" Menu Actions
  Orca::Plugin::Core::ActionContainer *editMenu = Orca::Plugin::Core::ActionManager::actionContainer(M_EDIT);
  registerAction(SELECT_ENCODING, [](TextEditorWidget *w) { w->selectEncoding(); }, false, tr("Select Encoding..."), QKeySequence(), G_EDIT_OTHER, editMenu);
  m_modifyingActions << registerAction(CIRCULAR_PASTE, [](TextEditorWidget *w) { w->circularPaste(); }, false, tr("Paste from Clipboard History"), QKeySequence(tr("Ctrl+Shift+V")), G_EDIT_COPYPASTE, editMenu);
  m_modifyingActions << registerAction(NO_FORMAT_PASTE, [](TextEditorWidget *w) { w->pasteWithoutFormat(); }, false, tr("Paste Without Formatting"), QKeySequence(Orca::Plugin::Core::use_mac_shortcuts ? tr("Ctrl+Alt+Shift+V") : QString()), G_EDIT_COPYPASTE, editMenu);

  // register "Edit -> Advanced" Menu Actions
  Orca::Plugin::Core::ActionContainer *advancedEditMenu = Orca::Plugin::Core::ActionManager::actionContainer(M_EDIT_ADVANCED);
  m_autoIndentAction = registerAction(AUTO_INDENT_SELECTION, [](TextEditorWidget *w) { w->autoIndent(); }, true, tr("Auto-&indent Selection"), QKeySequence(tr("Ctrl+I")), G_EDIT_FORMAT, advancedEditMenu);
  m_autoFormatAction = registerAction(AUTO_FORMAT_SELECTION, [](TextEditorWidget *w) { w->autoFormat(); }, true, tr("Auto-&format Selection"), QKeySequence(tr("Ctrl+;")), G_EDIT_FORMAT, advancedEditMenu);
  m_modifyingActions << registerAction(REWRAP_PARAGRAPH, [](TextEditorWidget *w) { w->rewrapParagraph(); }, true, tr("&Rewrap Paragraph"), QKeySequence(Orca::Plugin::Core::use_mac_shortcuts ? tr("Meta+E, R") : tr("Ctrl+E, R")), G_EDIT_FORMAT, advancedEditMenu);
  m_visualizeWhitespaceAction = registerBoolAction(VISUALIZE_WHITESPACE, [](TextEditorWidget *widget, bool checked) {
    if (widget) {
      auto ds = widget->displaySettings();
      ds.m_visualizeWhitespace = checked;
      widget->setDisplaySettings(ds);
    }
  }, false, tr("&Visualize Whitespace"), QKeySequence(Orca::Plugin::Core::use_mac_shortcuts ? tr("Meta+E, Meta+V") : tr("Ctrl+E, Ctrl+V")), G_EDIT_FORMAT, advancedEditMenu);
  m_visualizeWhitespaceAction->setCheckable(true);
  m_modifyingActions << registerAction(CLEAN_WHITESPACE, [](TextEditorWidget *w) { w->cleanWhitespace(); }, true, tr("Clean Whitespace"), QKeySequence(), G_EDIT_FORMAT, advancedEditMenu);
  m_textWrappingAction = registerBoolAction(TEXT_WRAPPING, [](TextEditorWidget *widget, bool checked) {
    if (widget) {
      auto ds = widget->displaySettings();
      ds.m_textWrapping = checked;
      widget->setDisplaySettings(ds);
    }
  }, false, tr("Enable Text &Wrapping"), QKeySequence(Orca::Plugin::Core::use_mac_shortcuts ? tr("Meta+E, Meta+W") : tr("Ctrl+E, Ctrl+W")), G_EDIT_FORMAT, advancedEditMenu);
  m_textWrappingAction->setCheckable(true);
  m_unCommentSelectionAction = registerAction(UN_COMMENT_SELECTION, [](TextEditorWidget *w) { w->unCommentSelection(); }, true, tr("Toggle Comment &Selection"), QKeySequence(tr("Ctrl+/")), G_EDIT_FORMAT, advancedEditMenu);
  m_modifyingActions << registerAction(CUT_LINE, [](TextEditorWidget *w) { w->cutLine(); }, true, tr("Cut &Line"), QKeySequence(tr("Shift+Del")), G_EDIT_TEXT, advancedEditMenu);
  registerAction(COPY_LINE, [](TextEditorWidget *w) { w->copyLine(); }, false, tr("Copy &Line"), QKeySequence(tr("Ctrl+Ins")), G_EDIT_TEXT, advancedEditMenu);
  m_modifyingActions << registerAction(DUPLICATE_SELECTION, [](TextEditorWidget *w) { w->duplicateSelection(); }, false, tr("&Duplicate Selection"), QKeySequence(), G_EDIT_TEXT, advancedEditMenu);
  m_modifyingActions << registerAction(DUPLICATE_SELECTION_AND_COMMENT, [](TextEditorWidget *w) { w->duplicateSelectionAndComment(); }, false, tr("&Duplicate Selection and Comment"), QKeySequence(), G_EDIT_TEXT, advancedEditMenu);
  m_modifyingActions << registerAction(UPPERCASE_SELECTION, [](TextEditorWidget *w) { w->uppercaseSelection(); }, true, tr("Uppercase Selection"), QKeySequence(Orca::Plugin::Core::use_mac_shortcuts ? tr("Meta+Shift+U") : tr("Alt+Shift+U")), G_EDIT_TEXT, advancedEditMenu);
  m_modifyingActions << registerAction(LOWERCASE_SELECTION, [](TextEditorWidget *w) { w->lowercaseSelection(); }, true, tr("Lowercase Selection"), QKeySequence(Orca::Plugin::Core::use_mac_shortcuts ? tr("Meta+U") : tr("Alt+U")), G_EDIT_TEXT, advancedEditMenu);
  m_modifyingActions << registerAction(SORT_SELECTED_LINES, [](TextEditorWidget *w) { w->sortSelectedLines(); }, false, tr("&Sort Selected Lines"), QKeySequence(Orca::Plugin::Core::use_mac_shortcuts ? tr("Meta+Shift+S") : tr("Alt+Shift+S")), G_EDIT_TEXT, advancedEditMenu);
  registerAction(FOLD, [](TextEditorWidget *w) { w->fold(); }, true, tr("Fold"), QKeySequence(tr("Ctrl+<")), G_EDIT_COLLAPSING, advancedEditMenu);
  registerAction(UNFOLD, [](TextEditorWidget *w) { w->unfold(); }, true, tr("Unfold"), QKeySequence(tr("Ctrl+>")), G_EDIT_COLLAPSING, advancedEditMenu);
  m_unfoldAllAction = registerAction(UNFOLD_ALL, [](TextEditorWidget *w) { w->unfoldAll(); }, true, tr("Toggle &Fold All"), QKeySequence(), G_EDIT_COLLAPSING, advancedEditMenu);
  registerAction(INCREASE_FONT_SIZE, [](TextEditorWidget *w) { w->zoomF(1.f); }, false, tr("Increase Font Size"), QKeySequence(tr("Ctrl++")), G_EDIT_FONT, advancedEditMenu);
  registerAction(DECREASE_FONT_SIZE, [](TextEditorWidget *w) { w->zoomF(-1.f); }, false, tr("Decrease Font Size"), QKeySequence(tr("Ctrl+-")), G_EDIT_FONT, advancedEditMenu);
  registerAction(RESET_FONT_SIZE, [](TextEditorWidget *w) { w->zoomReset(); }, false, tr("Reset Font Size"), QKeySequence(Orca::Plugin::Core::use_mac_shortcuts ? tr("Meta+0") : tr("Ctrl+0")), G_EDIT_FONT, advancedEditMenu);
  registerAction(GOTO_BLOCK_START, [](TextEditorWidget *w) { w->gotoBlockStart(); }, true, tr("Go to Block Start"), QKeySequence(tr("Ctrl+[")), G_EDIT_BLOCKS, advancedEditMenu);
  registerAction(GOTO_BLOCK_END, [](TextEditorWidget *w) { w->gotoBlockEnd(); }, true, tr("Go to Block End"), QKeySequence(tr("Ctrl+]")), G_EDIT_BLOCKS, advancedEditMenu);
  registerAction(SELECT_BLOCK_UP, [](TextEditorWidget *w) { w->selectBlockUp(); }, true, tr("Select Block Up"), QKeySequence(tr("Ctrl+U")), G_EDIT_BLOCKS, advancedEditMenu);
  registerAction(SELECT_BLOCK_DOWN, [](TextEditorWidget *w) { w->selectBlockDown(); }, true, tr("Select Block Down"), QKeySequence(tr("Ctrl+Shift+Alt+U")), G_EDIT_BLOCKS, advancedEditMenu);
  registerAction(SELECT_WORD_UNDER_CURSOR, [](TextEditorWidget *w) { w->selectWordUnderCursor(); }, true, tr("Select Word Under Cursor"));

  // register GOTO Actions
  registerAction(GOTO_DOCUMENT_START, [](TextEditorWidget *w) { w->gotoDocumentStart(); }, true, tr("Go to Document Start"));
  registerAction(GOTO_DOCUMENT_END, [](TextEditorWidget *w) { w->gotoDocumentEnd(); }, true, tr("Go to Document End"));
  registerAction(GOTO_LINE_START, [](TextEditorWidget *w) { w->gotoLineStart(); }, true, tr("Go to Line Start"));
  registerAction(GOTO_LINE_END, [](TextEditorWidget *w) { w->gotoLineEnd(); }, true, tr("Go to Line End"));
  registerAction(GOTO_NEXT_LINE, [](TextEditorWidget *w) { w->gotoNextLine(); }, true, tr("Go to Next Line"));
  registerAction(GOTO_PREVIOUS_LINE, [](TextEditorWidget *w) { w->gotoPreviousLine(); }, true, tr("Go to Previous Line"));
  registerAction(GOTO_PREVIOUS_CHARACTER, [](TextEditorWidget *w) { w->gotoPreviousCharacter(); }, true, tr("Go to Previous Character"));
  registerAction(GOTO_NEXT_CHARACTER, [](TextEditorWidget *w) { w->gotoNextCharacter(); }, true, tr("Go to Next Character"));
  registerAction(GOTO_PREVIOUS_WORD, [](TextEditorWidget *w) { w->gotoPreviousWord(); }, true, tr("Go to Previous Word"));
  registerAction(GOTO_NEXT_WORD, [](TextEditorWidget *w) { w->gotoNextWord(); }, true, tr("Go to Next Word"));
  registerAction(GOTO_PREVIOUS_WORD_CAMEL_CASE, [](TextEditorWidget *w) { w->gotoPreviousWordCamelCase(); }, false, tr("Go to Previous Word Camel Case"));
  registerAction(GOTO_NEXT_WORD_CAMEL_CASE, [](TextEditorWidget *w) { w->gotoNextWordCamelCase(); }, false, tr("Go to Next Word Camel Case"));

  // register GOTO actions with selection
  registerAction(GOTO_LINE_START_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoLineStartWithSelection(); }, true, tr("Go to Line Start with Selection"));
  registerAction(GOTO_LINE_END_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoLineEndWithSelection(); }, true, tr("Go to Line End with Selection"));
  registerAction(GOTO_NEXT_LINE_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoNextLineWithSelection(); }, true, tr("Go to Next Line with Selection"));
  registerAction(GOTO_PREVIOUS_LINE_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoPreviousLineWithSelection(); }, true, tr("Go to Previous Line with Selection"));
  registerAction(GOTO_PREVIOUS_CHARACTER_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoPreviousCharacterWithSelection(); }, true, tr("Go to Previous Character with Selection"));
  registerAction(GOTO_NEXT_CHARACTER_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoNextCharacterWithSelection(); }, true, tr("Go to Next Character with Selection"));
  registerAction(GOTO_PREVIOUS_WORD_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoPreviousWordWithSelection(); }, true, tr("Go to Previous Word with Selection"));
  registerAction(GOTO_NEXT_WORD_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoNextWordWithSelection(); }, true, tr("Go to Next Word with Selection"));
  registerAction(GOTO_PREVIOUS_WORD_CAMEL_CASE_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoPreviousWordCamelCaseWithSelection(); }, false, tr("Go to Previous Word Camel Case with Selection"));
  registerAction(GOTO_NEXT_WORD_CAMEL_CASE_WITH_SELECTION, [](TextEditorWidget *w) { w->gotoNextWordCamelCaseWithSelection(); }, false, tr("Go to Next Word Camel Case with Selection"));

  // Collect additional modifying actions so we can check for them inside a readonly file
  // and disable them
  m_modifyingActions << m_autoIndentAction;
  m_modifyingActions << m_autoFormatAction;
  m_modifyingActions << m_unCommentSelectionAction;

  updateOptionalActions();
}

auto TextEditorActionHandlerPrivate::updateActions() -> void
{
  const auto isWritable = m_currentEditorWidget && !m_currentEditorWidget->isReadOnly();
  foreach(QAction *a, m_modifyingActions)
    a->setEnabled(isWritable);
  m_unCommentSelectionAction->setEnabled(m_optionalActions & TextEditorActionHandler::UnCommentSelection && isWritable);
  m_visualizeWhitespaceAction->setEnabled(m_currentEditorWidget);
  m_textWrappingAction->setEnabled(m_currentEditorWidget);
  if (m_currentEditorWidget) {
    m_visualizeWhitespaceAction->setChecked(m_currentEditorWidget->displaySettings().m_visualizeWhitespace);
    m_textWrappingAction->setChecked(m_currentEditorWidget->displaySettings().m_textWrapping);
  }

  updateRedoAction(m_currentEditorWidget && m_currentEditorWidget->document()->isRedoAvailable());
  updateUndoAction(m_currentEditorWidget && m_currentEditorWidget->document()->isUndoAvailable());
  updateCopyAction(m_currentEditorWidget && m_currentEditorWidget->textCursor().hasSelection());
  updateOptionalActions();
}

auto TextEditorActionHandlerPrivate::updateOptionalActions() -> void
{
  const auto optionalActions = m_currentEditorWidget ? m_currentEditorWidget->optionalActions() : m_optionalActions;
  m_followSymbolAction->setEnabled(optionalActions & TextEditorActionHandler::FollowSymbolUnderCursor);
  m_followSymbolInNextSplitAction->setEnabled(optionalActions & TextEditorActionHandler::FollowSymbolUnderCursor);
  m_jumpToFileAction->setEnabled(optionalActions & TextEditorActionHandler::JumpToFileUnderCursor);
  m_jumpToFileInNextSplitAction->setEnabled(optionalActions & TextEditorActionHandler::JumpToFileUnderCursor);
  m_unfoldAllAction->setEnabled(optionalActions & TextEditorActionHandler::UnCollapseAll);
  m_renameSymbolAction->setEnabled(optionalActions & TextEditorActionHandler::RenameSymbol);

  const auto formatEnabled = optionalActions & TextEditorActionHandler::Format && m_currentEditorWidget && !m_currentEditorWidget->isReadOnly();
  m_autoIndentAction->setEnabled(formatEnabled);
  m_autoFormatAction->setEnabled(formatEnabled);
}

auto TextEditorActionHandlerPrivate::updateRedoAction(bool on) -> void
{
  m_redoAction->setEnabled(on);
}

auto TextEditorActionHandlerPrivate::updateUndoAction(bool on) -> void
{
  m_undoAction->setEnabled(on);
}

auto TextEditorActionHandlerPrivate::updateCopyAction(bool hasCopyableText) -> void
{
  if (m_cutAction)
    m_cutAction->setEnabled(hasCopyableText && m_currentEditorWidget && !m_currentEditorWidget->isReadOnly());
  if (m_copyAction)
    m_copyAction->setEnabled(hasCopyableText);
}

auto TextEditorActionHandlerPrivate::updateCurrentEditor(Orca::Plugin::Core::IEditor *editor) -> void
{
  if (m_currentEditorWidget)
    m_currentEditorWidget->disconnect(this);
  m_currentEditorWidget = nullptr;

  if (editor && editor->document()->id() == m_editorId) {
    const auto editorWidget = m_findTextWidget(editor);
    QTC_ASSERT(editorWidget, return); // editor has our id, so shouldn't happen
    m_currentEditorWidget = editorWidget;
    connect(editorWidget, &QPlainTextEdit::undoAvailable, this, &TextEditorActionHandlerPrivate::updateUndoAction);
    connect(editorWidget, &QPlainTextEdit::redoAvailable, this, &TextEditorActionHandlerPrivate::updateRedoAction);
    connect(editorWidget, &QPlainTextEdit::copyAvailable, this, &TextEditorActionHandlerPrivate::updateCopyAction);
    connect(editorWidget, &TextEditorWidget::readOnlyChanged, this, &TextEditorActionHandlerPrivate::updateActions);
    connect(editorWidget, &TextEditorWidget::optionalActionMaskChanged, this, &TextEditorActionHandlerPrivate::updateOptionalActions);
  }
  updateActions();
}

} // namespace Internal

TextEditorActionHandler::TextEditorActionHandler(Utils::Id editorId, Utils::Id contextId, uint optionalActions, const TextEditorWidgetResolver &resolver) : d(new Internal::TextEditorActionHandlerPrivate(editorId, contextId, optionalActions))
{
  if (resolver)
    d->m_findTextWidget = resolver;
  else
    d->m_findTextWidget = TextEditorWidget::fromEditor;
}

auto TextEditorActionHandler::optionalActions() const -> uint
{
  return d->m_optionalActions;
}

TextEditorActionHandler::~TextEditorActionHandler()
{
  delete d;
}

} // namespace TextEditor
