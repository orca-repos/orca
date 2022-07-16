// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppselectionchanger.hpp"

#include <utils/textutils.hpp>
#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QString>
#include <QTextBlock>
#include <QTextDocument>

using namespace CPlusPlus;
using namespace Utils::Text;

enum {
  debug = false
};

namespace CppEditor {

namespace Internal {
constexpr int kChangeSelectionNodeIndexNotSet = -1;
constexpr int kChangeSelectionNodeIndexWholeDocoument = -2;
} // namespace Internal

using namespace Internal;

CppSelectionChanger::CppSelectionChanger(QObject *parent) : QObject(parent), m_changeSelectionNodeIndex(kChangeSelectionNodeIndexNotSet), m_nodeCurrentStep(kChangeSelectionNodeIndexNotSet) {}

auto CppSelectionChanger::onCursorPositionChanged(const QTextCursor &newCursor) -> void
{
  // Reset the text cursor to be used for initial change selection behavior, only in the case
  // that the cursor is not being modified by the actual change selection methods.
  if (!m_inChangeSelection) {
    m_initialChangeSelectionCursor = newCursor;
    setNodeIndexAndStep(NodeIndexAndStepNotSet);
    if (debug)
      qDebug() << "Updating change selection cursor position:" << newCursor.position();
  }
}

namespace {

auto hasNoSelectionAndShrinking(CppSelectionChanger::Direction direction, const QTextCursor &cursor) -> bool
{
  if (direction == CppSelectionChanger::ShrinkSelection && !cursor.hasSelection()) {
    if (debug)
      qDebug() << "No selection to shrink, exiting early.";
    return true;
  }
  return false;
}

auto ensureCursorSelectionIsNotFlipped(QTextCursor &cursor) -> void
{
  if (cursor.hasSelection() && (cursor.anchor() > cursor.position()))
    cursor = flippedCursor(cursor);

  if (debug) {
    int l, c;
    convertPosition(cursor.document(), cursor.position(), &l, &c);

    qDebug() << "Cursor details: " << cursor.anchor() << cursor.position() << " l,c:" << l << ":" << c;
  }
}

auto isDocumentAvailable(const CPlusPlus::Document::Ptr doc) -> bool
{
  if (!doc) {
    if (debug)
      qDebug() << "Document is not available.";
    return false;
  }
  return true;
}

auto getWholeDocumentCursor(const QTextCursor &cursor) -> QTextCursor
{
  auto newWholeDocumentCursor(cursor);
  newWholeDocumentCursor.setPosition(0, QTextCursor::MoveAnchor);
  newWholeDocumentCursor.setPosition(cursor.document()->characterCount() - 1, QTextCursor::KeepAnchor);
  return newWholeDocumentCursor;
}

auto isWholeDocumentSelectedAndExpanding(CppSelectionChanger::Direction direction, const QTextCursor &cursor) -> bool
{
  if (direction == CppSelectionChanger::ExpandSelection && cursor.hasSelection()) {
    const auto wholeDocumentCursor = getWholeDocumentCursor(cursor);
    if (wholeDocumentCursor == cursor) {
      if (debug)
        qDebug() << "Selection is whole document, nothing to expand, exiting early.";
      return true;
    }
  }
  return false;
}

} // end of anonymous namespace

auto CppSelectionChanger::getTokenStartCursorPosition(unsigned tokenIndex, const QTextCursor &cursor) const -> int
{
  int startLine, startColumn;
  m_unit->getTokenStartPosition(tokenIndex, &startLine, &startColumn);

  const QTextDocument *document = cursor.document();
  const auto startPosition = document->findBlockByNumber(startLine - 1).position() + startColumn - 1;

  return startPosition;
}

auto CppSelectionChanger::getTokenEndCursorPosition(unsigned tokenIndex, const QTextCursor &cursor) const -> int
{
  int endLine, endColumn;
  m_unit->getTokenEndPosition(tokenIndex, &endLine, &endColumn);

  const QTextDocument *document = cursor.document();
  const auto endPosition = document->findBlockByNumber(endLine - 1).position() + endColumn - 1;

  return endPosition;
}

auto CppSelectionChanger::printTokenDebugInfo(unsigned tokenIndex, const QTextCursor &cursor, QString prefix) const -> void
{
  int line, column;
  const Token token = m_unit->tokenAt(tokenIndex);
  m_unit->getTokenStartPosition(tokenIndex, &line, &column);
  const auto startPos = getTokenStartCursorPosition(tokenIndex, cursor);
  const auto endPos = getTokenEndCursorPosition(tokenIndex, cursor);

  qDebug() << qSetFieldWidth(20) << prefix << qSetFieldWidth(0) << token.spell() << tokenIndex << " l, c:" << line << ":" << column << " offset: " << token.utf16chars() << startPos << endPos;
}

auto CppSelectionChanger::shouldSkipASTNodeBasedOnPosition(const ASTNodePositions &positions, const QTextCursor &cursor) const -> bool
{
  auto shouldSkipNode = false;

  auto isEqual = cursor.anchor() == positions.astPosStart && cursor.position() == positions.astPosEnd;

  // New selections should include initial selection.
  auto includesInitialSelection = m_initialChangeSelectionCursor.anchor() >= positions.astPosStart && m_initialChangeSelectionCursor.position() <= positions.astPosEnd;

  // Prefer new selections to start with initial cursor if anchor == position.
  if (!m_initialChangeSelectionCursor.hasSelection()) {
    includesInitialSelection = m_initialChangeSelectionCursor.position() < positions.astPosEnd;
  }

  // When expanding: Skip if new selection is smaller than current cursor selection.
  // When shrinking: Skip if new selection is bigger than current cursor selection.
  auto isNewSelectionSmaller = positions.astPosStart > cursor.anchor() || positions.astPosEnd < cursor.position();
  auto isNewSelectionBigger = positions.astPosStart < cursor.anchor() || positions.astPosEnd > cursor.position();

  if (m_direction == CppSelectionChanger::ExpandSelection && (isNewSelectionSmaller || isEqual || !includesInitialSelection)) {
    shouldSkipNode = true;
  } else if (m_direction == CppSelectionChanger::ShrinkSelection && (isNewSelectionBigger || isEqual || !includesInitialSelection)) {
    shouldSkipNode = true;
  }

  if (debug && shouldSkipNode) {
    qDebug() << "isEqual:" << isEqual << "includesInitialSelection:" << includesInitialSelection << "isNewSelectionSmaller:" << isNewSelectionSmaller << "isNewSelectionBigger:" << isNewSelectionBigger;
  }

  return shouldSkipNode;
}

auto CppSelectionChanger::getASTPositions(AST *ast, const QTextCursor &cursor) const -> ASTNodePositions
{
  ASTNodePositions positions(ast);

  // An AST node's contents is bound by its first token start position inclusively,
  // and its last token start position exclusively.
  // So we are also interested in the second to last token, which is actually
  // included in the bounds.
  positions.firstTokenIndex = ast->firstToken();
  positions.lastTokenIndex = ast->lastToken();
  positions.secondToLastTokenIndex = positions.lastTokenIndex - 1;

  // The AST position start is the start of the first token.
  positions.astPosStart = getTokenStartCursorPosition(positions.firstTokenIndex, cursor);

  // The end position depends on whether, there is only one token involved in the current AST
  // node or multiple ones.
  // Default we assume that there is only one token, so the end position of the AST node
  // is the start of the last token.
  // If there is more than one (second to last token will be different to the first token)
  // use the second to last token end position as the AST node end position.
  positions.astPosEnd = getTokenStartCursorPosition(positions.lastTokenIndex, cursor);
  if (positions.lastTokenIndex != positions.firstTokenIndex)
    positions.astPosEnd = getTokenEndCursorPosition(positions.secondToLastTokenIndex, cursor);

  if (debug) {
    qDebug() << "Token positions start and end:" << positions.astPosStart << positions.astPosEnd;
  }

  return positions;
}

auto CppSelectionChanger::updateCursorSelection(QTextCursor &cursorToModify, ASTNodePositions positions) -> void
{
  m_workingCursor.setPosition(positions.astPosStart, QTextCursor::MoveAnchor);
  m_workingCursor.setPosition(positions.astPosEnd, QTextCursor::KeepAnchor);
  cursorToModify = m_workingCursor;

  if (debug) {
    printTokenDebugInfo(positions.firstTokenIndex, m_workingCursor, QString::fromLatin1("First token:"));
    printTokenDebugInfo(positions.lastTokenIndex, m_workingCursor, QString::fromLatin1("Last token:"));
    printTokenDebugInfo(positions.secondToLastTokenIndex, m_workingCursor, QString::fromLatin1("Second to last:"));

    qDebug() << "Anchor is now: " << m_workingCursor.anchor();
    qDebug() << "Position is now: " << m_workingCursor.position();
  }
}

auto CppSelectionChanger::getFirstCurrentStepForASTNode(AST *ast) const -> int
{
  if (m_direction == ExpandSelection)
    return 1;
  else
    return possibleASTStepCount(ast);
}

auto CppSelectionChanger::isLastPossibleStepForASTNode(AST *ast) const -> bool
{
  if (m_direction == ExpandSelection)
    return currentASTStep() == possibleASTStepCount(ast);
  else
    return currentASTStep() == 1;
}

auto CppSelectionChanger::getFineTunedASTPositions(AST *ast, const QTextCursor &cursor) const -> ASTNodePositions
{
  ASTNodePositions positions = getASTPositions(ast, cursor);
  fineTuneASTNodePositions(positions);
  return positions;
}

auto CppSelectionChanger::findRelevantASTPositionsFromCursor(const QList<AST*> &astPath, const QTextCursor &cursor, int startingFromNodeIndex) -> ASTNodePositions
{
  ASTNodePositions currentNodePositions;
  const int size = astPath.size();
  int currentAstIndex = m_direction == ExpandSelection ? size - 1 : 0;

  // Adjust starting node index, if a valid value was passed.
  if (startingFromNodeIndex != kChangeSelectionNodeIndexNotSet)
    currentAstIndex = startingFromNodeIndex;

  if (currentAstIndex < size && currentAstIndex >= 0) {
    AST *ast = astPath.at(currentAstIndex);
    m_changeSelectionNodeIndex = currentAstIndex;
    m_nodeCurrentStep = getFirstCurrentStepForASTNode(ast);
    currentNodePositions = getFineTunedASTPositions(ast, cursor);

    if (debug && startingFromNodeIndex == kChangeSelectionNodeIndexNotSet)
      qDebug() << "Setting AST index for the first time.";
  }

  if (!currentNodePositions.ast)
    setNodeIndexAndStep(NodeIndexAndStepNotSet);

  return currentNodePositions;
}

auto CppSelectionChanger::findRelevantASTPositionsFromCursorWhenNodeIndexNotSet(const QList<AST*> &astPath, const QTextCursor &cursor) -> ASTNodePositions
{
  // Find relevant AST node from cursor, when the user expands for the first time.
  return findRelevantASTPositionsFromCursor(astPath, cursor);
}

auto CppSelectionChanger::findRelevantASTPositionsFromCursorWhenWholeDocumentSelected(const QList<AST*> &astPath, const QTextCursor &cursor) -> ASTNodePositions
{
  // Can't expand more, because whole document is selected.
  if (m_direction == ExpandSelection)
    return {};

  // In case of shrink, select the next smaller selection.
  return findRelevantASTPositionsFromCursor(astPath, cursor);
}

auto CppSelectionChanger::findRelevantASTPositionsFromCursorFromPreviousNodeIndex(const QList<AST*> &astPath, const QTextCursor &cursor) -> ASTNodePositions
{
  ASTNodePositions nodePositions;

  // This is not the first expansion, use the previous node index.
  nodePositions.ast = astPath.at(m_changeSelectionNodeIndex);

  // We reached the last possible step for the current AST node, so we move to the
  // next / previous one depending on the direction.
  if (isLastPossibleStepForASTNode(nodePositions.ast)) {
    auto newAstIndex = m_changeSelectionNodeIndex;
    if (m_direction == ExpandSelection)
      --newAstIndex;
    else
      ++newAstIndex;

    if (newAstIndex < 0 || newAstIndex >= astPath.count()) {
      if (debug)
        qDebug() << "Skipping expansion because there is no available next AST node.";
      return {};
    }

    // Switch to next AST and set the first step.
    nodePositions = findRelevantASTPositionsFromCursor(astPath, cursor, newAstIndex);
    if (!nodePositions)
      return {};

    if (debug)
      qDebug() << "Moved to next AST node.";
  } else {
    // There are possible steps available for current node, so move to the next / previous
    // step.
    if (m_direction == ExpandSelection)
      ++m_nodeCurrentStep;
    else
      --m_nodeCurrentStep;
    nodePositions = getFineTunedASTPositions(nodePositions.ast, cursor);

    if (debug)
      qDebug() << "Moved to next AST step.";
  }

  return nodePositions;
}

auto CppSelectionChanger::findNextASTStepPositions(const QTextCursor &cursor) -> ASTNodePositions
{
  // Find AST node path starting from the initial change selection cursor.
  // The ASTPath class, only takes into consideration the position of the cursor, but not the
  // anchor. We make up for that later in the code.
  auto cursorToStartFrom(m_initialChangeSelectionCursor);

  ASTPath astPathFinder(m_doc);
  const QList<AST*> astPath = astPathFinder(cursorToStartFrom);

  #ifdef WITH_AST_PATH_DUMP
    if (debug)
        ASTPath::dump(astPath);
  #endif

  if (astPath.size() == 0)
    return {};

  ASTNodePositions currentNodePositions;
  if (m_changeSelectionNodeIndex == kChangeSelectionNodeIndexNotSet) {
    currentNodePositions = findRelevantASTPositionsFromCursorWhenNodeIndexNotSet(astPath, cursor);
  } else if (m_changeSelectionNodeIndex == kChangeSelectionNodeIndexWholeDocoument) {
    currentNodePositions = findRelevantASTPositionsFromCursorWhenWholeDocumentSelected(astPath, cursor);
  } else {
    currentNodePositions = findRelevantASTPositionsFromCursorFromPreviousNodeIndex(astPath, cursor);
  }

  if (debug) {
    qDebug() << "m_changeSelectionNodeIndex:" << m_changeSelectionNodeIndex << "possible step count:" << possibleASTStepCount(currentNodePositions.ast) << "current step:" << m_nodeCurrentStep;
  }

  QTC_ASSERT(m_nodeCurrentStep >= 1, return {});

  return currentNodePositions;
}

auto CppSelectionChanger::fineTuneForStatementPositions(unsigned firstParenTokenIndex, unsigned lastParenTokenIndex, ASTNodePositions &positions) const -> void
{
  Token firstParenToken = m_unit->tokenAt(firstParenTokenIndex);
  Token lastParenToken = m_unit->tokenAt(lastParenTokenIndex);
  if (debug) {
    qDebug() << "firstParenToken:" << firstParenToken.spell();
    qDebug() << "lastParenToken:" << lastParenToken.spell();
  }

  auto newPosStart = getTokenStartCursorPosition(firstParenTokenIndex, m_workingCursor);
  auto newPosEnd = getTokenEndCursorPosition(lastParenTokenIndex, m_workingCursor);

  auto isOutsideParen = m_initialChangeSelectionCursor.position() <= newPosStart;

  if (currentASTStep() == 1 && !isOutsideParen) {
    if (debug)
      qDebug() << "Selecting Paren contents of for statement.";
    positions.astPosStart = newPosStart + 1;
    positions.astPosEnd = newPosEnd - 1;
  }
  if (currentASTStep() == 2 && !isOutsideParen) {
    if (debug)
      qDebug() << "Selecting Paren of for statement together with contents.";
    positions.astPosStart = newPosStart;
    positions.astPosEnd = newPosEnd;
  }
}

auto CppSelectionChanger::fineTuneASTNodePositions(ASTNodePositions &positions) const -> void
{
  AST *ast = positions.ast;

  if (ast->asCompoundStatement()) {
    // Allow first selecting the contents of the scope, without selecting the braces, and
    // afterwards select the contents together with  braces.
    if (currentASTStep() == 1) {
      if (debug)
        qDebug() << "Selecting inner contents of compound statement.";

      auto firstInnerTokenIndex = positions.firstTokenIndex + 1;
      auto lastInnerTokenIndex = positions.lastTokenIndex - 2;
      Token firstInnerToken = m_unit->tokenAt(firstInnerTokenIndex);
      Token lastInnerToken = m_unit->tokenAt(lastInnerTokenIndex);
      if (debug) {
        qDebug() << "LastInnerToken:" << lastInnerToken.spell();
        qDebug() << "FirstInnerToken:" << firstInnerToken.spell();
      }

      // Check if compound statement is empty, then select just the blank space inside it.
      int newPosStart, newPosEnd;
      if (positions.secondToLastTokenIndex - positions.firstTokenIndex <= 1) {
        // TODO: If the empty space has a new tab character, or spaces, and the document is
        // not saved, the last semantic info is not updated, and the selection is not
        // properly computed. Figure out how to work around this.
        newPosStart = getTokenEndCursorPosition(positions.firstTokenIndex, m_workingCursor);
        newPosEnd = getTokenStartCursorPosition(positions.secondToLastTokenIndex, m_workingCursor);
        if (debug)
          qDebug() << "Selecting inner contents of compound statement which is empty.";
      } else {
        // Select the inner contents of the scope, without the braces.
        newPosStart = getTokenStartCursorPosition(firstInnerTokenIndex, m_workingCursor);
        newPosEnd = getTokenEndCursorPosition(lastInnerTokenIndex, m_workingCursor);
      }

      if (debug) {
        qDebug() << "New" << newPosStart << newPosEnd << "Old" << m_workingCursor.anchor() << m_workingCursor.position();
      }

      positions.astPosStart = newPosStart;
      positions.astPosEnd = newPosEnd;
    }
    // Next time, we select the braces as well. Reverse for shrinking.
    // The positions already have the correct selection, so no need to set them.
  } else if (CallAST *callAST = ast->asCall()) {
    unsigned firstParenTokenIndex = callAST->lparen_token;
    unsigned lastParenTokenIndex = callAST->rparen_token;
    Token firstParenToken = m_unit->tokenAt(firstParenTokenIndex);
    Token lastParenToken = m_unit->tokenAt(lastParenTokenIndex);
    if (debug) {
      qDebug() << "firstParenToken:" << firstParenToken.spell();
      qDebug() << "lastParenToken:" << lastParenToken.spell();
    }

    // Select the parenthesis of the call, and everything between.
    int newPosStart = getTokenStartCursorPosition(firstParenTokenIndex, m_workingCursor);
    int newPosEnd = getTokenEndCursorPosition(lastParenTokenIndex, m_workingCursor);

    bool isInFunctionName = m_initialChangeSelectionCursor.position() <= newPosStart;

    // If cursor is inside the function name, select the name implicitly (because it's a
    // different AST node), and then the whole call expression (so just one step).
    // If cursor is inside parentheses, on first step select everything inside them,
    // on second step select the everything inside parentheses including them,
    // on third step select the whole call expression.
    if (currentASTStep() == 1 && !isInFunctionName) {
      if (debug)
        qDebug() << "Selecting everything inside parentheses.";
      positions.astPosStart = newPosStart + 1;
      positions.astPosEnd = newPosEnd - 1;
    }
    if (currentASTStep() == 2 && !isInFunctionName) {
      if (debug)
        qDebug() << "Selecting everything inside and including " "the parentheses of the function call.";
      positions.astPosStart = newPosStart;
      positions.astPosEnd = newPosEnd;
    }
  } else if (StringLiteralAST *stringLiteralAST = ast->asStringLiteral()) {
    // Select literal without quotes on first step, and the whole literal on next step.
    if (currentASTStep() == 1) {
      Token firstToken = m_unit->tokenAt(stringLiteralAST->firstToken());
      bool isRawLiteral = firstToken.f.kind >= T_FIRST_RAW_STRING_LITERAL && firstToken.f.kind <= T_RAW_UTF32_STRING_LITERAL;
      if (debug && isRawLiteral)
        qDebug() << "Is raw literal.";

      // Start from positions that include quotes.
      auto newPosEnd = positions.astPosEnd;

      // Decrement last position to skip last quote.
      --newPosEnd;

      // If raw literal also skip parenthesis.
      if (isRawLiteral)
        --newPosEnd;

      // Start position will be the end position minus the size of the actual contents of the
      // literal.
      int newPosStart = newPosEnd - firstToken.string->size();

      // Skip raw literal parentheses.
      if (isRawLiteral)
        newPosStart += 2;

      positions.astPosStart = newPosStart;
      positions.astPosEnd = newPosEnd;
      if (debug)
        qDebug() << "Selecting inner contents of string literal.";
    }
  } else if (NumericLiteralAST *numericLiteralAST = ast->asNumericLiteral()) {
    Token firstToken = m_unit->tokenAt(numericLiteralAST->firstToken());
    // If char literal, select it without quotes on first step.
    if (firstToken.isCharLiteral()) {
      if (currentASTStep() == 1) {
        if (debug)
          qDebug() << "Selecting inner contents of char literal.";

        positions.astPosEnd = positions.astPosEnd - 1;
        positions.astPosStart = positions.astPosEnd - firstToken.literal->size();
      }
    }
  } else if (ForStatementAST *forStatementAST = ast->asForStatement()) {
    unsigned firstParenTokenIndex = forStatementAST->lparen_token;
    unsigned lastParenTokenIndex = forStatementAST->rparen_token;
    fineTuneForStatementPositions(firstParenTokenIndex, lastParenTokenIndex, positions);
  } else if (RangeBasedForStatementAST *rangeForStatementAST = ast->asRangeBasedForStatement()) {
    unsigned firstParenTokenIndex = rangeForStatementAST->lparen_token;
    unsigned lastParenTokenIndex = rangeForStatementAST->rparen_token;
    fineTuneForStatementPositions(firstParenTokenIndex, lastParenTokenIndex, positions);
  } else if (ClassSpecifierAST *classSpecificerAST = ast->asClassSpecifier()) {

    unsigned firstBraceTokenIndex = classSpecificerAST->lbrace_token;
    unsigned lastBraceTokenIndex = classSpecificerAST->rbrace_token;
    unsigned classKeywordTokenIndex = classSpecificerAST->classkey_token;

    Token firstBraceToken = m_unit->tokenAt(firstBraceTokenIndex);
    Token lastBraceToken = m_unit->tokenAt(lastBraceTokenIndex);
    Token classKeywordToken = m_unit->tokenAt(classKeywordTokenIndex);

    if (debug) {
      qDebug() << "firstBraceToken:" << firstBraceToken.spell();
      qDebug() << "lastBraceToken:" << lastBraceToken.spell();
      qDebug() << "classKeywordToken:" << classKeywordToken.spell();

    }

    int newPosStart = getTokenStartCursorPosition(firstBraceTokenIndex, m_workingCursor);
    int newPosEnd = getTokenEndCursorPosition(lastBraceTokenIndex, m_workingCursor);

    bool isOutsideBraces = m_initialChangeSelectionCursor.position() <= newPosStart;
    bool isInsideBraces = !isOutsideBraces;

    int classKeywordPosStart = getTokenStartCursorPosition(classKeywordTokenIndex, m_workingCursor);

    int classKeywordPosEnd = getTokenEndCursorPosition(classKeywordTokenIndex, m_workingCursor);

    bool isInClassKeyword = m_initialChangeSelectionCursor.anchor() >= classKeywordPosStart && m_initialChangeSelectionCursor.position() <= classKeywordPosEnd;

    auto isInClassName = false;
    int classNamePosEnd = newPosEnd;
    NameAST *nameAST = classSpecificerAST->name;
    if (nameAST) {
      SimpleNameAST *classNameAST = nameAST->asSimpleName();
      if (classNameAST) {
        unsigned identifierTokenIndex = classNameAST->identifier_token;
        Token identifierToken = m_unit->tokenAt(identifierTokenIndex);
        if (debug)
          qDebug() << "identifierToken:" << identifierToken.spell();

        int classNamePosStart = getTokenStartCursorPosition(identifierTokenIndex, m_workingCursor);
        classNamePosEnd = getTokenEndCursorPosition(identifierTokenIndex, m_workingCursor);

        isInClassName = m_initialChangeSelectionCursor.anchor() >= classNamePosStart && m_initialChangeSelectionCursor.position() <= classNamePosEnd;
      }
    }

    if (currentASTStep() == 1 && isInsideBraces) {
      if (debug)
        qDebug() << "Selecting everything inside braces of class statement.";
      positions.astPosStart = newPosStart + 1;
      positions.astPosEnd = newPosEnd - 1;
    }
    if (currentASTStep() == 2 && isInsideBraces) {
      if (debug)
        qDebug() << "Selecting braces of class statement.";
      positions.astPosStart = newPosStart;
      positions.astPosEnd = newPosEnd;
    }
    if (currentASTStep() == 1 && isInClassKeyword) {
      if (debug)
        qDebug() << "Selecting class keyword.";
      positions.astPosStart = classKeywordPosStart;
      positions.astPosEnd = classKeywordPosEnd;
    }
    if (currentASTStep() == 2 && isInClassKeyword) {
      if (debug)
        qDebug() << "Selecting class keyword and name.";
      positions.astPosStart = classKeywordPosStart;
      positions.astPosEnd = classNamePosEnd;
    }
    if (currentASTStep() == 1 && isInClassName) {
      if (debug)
        qDebug() << "Selecting class keyword and name.";
      positions.astPosStart = classKeywordPosStart;
      positions.astPosEnd = classNamePosEnd;
    }
  } else if (NamespaceAST *namespaceAST = ast->asNamespace()) {
    unsigned namespaceTokenIndex = namespaceAST->namespace_token;
    unsigned identifierTokenIndex = namespaceAST->identifier_token;
    Token namespaceToken = m_unit->tokenAt(namespaceTokenIndex);
    Token identifierToken = m_unit->tokenAt(identifierTokenIndex);
    if (debug) {
      qDebug() << "namespace token:" << namespaceToken.spell();
      qDebug() << "identifier token:" << identifierToken.spell();
    }

    int namespacePosStart = getTokenStartCursorPosition(namespaceTokenIndex, m_workingCursor);
    int namespacePosEnd = getTokenEndCursorPosition(namespaceTokenIndex, m_workingCursor);

    int identifierPosStart = getTokenStartCursorPosition(identifierTokenIndex, m_workingCursor);
    int identifierPosEnd = getTokenEndCursorPosition(identifierTokenIndex, m_workingCursor);

    bool isInNamespaceKeyword = m_initialChangeSelectionCursor.position() <= namespacePosEnd;

    bool isInNamespaceIdentifier = m_initialChangeSelectionCursor.anchor() >= identifierPosStart && m_initialChangeSelectionCursor.position() <= identifierPosEnd;

    if (currentASTStep() == 1) {
      if (isInNamespaceKeyword) {
        if (debug)
          qDebug() << "Selecting namespace keyword.";
        positions.astPosStart = namespacePosStart;
        positions.astPosEnd = namespacePosEnd;
      } else if (isInNamespaceIdentifier) {
        if (debug)
          qDebug() << "Selecting namespace identifier.";
        positions.astPosStart = identifierPosStart;
        positions.astPosEnd = identifierPosEnd;
      }
    } else if (currentASTStep() == 2) {
      if (isInNamespaceKeyword || isInNamespaceIdentifier) {
        if (debug)
          qDebug() << "Selecting namespace keyword and identifier.";
        positions.astPosStart = namespacePosStart;
        positions.astPosEnd = identifierPosEnd;

      }
    }
  } else if (ExpressionListParenAST *parenAST = ast->asExpressionListParen()) {
    unsigned firstParenTokenIndex = parenAST->lparen_token;
    unsigned lastParenTokenIndex = parenAST->rparen_token;
    Token firstParenToken = m_unit->tokenAt(firstParenTokenIndex);
    Token lastParenToken = m_unit->tokenAt(lastParenTokenIndex);
    if (debug) {
      qDebug() << "firstParenToken:" << firstParenToken.spell();
      qDebug() << "lastParenToken:" << lastParenToken.spell();
    }

    // Select the parentheses, and everything between.
    int newPosStart = getTokenStartCursorPosition(firstParenTokenIndex, m_workingCursor);
    int newPosEnd = getTokenEndCursorPosition(lastParenTokenIndex, m_workingCursor);

    if (currentASTStep() == 1) {
      if (debug)
        qDebug() << "Selecting everything inside parentheses.";
      positions.astPosStart = newPosStart + 1;
      positions.astPosEnd = newPosEnd - 1;
    }
    if (currentASTStep() == 2) {
      if (debug)
        qDebug() << "Selecting everything inside including the parentheses.";
      positions.astPosStart = newPosStart;
      positions.astPosEnd = newPosEnd;
    }
  } else if (FunctionDeclaratorAST *functionDeclaratorAST = ast->asFunctionDeclarator()) {
    unsigned firstParenTokenIndex = functionDeclaratorAST->lparen_token;
    unsigned lastParenTokenIndex = functionDeclaratorAST->rparen_token;
    Token firstParenToken = m_unit->tokenAt(firstParenTokenIndex);
    Token lastParenToken = m_unit->tokenAt(lastParenTokenIndex);
    if (debug) {
      qDebug() << "firstParenToken:" << firstParenToken.spell();
      qDebug() << "lastParenToken:" << lastParenToken.spell();
    }

    int newPosStart = getTokenStartCursorPosition(firstParenTokenIndex, m_workingCursor);
    int newPosEnd = getTokenEndCursorPosition(lastParenTokenIndex, m_workingCursor);

    if (currentASTStep() == 1) {
      if (debug)
        qDebug() << "Selecting everything inside and including the parentheses.";
      positions.astPosStart = newPosStart;
      positions.astPosEnd = newPosEnd;
    }
  } else if (FunctionDefinitionAST *functionDefinitionAST = ast->asFunctionDefinition()) {
    if (!functionDefinitionAST->function_body)
      return;

    CompoundStatementAST *compoundStatementAST = functionDefinitionAST->function_body->asCompoundStatement();
    if (!compoundStatementAST)
      return;

    if (!functionDefinitionAST->decl_specifier_list || !functionDefinitionAST->decl_specifier_list->value)
      return;

    SimpleSpecifierAST *simpleSpecifierAST = functionDefinitionAST->decl_specifier_list->value->asSimpleSpecifier();
    if (!simpleSpecifierAST)
      return;

    unsigned firstBraceTokenIndex = compoundStatementAST->lbrace_token;
    unsigned specifierTokenIndex = simpleSpecifierAST->firstToken();
    Token firstBraceToken = m_unit->tokenAt(firstBraceTokenIndex);
    Token specifierToken = m_unit->tokenAt(specifierTokenIndex);
    if (debug) {
      qDebug() << "firstBraceToken:" << firstBraceToken.spell();
      qDebug() << "specifierToken:" << specifierToken.spell();
    }

    int firstBracePosEnd = getTokenStartCursorPosition(firstBraceTokenIndex, m_workingCursor);

    bool isOutsideBraces = m_initialChangeSelectionCursor.position() <= firstBracePosEnd;

    if (currentASTStep() == 1 && isOutsideBraces) {
      int newPosStart = getTokenStartCursorPosition(specifierTokenIndex, m_workingCursor);

      if (debug)
        qDebug() << "Selecting everything to the left of the function braces.";
      positions.astPosStart = newPosStart;
      positions.astPosEnd = firstBracePosEnd - 1;
    }
  } else if (DeclaratorAST *declaratorAST = ast->asDeclarator()) {
    PostfixDeclaratorListAST *list = declaratorAST->postfix_declarator_list;
    if (!list)
      return;

    PostfixDeclaratorAST *postfixDeclarator = list->value;
    if (!postfixDeclarator)
      return;

    FunctionDeclaratorAST *functionDeclarator = postfixDeclarator->asFunctionDeclarator();
    if (!functionDeclarator)
      return;

    SpecifierListAST *cv_list = functionDeclarator->cv_qualifier_list;
    if (!cv_list)
      return;

    SpecifierAST *first_cv = cv_list->value;
    if (!first_cv)
      return;

    unsigned firstCVTokenIndex = first_cv->firstToken();
    Token firstCVToken = m_unit->tokenAt(firstCVTokenIndex);
    if (debug) {
      qDebug() << "firstCVTokenIndex:" << firstCVToken.spell();
    }

    int cvPosStart = getTokenStartCursorPosition(firstCVTokenIndex, m_workingCursor);
    bool isBeforeCVList = m_initialChangeSelectionCursor.position() < cvPosStart;

    if (currentASTStep() == 1 && isBeforeCVList) {
      if (debug)
        qDebug() << "Selecting function declarator without CV qualifiers.";

      int newPosEnd = cvPosStart;
      positions.astPosEnd = newPosEnd - 1;
    }

  } else if (TemplateIdAST *templateIdAST = ast->asTemplateId()) {
    unsigned identifierTokenIndex = templateIdAST->identifier_token;
    Token identifierToken = m_unit->tokenAt(identifierTokenIndex);
    if (debug) {
      qDebug() << "identifierTokenIndex:" << identifierToken.spell();
    }

    int newPosStart = getTokenStartCursorPosition(identifierTokenIndex, m_workingCursor);
    int newPosEnd = getTokenEndCursorPosition(identifierTokenIndex, m_workingCursor);

    bool isInsideIdentifier = m_initialChangeSelectionCursor.anchor() >= newPosStart && m_initialChangeSelectionCursor.position() <= newPosEnd;

    if (currentASTStep() == 1 && isInsideIdentifier) {
      if (debug)
        qDebug() << "Selecting just identifier before selecting template id.";
      positions.astPosStart = newPosStart;
      positions.astPosEnd = newPosEnd;
    }
  } else if (TemplateDeclarationAST *templateDeclarationAST = ast->asTemplateDeclaration()) {
    unsigned templateKeywordTokenIndex = templateDeclarationAST->template_token;
    unsigned greaterTokenIndex = templateDeclarationAST->greater_token;
    Token templateKeywordToken = m_unit->tokenAt(templateKeywordTokenIndex);
    Token greaterToken = m_unit->tokenAt(greaterTokenIndex);
    if (debug) {
      qDebug() << "templateKeywordTokenIndex:" << templateKeywordToken.spell();
      qDebug() << "greaterTokenIndex:" << greaterToken.spell();
    }

    int templateKeywordPosStart = getTokenStartCursorPosition(templateKeywordTokenIndex, m_workingCursor);
    int templateKeywordPosEnd = getTokenEndCursorPosition(templateKeywordTokenIndex, m_workingCursor);

    int templateParametersPosEnd = getTokenEndCursorPosition(greaterTokenIndex, m_workingCursor);

    bool isInsideTemplateKeyword = m_initialChangeSelectionCursor.anchor() >= templateKeywordPosStart && m_initialChangeSelectionCursor.position() <= templateKeywordPosEnd;

    if (currentASTStep() == 1 && isInsideTemplateKeyword) {
      if (debug)
        qDebug() << "Selecting template keyword.";
      positions.astPosStart = templateKeywordPosStart;
      positions.astPosEnd = templateKeywordPosEnd;
    }
    if (currentASTStep() == 2 && isInsideTemplateKeyword) {
      if (debug)
        qDebug() << "Selecting template keyword and parameters.";
      positions.astPosStart = templateKeywordPosStart;
      positions.astPosEnd = templateParametersPosEnd;
    }
  } else if (LambdaExpressionAST *lambdaExpressionAST = ast->asLambdaExpression()) {
    // TODO: Fix more lambda cases.
    LambdaIntroducerAST *lambdaIntroducerAST = lambdaExpressionAST->lambda_introducer;
    LambdaDeclaratorAST *lambdaDeclaratorAST = lambdaExpressionAST->lambda_declarator;
    if (!lambdaDeclaratorAST)
      return;

    TrailingReturnTypeAST *trailingReturnTypeAST = lambdaDeclaratorAST->trailing_return_type;
    unsigned firstSquareBracketTokenIndex = lambdaIntroducerAST->lbracket_token;
    unsigned lastParenTokenIndex = lambdaDeclaratorAST->rparen_token;

    Token firstSquareBracketToken = m_unit->tokenAt(firstSquareBracketTokenIndex);
    Token lastParenToken = m_unit->tokenAt(lastParenTokenIndex);
    if (debug) {
      qDebug() << "firstSquareBracketToken:" << firstSquareBracketToken.spell();
      qDebug() << "lastParenToken:" << lastParenToken.spell();
    }

    int firstSquareBracketPosStart = getTokenStartCursorPosition(firstSquareBracketTokenIndex, m_workingCursor);
    int lastParenPosEnd = getTokenEndCursorPosition(lastParenTokenIndex, m_workingCursor);

    bool isInsideDeclarator = m_initialChangeSelectionCursor.anchor() >= firstSquareBracketPosStart && m_initialChangeSelectionCursor.position() <= lastParenPosEnd;

    if (currentASTStep() == 1 && isInsideDeclarator) {
      if (debug)
        qDebug() << "Selecting lambda capture group and arguments.";
      positions.astPosStart = firstSquareBracketPosStart;
      positions.astPosEnd = lastParenPosEnd;
    }
    if (currentASTStep() == 2 && isInsideDeclarator && trailingReturnTypeAST) {
      if (debug)
        qDebug() << "Selecting lambda prototype.";

      unsigned lastReturnTypeTokenIndex = trailingReturnTypeAST->lastToken();
      Token lastReturnTypeToken = m_unit->tokenAt(lastReturnTypeTokenIndex);
      if (debug)
        qDebug() << "lastReturnTypeToken:" << lastReturnTypeToken.spell();
      int lastReturnTypePosEnd = getTokenEndCursorPosition(lastReturnTypeTokenIndex, m_workingCursor);

      positions.astPosStart = firstSquareBracketPosStart;
      positions.astPosEnd = lastReturnTypePosEnd - 2;
    }
  }
}

auto CppSelectionChanger::performSelectionChange(QTextCursor &cursorToModify) -> bool
{
  forever {
    if (auto positions = findNextASTStepPositions(m_workingCursor)) {
      if (!shouldSkipASTNodeBasedOnPosition(positions, m_workingCursor)) {
        updateCursorSelection(cursorToModify, positions);
        return true;
      } else {
        if (debug)
          qDebug() << "Skipping node.";
      }
    } else if (m_direction == ShrinkSelection) {
      // The last possible action to do, if there was no step with a smaller selection, is
      // to set the cursor to the initial change selection cursor, without an anchor.
      auto finalCursor(m_initialChangeSelectionCursor);
      finalCursor.setPosition(finalCursor.position(), QTextCursor::MoveAnchor);
      cursorToModify = finalCursor;
      setNodeIndexAndStep(NodeIndexAndStepNotSet);
      if (debug)
        qDebug() << "Final shrink selection case.";
      return true;
    } else if (m_direction == ExpandSelection) {
      // The last possible action to do, if there was no step with a bigger selection, is
      // to set the cursor to the whole document including header inclusions.
      auto finalCursor = getWholeDocumentCursor(m_initialChangeSelectionCursor);
      cursorToModify = finalCursor;
      setNodeIndexAndStep(NodeIndexAndStepWholeDocument);
      if (debug)
        qDebug() << "Final expand selection case.";
      return true;
    }
    // Break out of the loop, because no further modification of the selection can be done.
    else
      break;
  }

  // No next step found for given direction, return early without modifying the cursor.
  return false;
}

auto CppSelectionChanger::setNodeIndexAndStep(NodeIndexAndStepState state) -> void
{
  switch (state) {
  case NodeIndexAndStepWholeDocument:
    m_changeSelectionNodeIndex = kChangeSelectionNodeIndexWholeDocoument;
    m_nodeCurrentStep = kChangeSelectionNodeIndexWholeDocoument;
    break;
  case NodeIndexAndStepNotSet: default:
    m_changeSelectionNodeIndex = kChangeSelectionNodeIndexNotSet;
    m_nodeCurrentStep = kChangeSelectionNodeIndexNotSet;
    break;
  }
}

auto CppSelectionChanger::changeSelection(Direction direction, QTextCursor &cursorToModify, const CPlusPlus::Document::Ptr doc) -> bool
{
  m_workingCursor = cursorToModify;

  if (hasNoSelectionAndShrinking(direction, m_workingCursor))
    return false;

  if (isWholeDocumentSelectedAndExpanding(direction, m_workingCursor))
    return false;

  if (!isDocumentAvailable(doc)) {
    return false;
  }

  ensureCursorSelectionIsNotFlipped(m_workingCursor);

  m_doc = doc;
  m_unit = m_doc->translationUnit();
  m_direction = direction;

  return performSelectionChange(cursorToModify);
}

auto CppSelectionChanger::startChangeSelection() -> void
{
  // Stop cursorPositionChanged signal handler from setting the initial
  // change selection cursor, when the cursor is being changed as a result of the change
  // selection operation.
  m_inChangeSelection = true;
}

auto CppSelectionChanger::stopChangeSelection() -> void
{
  m_inChangeSelection = false;
}

auto CppSelectionChanger::possibleASTStepCount(CPlusPlus::AST *ast) const -> int
{
  // Different AST nodes, have a different number of steps though which they can go.
  // For example in a string literal, we first want to select the literal contents on the first
  // step, and then the quotes + the literal content in the second step.
  if (!ast)
    return 1;
  if (ast->asCompoundStatement())
    return 2;
  if (ast->asCall())
    return 3;
  if (ast->asStringLiteral())
    return 2;
  if (NumericLiteralAST *numericLiteralAST = ast->asNumericLiteral()) {
    Token firstToken = m_unit->tokenAt(numericLiteralAST->firstToken());
    if (firstToken.isCharLiteral())
      return 2;
    return 1;
  }
  if (ast->asForStatement())
    return 3;
  if (ast->asRangeBasedForStatement())
    return 3;
  if (ast->asClassSpecifier())
    return 3;
  if (ast->asNamespace())
    return 3;
  if (ast->asExpressionListParen())
    return 2;
  if (ast->asFunctionDeclarator())
    return 1;
  if (ast->asFunctionDefinition())
    return 2;
  if (ast->asTemplateId())
    return 2;
  if (ast->asDeclarator())
    return 2;
  if (ast->asTemplateDeclaration())
    return 3;
  if (ast->asLambdaExpression())
    return 3;

  return 1;
}

auto CppSelectionChanger::currentASTStep() const -> int
{
  return m_nodeCurrentStep;
}

} // namespace CppEditor
