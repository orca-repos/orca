// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "builtincursorinfo.hpp"

#include "cppcanonicalsymbol.hpp"
#include "cppcursorinfo.hpp"
#include "cpplocalsymbols.hpp"
#include "cppmodelmanager.hpp"
#include "cppsemanticinfo.hpp"
#include "cpptoolsreuse.hpp"

#include <cplusplus/CppDocument.h>
#include <cplusplus/Macro.h>
#include <cplusplus/TranslationUnit.h>

#include <utils/textutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>

#include <QTextBlock>

using namespace CPlusPlus;

namespace CppEditor {

using SemanticUses = QList<SemanticInfo::Use>;

namespace {

auto toRange(const SemanticInfo::Use &use) -> CursorInfo::Range
{
  return {use.line, use.column, use.length};
}

auto toRange(int tokenIndex, TranslationUnit *translationUnit) -> CursorInfo::Range
{
  int line, column;
  translationUnit->getTokenPosition(tokenIndex, &line, &column);
  if (column)
    --column; // adjust the column position.

  return {line, column + 1, translationUnit->tokenAt(tokenIndex).utf16chars()};
}

auto toRange(const QTextCursor &textCursor, int utf16offset, int length) -> CursorInfo::Range
{
  QTextCursor cursor(textCursor.document());
  cursor.setPosition(utf16offset);
  const auto textBlock = cursor.block();

  return {textBlock.blockNumber() + 1, cursor.position() - textBlock.position() + 1, length};
}

auto toRanges(const SemanticUses &uses) -> CursorInfo::Ranges
{
  CursorInfo::Ranges ranges;
  ranges.reserve(uses.size());

  for (const auto &use : uses)
    ranges.append(toRange(use));

  return ranges;
}

auto toRanges(const QList<int> &tokenIndices, TranslationUnit *translationUnit) -> CursorInfo::Ranges
{
  CursorInfo::Ranges ranges;
  ranges.reserve(tokenIndices.size());

  for (auto reference : tokenIndices)
    ranges.append(toRange(reference, translationUnit));

  return ranges;
}

class FunctionDefinitionUnderCursor : protected ASTVisitor {
  int m_line = 0;
  int m_column = 0;
  DeclarationAST *m_functionDefinition = nullptr;

public:
  explicit FunctionDefinitionUnderCursor(TranslationUnit *translationUnit) : ASTVisitor(translationUnit) { }

  auto operator()(AST *ast, int line, int column) -> DeclarationAST*
  {
    m_functionDefinition = nullptr;
    m_line = line;
    m_column = column;
    accept(ast);
    return m_functionDefinition;
  }

protected:
  auto preVisit(AST *ast) -> bool override
  {
    if (m_functionDefinition)
      return false;

    if (FunctionDefinitionAST *def = ast->asFunctionDefinition())
      return checkDeclaration(def);

    if (ObjCMethodDeclarationAST *method = ast->asObjCMethodDeclaration()) {
      if (method->function_body)
        return checkDeclaration(method);
    }

    return true;
  }

private:
  auto checkDeclaration(DeclarationAST *ast) -> bool
  {
    int startLine, startColumn;
    int endLine, endColumn;
    getTokenStartPosition(ast->firstToken(), &startLine, &startColumn);
    getTokenEndPosition(ast->lastToken() - 1, &endLine, &endColumn);

    if (m_line > startLine || (m_line == startLine && m_column >= startColumn)) {
      if (m_line < endLine || (m_line == endLine && m_column < endColumn)) {
        m_functionDefinition = ast;
        return false;
      }
    }

    return true;
  }
};

class FindUses {
public:
  static auto find(const Document::Ptr document, const Snapshot &snapshot, int line, int column, Scope *scope, const QString &expression) -> CursorInfo
  {
    FindUses findUses(document, snapshot, line, column, scope, expression);
    return findUses.doFind();
  }

private:
  FindUses(const Document::Ptr document, const Snapshot &snapshot, int line, int column, Scope *scope, const QString &expression) : m_document(document), m_line(line), m_column(column), m_scope(scope), m_expression(expression), m_snapshot(snapshot) { }

  auto doFind() const -> CursorInfo
  {
    CursorInfo result;

    // findLocalUses operates with 1-based line and 0-based column
    const SemanticInfo::LocalUseMap localUses = BuiltinCursorInfo::findLocalUses(m_document, m_line, m_column - 1);
    result.localUses = localUses;
    splitLocalUses(localUses, &result.useRanges, &result.unusedVariablesRanges);

    if (!result.useRanges.isEmpty()) {
      result.areUseRangesForLocalVariable = true;
      return result;
    }

    result.useRanges = findReferences();
    result.areUseRangesForLocalVariable = false;
    return result; // OK, result.unusedVariablesRanges will be passed on
  }

