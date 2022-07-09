// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cppcodestylesettings.hpp"

#include <texteditor/tabsettings.hpp>

#include <cplusplus/SimpleLexer.h>

#include <QStack>
#include <QList>
#include <QVector>

QT_BEGIN_NAMESPACE
class QTextDocument;
class QTextBlock;
QT_END_NAMESPACE

namespace CppEditor {
namespace Internal {
class CppCodeFormatterData;
}

class CPPEDITOR_EXPORT CodeFormatter {
  Q_GADGET

public:
  virtual ~CodeFormatter();

  // updates all states up until block if necessary
  // it is safe to call indentFor on block afterwards
  auto updateStateUntil(const QTextBlock &block) -> void;
  // calculates the state change introduced by changing a single line
  auto updateLineStateChange(const QTextBlock &block) -> void;
  auto isInRawStringLiteral(const QTextBlock &block) const -> bool;
  auto indentFor(const QTextBlock &block, int *indent, int *padding) -> void;
  auto indentForNewLineAfter(const QTextBlock &block, int *indent, int *padding) -> void;
  auto setTabSize(int tabSize) -> void;
  auto invalidateCache(QTextDocument *document) -> void;

protected:
  virtual auto onEnter(int newState, int *indentDepth, int *savedIndentDepth, int *paddingDepth, int *savedPaddingDepth) const -> void = 0;
  virtual auto adjustIndent(const CPlusPlus::Tokens &tokens, int lexerState, int *indentDepth, int *paddingDepth) const -> void = 0;

  class State;
  class BlockData {
  public:
    QStack<State> m_beginState;
    QStack<State> m_endState;
    int m_indentDepth = 0;
    int m_paddingDepth = 0;
    int m_blockRevision = -1;
  };

  virtual auto saveBlockData(QTextBlock *block, const BlockData &data) const -> void = 0;
  virtual auto loadBlockData(const QTextBlock &block, BlockData *data) const -> bool = 0;
  virtual auto saveLexerState(QTextBlock *block, int state) const -> void = 0;
  virtual auto loadLexerState(const QTextBlock &block) const -> int = 0;

public: // must be public to make Q_GADGET introspection work
  enum StateType {
    invalid = 0,
    topmost_intro,
    // The first line in a "topmost" definition.
    multiline_comment_start,
    // Inside the first line of a multi-line C style block comment.
    multiline_comment_cont,
    // Inside the following lines of a multi-line C style block comment.
    cpp_macro_start,
    // After the '#' token
    cpp_macro,
    // The start of a C preprocessor macro definition.
    cpp_macro_cont,
    // Subsequent lines of a multi-line C preprocessor macro definition.
    cpp_macro_conditional,
    // Special marker used for separating saved from current state when dealing with #ifdef
    qt_like_macro,
    // after an identifier starting with Q_ or QT_ at the beginning of the line
    label,
    // after an identifier followed by a colon
    defun_open,
    // Brace that opens a top-level function definition.
    using_start,
    // right after the "using" token
    class_start,
    // after the 'class' token
    class_open,
    // Brace that opens a class definition.
    access_specifier_start,
    // after 'private', 'protected' etc.
    member_init_open,
    // After ':' that starts a member initialization list.
    member_init_expected,
    // At the start and after every ',' in member_init_open
    member_init,
    // After an identifier in member_init_expected
    member_init_nest_open,
    // After '(' or '{' in member_init.
    enum_start,
    // After 'enum'
    enum_open,
    // Brace that opens a enum declaration.
    brace_list_open,
    // Open brace nested inside an enum or for a static array list.
    namespace_start,
    // after the namespace token, before the opening brace.
    namespace_open,
    // Brace that opens a C++ namespace block.
    extern_start,
    // after the extern token, before the opening brace.
    extern_open,
    // Brace that opens a C++ extern block.
    declaration_start,
    // shifted a token which could start a declaration.
    operator_declaration,
    // after 'operator' in declaration_start
    template_start,
    // after the 'template' token
    template_param,
    // after the '<' in a template_start
    if_statement,
    // After 'if'
    maybe_else,
    // after the first substatement in an if
    else_clause,
    // The else line of an if-else construct.
    for_statement,
    // After the 'for' token
    for_statement_paren_open,
    // While inside the (...)
    for_statement_init,
    // The initializer part of the for statement
    for_statement_condition,
    // The condition part of the for statement
    for_statement_expression,
    // The expression part of the for statement
    switch_statement,
    // After 'switch' token
    case_start,
    // after a 'case' or 'default' token
    case_cont,
    // after the colon in a case/default
    statement_with_condition,
    // A statement that takes a condition after the start token.
    do_statement,
    // After 'do' token
    return_statement,
    // After 'return'
    block_open,
    // Statement block open brace.
    substatement,
    // The first line after a conditional or loop construct.
    substatement_open,
    // The brace that opens a substatement block.
    arglist_open,
    // after the lparen. TODO: check if this is enough.
    stream_op,
    // After a '<<' or '>>' in a context where it's likely a stream operator.
    stream_op_cont,
    // When finding another stream operator in stream_op
    ternary_op,
    // The ? : operator
    braceinit_open,
    // after '{' in an expression context
    condition_open,
    // Start of a condition in 'if', 'while', entered after opening paren
    condition_paren_open,
    // After an lparen in a condition
    assign_open,
    // after an assignment token
    expression,
    // after a '=' in a declaration_start once we're sure it's not '= {'
    assign_open_or_initializer,
    // after a '=' in a declaration start
    lambda_instroducer_or_subscribtion,
    // just after '[' or in cases '[]' and '[id]' when we're not sure in the exact kind of expression
    lambda_declarator_expected,
    // just after ']' in lambda_introducer_or_subscribtion
    lambda_declarator_or_expression,
    // just after '](' when previous state is 'lambda_instroducer_or_subscribtion'
    lambda_statement_expected,
    lambda_instroducer,
    // when '=', '&' or ',' occurred within '[]'
    lambda_declarator,
    // just after ']' when previous state is lambda_introducer
    lambda_statement,
    // just after '{' when previous state is lambda_declarator or lambda_declarator_or_expression
    string_open,
    // after opening quote of simple string types, like ", L", u8" etc.
    raw_string_open // after raw string open delimiter, like R"EOF(
  };

