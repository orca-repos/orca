// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include "texteditor_global.hpp"

#include <QtGlobal>

namespace TextEditor {

// Text color and style categories
enum TextStyle : quint8 {
  C_TEXT,
  C_LINK,
  C_SELECTION,
  C_LINE_NUMBER,
  C_SEARCH_RESULT,
  C_SEARCH_RESULT_ALT1,
  C_SEARCH_RESULT_ALT2,
  C_SEARCH_SCOPE,
  C_PARENTHESES,
  C_PARENTHESES_MISMATCH,
  C_AUTOCOMPLETE,
  C_CURRENT_LINE,
  C_CURRENT_LINE_NUMBER,
  C_OCCURRENCES,
  C_OCCURRENCES_UNUSED,
  C_OCCURRENCES_RENAME,
  C_NUMBER,
  C_STRING,
  C_TYPE,
  C_NAMESPACE,
  C_LOCAL,
  C_PARAMETER,
  C_GLOBAL,
  C_FIELD,
  C_ENUMERATION,
  C_VIRTUAL_METHOD,
  C_FUNCTION,
  C_KEYWORD,
  C_PRIMITIVE_TYPE,
  C_OPERATOR,
  C_OVERLOADED_OPERATOR,
  C_PUNCTUATION,
  C_PREPROCESSOR,
  C_LABEL,
  C_COMMENT,
  C_DOXYGEN_COMMENT,
  C_DOXYGEN_TAG,
  C_VISUAL_WHITESPACE,
  C_QML_LOCAL_ID,
  C_QML_EXTERNAL_ID,
  C_QML_TYPE_ID,
  C_QML_ROOT_OBJECT_PROPERTY,
  C_QML_SCOPE_OBJECT_PROPERTY,
  C_QML_EXTERNAL_OBJECT_PROPERTY,
  C_JS_SCOPE_VAR,
  C_JS_IMPORT_VAR,
  C_JS_GLOBAL_VAR,
  C_QML_STATE_NAME,
  C_BINDING,
  C_DISABLED_CODE,
  C_ADDED_LINE,
  C_REMOVED_LINE,
  C_DIFF_FILE,
  C_DIFF_LOCATION,
  C_DIFF_FILE_LINE,
  C_DIFF_CONTEXT_LINE,
  C_DIFF_SOURCE_LINE,
  C_DIFF_SOURCE_CHAR,
  C_DIFF_DEST_LINE,
  C_DIFF_DEST_CHAR,
  C_LOG_CHANGE_LINE,
  C_LOG_AUTHOR_NAME,
  C_LOG_COMMIT_DATE,
  C_LOG_COMMIT_HASH,
  C_LOG_COMMIT_SUBJECT,
  C_LOG_DECORATION,
  C_WARNING,
  C_WARNING_CONTEXT,
  C_ERROR,
  C_ERROR_CONTEXT,
  C_DECLARATION,
  C_FUNCTION_DEFINITION,
  C_OUTPUT_ARGUMENT,
  C_STATIC_MEMBER,
  C_LAST_STYLE_SENTINEL
};

namespace Constants {

constexpr char C_TEXTEDITOR[]                                 = "Text Editor";
constexpr char M_STANDARDCONTEXTMENU[]                        = "TextEditor.StandardContextMenu";
constexpr char G_UNDOREDO[]                                   = "TextEditor.UndoRedoGroup";
constexpr char G_COPYPASTE[]                                  = "TextEditor.CopyPasteGroup";
constexpr char G_SELECT[]                                     = "TextEditor.SelectGroup";
constexpr char G_BOM[]                                        = "TextEditor.BomGroup";
constexpr char COMPLETE_THIS[]                                = "TextEditor.CompleteThis";
constexpr char FUNCTION_HINT[]                                = "TextEditor.FunctionHint";
constexpr char QUICKFIX_THIS[]                                = "TextEditor.QuickFix";
constexpr char SHOWCONTEXTMENU[]                              = "TextEditor.ShowContextMenu";
constexpr char CREATE_SCRATCH_BUFFER[]                        = "TextEditor.CreateScratchBuffer";
constexpr char VISUALIZE_WHITESPACE[]                         = "TextEditor.VisualizeWhitespace";
constexpr char CLEAN_WHITESPACE[]                             = "TextEditor.CleanWhitespace";
constexpr char TEXT_WRAPPING[]                                = "TextEditor.TextWrapping";
constexpr char UN_COMMENT_SELECTION[]                         = "TextEditor.UnCommentSelection";
constexpr char FOLD[]                                         = "TextEditor.Fold";
constexpr char UNFOLD[]                                       = "TextEditor.Unfold";
constexpr char UNFOLD_ALL[]                                   = "TextEditor.UnFoldAll";
constexpr char AUTO_INDENT_SELECTION[]                        = "TextEditor.AutoIndentSelection";
constexpr char AUTO_FORMAT_SELECTION[]                        = "TextEditor.AutoFormatSelection";
constexpr char INCREASE_FONT_SIZE[]                           = "TextEditor.IncreaseFontSize";
constexpr char DECREASE_FONT_SIZE[]                           = "TextEditor.DecreaseFontSize";
constexpr char RESET_FONT_SIZE[]                              = "TextEditor.ResetFontSize";
constexpr char GOTO_BLOCK_START[]                             = "TextEditor.GotoBlockStart";
constexpr char GOTO_BLOCK_START_WITH_SELECTION[]              = "TextEditor.GotoBlockStartWithSelection";
constexpr char GOTO_BLOCK_END[]                               = "TextEditor.GotoBlockEnd";
constexpr char GOTO_BLOCK_END_WITH_SELECTION[]                = "TextEditor.GotoBlockEndWithSelection";
constexpr char SELECT_BLOCK_UP[]                              = "TextEditor.SelectBlockUp";
constexpr char SELECT_BLOCK_DOWN[]                            = "TextEditor.SelectBlockDown";
constexpr char SELECT_WORD_UNDER_CURSOR[]                     = "TextEditor.SelectWordUnderCursor";
constexpr char VIEW_PAGE_UP[]                                 = "TextEditor.viewPageUp";
constexpr char VIEW_PAGE_DOWN[]                               = "TextEditor.viewPageDown";
constexpr char VIEW_LINE_UP[]                                 = "TextEditor.viewLineUp";
constexpr char VIEW_LINE_DOWN[]                               = "TextEditor.viewLineDown";
constexpr char MOVE_LINE_UP[]                                 = "TextEditor.MoveLineUp";
constexpr char MOVE_LINE_DOWN[]                               = "TextEditor.MoveLineDown";
constexpr char COPY_LINE_UP[]                                 = "TextEditor.CopyLineUp";
constexpr char COPY_LINE_DOWN[]                               = "TextEditor.CopyLineDown";
constexpr char JOIN_LINES[]                                   = "TextEditor.JoinLines";
constexpr char INSERT_LINE_ABOVE[]                            = "TextEditor.InsertLineAboveCurrentLine";
constexpr char INSERT_LINE_BELOW[]                            = "TextEditor.InsertLineBelowCurrentLine";
constexpr char UPPERCASE_SELECTION[]                          = "TextEditor.UppercaseSelection";
constexpr char LOWERCASE_SELECTION[]                          = "TextEditor.LowercaseSelection";
constexpr char SORT_SELECTED_LINES[]                          = "TextEditor.SortSelectedLines";
constexpr char CUT_LINE[]                                     = "TextEditor.CutLine";
constexpr char COPY_LINE[]                                    = "TextEditor.CopyLine";
constexpr char DUPLICATE_SELECTION[]                          = "TextEditor.DuplicateSelection";
constexpr char DUPLICATE_SELECTION_AND_COMMENT[]              = "TextEditor.DuplicateSelectionAndComment";
constexpr char DELETE_LINE[]                                  = "TextEditor.DeleteLine";
constexpr char DELETE_END_OF_WORD[]                           = "TextEditor.DeleteEndOfWord";
constexpr char DELETE_END_OF_LINE[]                           = "TextEditor.DeleteEndOfLine";
constexpr char DELETE_END_OF_WORD_CAMEL_CASE[]                = "TextEditor.DeleteEndOfWordCamelCase";
constexpr char DELETE_START_OF_WORD[]                         = "TextEditor.DeleteStartOfWord";
constexpr char DELETE_START_OF_LINE[]                         = "TextEditor.DeleteStartOfLine";
constexpr char DELETE_START_OF_WORD_CAMEL_CASE[]              = "TextEditor.DeleteStartOfWordCamelCase";
constexpr char SELECT_ENCODING[]                              = "TextEditor.SelectEncoding";
constexpr char REWRAP_PARAGRAPH[]                             =  "TextEditor.RewrapParagraph";
constexpr char GOTO_DOCUMENT_START[]                          = "TextEditor.GotoDocumentStart";
constexpr char GOTO_DOCUMENT_END[]                            = "TextEditor.GotoDocumentEnd";
constexpr char GOTO_LINE_START[]                              = "TextEditor.GotoLineStart";
constexpr char GOTO_LINE_END[]                                = "TextEditor.GotoLineEnd";
constexpr char GOTO_NEXT_LINE[]                               = "TextEditor.GotoNextLine";
constexpr char GOTO_PREVIOUS_LINE[]                           = "TextEditor.GotoPreviousLine";
constexpr char GOTO_PREVIOUS_CHARACTER[]                      = "TextEditor.GotoPreviousCharacter";
constexpr char GOTO_NEXT_CHARACTER[]                          = "TextEditor.GotoNextCharacter";
constexpr char GOTO_PREVIOUS_WORD[]                           = "TextEditor.GotoPreviousWord";
constexpr char GOTO_NEXT_WORD[]                               = "TextEditor.GotoNextWord";
constexpr char GOTO_PREVIOUS_WORD_CAMEL_CASE[]                = "TextEditor.GotoPreviousWordCamelCase";
constexpr char GOTO_NEXT_WORD_CAMEL_CASE[]                    = "TextEditor.GotoNextWordCamelCase";
constexpr char GOTO_LINE_START_WITH_SELECTION[]               = "TextEditor.GotoLineStartWithSelection";
constexpr char GOTO_LINE_END_WITH_SELECTION[]                 = "TextEditor.GotoLineEndWithSelection";
constexpr char GOTO_NEXT_LINE_WITH_SELECTION[]                = "TextEditor.GotoNextLineWithSelection";
constexpr char GOTO_PREVIOUS_LINE_WITH_SELECTION[]            = "TextEditor.GotoPreviousLineWithSelection";
constexpr char GOTO_PREVIOUS_CHARACTER_WITH_SELECTION[]       = "TextEditor.GotoPreviousCharacterWithSelection";
constexpr char GOTO_NEXT_CHARACTER_WITH_SELECTION[]           = "TextEditor.GotoNextCharacterWithSelection";
constexpr char GOTO_PREVIOUS_WORD_WITH_SELECTION[]            = "TextEditor.GotoPreviousWordWithSelection";
constexpr char GOTO_NEXT_WORD_WITH_SELECTION[]                = "TextEditor.GotoNextWordWithSelection";
constexpr char GOTO_PREVIOUS_WORD_CAMEL_CASE_WITH_SELECTION[] = "TextEditor.GotoPreviousWordCamelCaseWithSelection";
constexpr char GOTO_NEXT_WORD_CAMEL_CASE_WITH_SELECTION[]     = "TextEditor.GotoNextWordCamelCaseWithSelection";
constexpr char C_TEXTEDITOR_MIMETYPE_TEXT[]                   = "text/plain";
constexpr char INFO_MISSING_SYNTAX_DEFINITION[]               = "TextEditor.InfoSyntaxDefinition";
constexpr char INFO_MULTIPLE_SYNTAX_DEFINITIONS[]             = "TextEditor.InfoMultipleSyntaxDefinitions";
constexpr char TASK_OPEN_FILE[]                               = "TextEditor.Task.OpenFile";
constexpr char CIRCULAR_PASTE[]                               = "TextEditor.CircularPaste";
constexpr char NO_FORMAT_PASTE[]                              = "TextEditor.NoFormatPaste";
constexpr char SWITCH_UTF8BOM[]                               = "TextEditor.SwitchUtf8bom";
constexpr char INDENT[]                                       = "TextEditor.Indent";
constexpr char UNINDENT[]                                     = "TextEditor.Unindent";
constexpr char FOLLOW_SYMBOL_UNDER_CURSOR[]                   = "TextEditor.FollowSymbolUnderCursor";
constexpr char FOLLOW_SYMBOL_UNDER_CURSOR_IN_NEXT_SPLIT[]     = "TextEditor.FollowSymbolUnderCursorInNextSplit";
constexpr char FIND_USAGES[]                                  = "TextEditor.FindUsages";

// moved from CppEditor to TextEditor avoid breaking the setting by using the old key
constexpr char RENAME_SYMBOL[]                                = "CppEditor.RenameSymbolUnderCursor";
constexpr char JUMP_TO_FILE_UNDER_CURSOR[]                    = "TextEditor.JumpToFileUnderCursor";
constexpr char JUMP_TO_FILE_UNDER_CURSOR_IN_NEXT_SPLIT[]      = "TextEditor.JumpToFileUnderCursorInNextSplit";
constexpr char SCROLL_BAR_SEARCH_RESULT[]                     = "TextEditor.ScrollBarSearchResult";
constexpr char SCROLL_BAR_CURRENT_LINE[]                      = "TextEditor.ScrollBarCurrentLine";

TEXTEDITOR_EXPORT auto nameForStyle(TextStyle style) -> const char*;
auto styleFromName(const char *name) -> TextStyle;

constexpr char TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH[]      =  ":/texteditor/images/settingscategory_texteditor.png";
constexpr char TEXT_EDITOR_SETTINGS_CATEGORY[]                = "C.TextEditor";
constexpr char TEXT_EDITOR_FONT_SETTINGS[]                    = "A.FontSettings";
constexpr char TEXT_EDITOR_BEHAVIOR_SETTINGS[]                = "B.BehaviourSettings";
constexpr char TEXT_EDITOR_DISPLAY_SETTINGS[]                 = "D.DisplaySettings";
constexpr char TEXT_EDITOR_HIGHLIGHTER_SETTINGS[]             = "E.HighlighterSettings";
constexpr char TEXT_EDITOR_SNIPPETS_SETTINGS[]                = "F.SnippetsSettings";
constexpr char HIGHLIGHTER_SETTINGS_CATEGORY[]                = "HighlighterSettings";
constexpr char SNIPPET_EDITOR_ID[]                            = "TextEditor.SnippetEditor";
constexpr char TEXT_SNIPPET_GROUP_ID[]                        = "Text";
constexpr char GLOBAL_SETTINGS_ID[]                           = "Global";
constexpr char GENERIC_PROPOSAL_ID[]                          = "TextEditor.GenericProposalId";

/**
 * Delay before tooltip will be shown near completion assistant proposal
 */
const unsigned COMPLETION_ASSIST_TOOLTIP_DELAY = 100;

} // namespace Constants
} // namespace TextEditor