  auto splitLocalUses(const SemanticInfo::LocalUseMap &uses, CursorInfo::Ranges *rangesForLocalVariableUnderCursor, CursorInfo::Ranges *rangesForLocalUnusedVariables) const -> void
  {
    QTC_ASSERT(rangesForLocalVariableUnderCursor, return);
    QTC_ASSERT(rangesForLocalUnusedVariables, return);

    LookupContext context(m_document, m_snapshot);

    for (auto it = uses.cbegin(), end = uses.cend(); it != end; ++it) {
      const SemanticUses &uses = it.value();

      auto good = false;
      foreach(const SemanticInfo::Use &use, uses) {
        if (m_line == use.line && m_column >= use.column && m_column <= static_cast<int>(use.column + use.length)) {
          good = true;
          break;
        }
      }

      if (uses.size() == 1) {
        if (!isOwnershipRAIIType(it.key(), context))
          rangesForLocalUnusedVariables->append(toRanges(uses)); // unused declaration
      } else if (good && rangesForLocalVariableUnderCursor->isEmpty()) {
        rangesForLocalVariableUnderCursor->append(toRanges(uses));
      }
    }
  }

  auto findReferences() const -> CursorInfo::Ranges
  {
    CursorInfo::Ranges result;
    if (!m_scope || m_expression.isEmpty())
      return result;

    TypeOfExpression typeOfExpression;
    Snapshot theSnapshot = m_snapshot;
    theSnapshot.insert(m_document);
    typeOfExpression.init(m_document, theSnapshot);
    typeOfExpression.setExpandTemplates(true);

    if (Symbol *s = Internal::CanonicalSymbol::canonicalSymbol(m_scope, m_expression, typeOfExpression)) {
      const QList<int> tokenIndices = CppModelManager::instance()->references(s, typeOfExpression.context());
      result = toRanges(tokenIndices, m_document->translationUnit());
    }

    return result;
  }

  // Shared
  Document::Ptr m_document;

  // For local use calculation
  int m_line;
  int m_column;

  // For references calculation
  Scope *m_scope;
  QString m_expression;
  Snapshot m_snapshot;
};

auto isSemanticInfoValidExceptLocalUses(const SemanticInfo &semanticInfo, int revision) -> bool
{
  return semanticInfo.doc && semanticInfo.revision == static_cast<unsigned>(revision) && !semanticInfo.snapshot.isEmpty();
}

auto isMacroUseOf(const Document::MacroUse &marcoUse, const Macro &macro) -> bool
{
  const Macro &candidate = marcoUse.macro();

  return candidate.line() == macro.line() && candidate.utf16CharOffset() == macro.utf16CharOffset() && candidate.length() == macro.length() && candidate.fileName() == macro.fileName();
}

auto handleMacroCase(const Document::Ptr document, const QTextCursor &textCursor, CursorInfo::Ranges *ranges) -> bool
{
  QTC_ASSERT(ranges, return false);

  const Macro *macro = findCanonicalMacro(textCursor, document);
  if (!macro)
    return false;

  const int length = macro->nameToQString().size();

  // Macro definition
  if (macro->fileName() == document->fileName())
    ranges->append(toRange(textCursor, macro->utf16CharOffset(), length));

  // Other macro uses
  foreach(const Document::MacroUse &use, document->macroUses()) {
    if (isMacroUseOf(use, *macro))
      ranges->append(toRange(textCursor, use.utf16charsBegin(), length));
  }

  return true;
}

} // anonymous namespace

auto BuiltinCursorInfo::run(const CursorInfoParams &cursorInfoParams) -> QFuture<CursorInfo>
{
  QFuture<CursorInfo> nothing;

  const auto semanticInfo = cursorInfoParams.semanticInfo;
  const auto currentDocumentRevision = cursorInfoParams.textCursor.document()->revision();
  if (!isSemanticInfoValidExceptLocalUses(semanticInfo, currentDocumentRevision))
    return nothing;

  const Document::Ptr document = semanticInfo.doc;
  const Snapshot snapshot = semanticInfo.snapshot;
  if (!document)
    return nothing;

  QTC_ASSERT(document->translationUnit(), return nothing);
  QTC_ASSERT(document->translationUnit()->ast(), return nothing);
  QTC_ASSERT(!snapshot.isEmpty(), return nothing);

  CursorInfo::Ranges ranges;
  if (handleMacroCase(document, cursorInfoParams.textCursor, &ranges)) {
    CursorInfo result;
    result.useRanges = ranges;
    result.areUseRangesForLocalVariable = false;

    QFutureInterface<CursorInfo> fi;
    fi.reportResult(result);
    fi.reportFinished();

    return fi.future();
  }

  const auto &textCursor = cursorInfoParams.textCursor;
  int line, column;
  Utils::Text::convertPosition(textCursor.document(), textCursor.position(), &line, &column);
  Internal::CanonicalSymbol canonicalSymbol(document, snapshot);
  QString expression;
  Scope *scope = canonicalSymbol.getScopeAndExpression(textCursor, &expression);

  return Utils::runAsync(&FindUses::find, document, snapshot, line, column, scope, expression);
}

auto BuiltinCursorInfo::findLocalUses(const Document::Ptr &document, int line, int column) -> SemanticInfo::LocalUseMap
{
  if (!document || !document->translationUnit() || !document->translationUnit()->ast())
    return SemanticInfo::LocalUseMap();

  AST *ast = document->translationUnit()->ast();
  FunctionDefinitionUnderCursor functionDefinitionUnderCursor(document->translationUnit());
  DeclarationAST *declaration = functionDefinitionUnderCursor(ast, line, column);
  return Internal::LocalSymbols(document, declaration).uses;
}

} // namespace CppEditor