  Q_ENUM(StateType)

protected:
  class State {
  public:
    State() = default;
    State(quint8 ty, quint16 savedIndentDepth, quint16 savedPaddingDepth) : savedIndentDepth(savedIndentDepth), savedPaddingDepth(savedPaddingDepth), type(ty) {}

    quint16 savedIndentDepth = 0;
    quint16 savedPaddingDepth = 0;
    quint8 type = 0;

    auto operator==(const State &other) const -> bool
    {
      return type == other.type && savedIndentDepth == other.savedIndentDepth && savedPaddingDepth == other.savedPaddingDepth;
    }
  };

  auto state(int belowTop = 0) const -> State;
  auto tokenIndex() const -> int;
  auto tokenCount() const -> int;
  auto currentToken() const -> const CPlusPlus::Token&;
  auto tokenAt(int idx) const -> const CPlusPlus::Token&;
  auto column(int position) const -> int;
  auto isBracelessState(int type) const -> bool;
  auto dump() const -> void;

private:
  auto recalculateStateAfter(const QTextBlock &block) -> void;
  auto saveCurrentState(const QTextBlock &block) -> void;
  auto restoreCurrentState(const QTextBlock &block) -> void;
  auto currentTokenText() const -> QStringView;
  auto tokenizeBlock(const QTextBlock &block, bool *endedJoined = nullptr) -> int;
  auto turnInto(int newState) -> void;
  auto tryExpression(bool alsoExpression = false) -> bool;
  auto tryDeclaration() -> bool;
  auto tryStatement() -> bool;
  auto enter(int newState) -> void;
  auto leave(bool statementDone = false) -> void;
  auto correctIndentation(const QTextBlock &block) -> void;
  static auto initialState() -> QStack<State>;

  QStack<State> m_beginState;
  QStack<State> m_currentState;
  QStack<State> m_newStates;
  CPlusPlus::Tokens m_tokens;
  QString m_currentLine;
  CPlusPlus::Token m_currentToken;
  int m_tokenIndex = 0;
  int m_indentDepth = 0;
  int m_paddingDepth = 0;
  int m_tabSize = 4;

  friend class Internal::CppCodeFormatterData;
};

class CPPEDITOR_EXPORT QtStyleCodeFormatter : public CodeFormatter {
public:
  QtStyleCodeFormatter();
  QtStyleCodeFormatter(const TextEditor::TabSettings &tabSettings, const CppCodeStyleSettings &settings);

  auto setTabSettings(const TextEditor::TabSettings &tabSettings) -> void;
  auto setCodeStyleSettings(const CppCodeStyleSettings &settings) -> void;

protected:
  auto onEnter(int newState, int *indentDepth, int *savedIndentDepth, int *paddingDepth, int *savedPaddingDepth) const -> void override;
  auto adjustIndent(const CPlusPlus::Tokens &tokens, int lexerState, int *indentDepth, int *paddingDepth) const -> void override;
  auto saveBlockData(QTextBlock *block, const BlockData &data) const -> void override;
  auto loadBlockData(const QTextBlock &block, BlockData *data) const -> bool override;
  auto saveLexerState(QTextBlock *block, int state) const -> void override;
  auto loadLexerState(const QTextBlock &block) const -> int override;
  static auto shouldClearPaddingOnEnter(int state) -> bool;

private:
  auto addContinuationIndent(int *paddingDepth) const -> void;

  TextEditor::TabSettings m_tabSettings;
  CppCodeStyleSettings m_styleSettings;
};

} // namespace CppEditor
