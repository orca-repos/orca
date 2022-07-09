// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cpprefactoringchanges.hpp"

#include <cplusplus/ASTVisitor.h>

#include <utils/changeset.hpp>

namespace CPlusPlus {
class Overview;
}

namespace CppEditor::Internal {

using namespace CPlusPlus;

/*!
    \class CppEditor::PointerDeclarationFormatter

    \brief The PointerDeclarationFormatter class rewrites pointer or reference
    declarations to an Overview.

    The following constructs are supported:
    \list
     \li Simple declarations
     \li Parameters and return types of function declarations and definitions
     \li Control flow statements like if, while, for, foreach
    \endlist
*/

class PointerDeclarationFormatter : protected ASTVisitor {
public:
  /*!
      \enum PointerDeclarationFormatter::CursorHandling

      This enum type simplifies the QuickFix implementation.

        \value RespectCursor
               Consider the cursor position or selection of the CppRefactoringFile
               for rejecting edit operation candidates for the resulting ChangeSet.
               If there is a selection, the range of the edit operation candidate
               should be inside the selection. If there is no selection, the cursor
               position should be within the range of the edit operation candidate.
        \value IgnoreCursor
               Cursor position or selection of the CppRefactoringFile will
              _not_ be considered for aborting.
   */
  enum CursorHandling {
    RespectCursor,
    IgnoreCursor
  };

  explicit PointerDeclarationFormatter(const CppRefactoringFilePtr &refactoringFile, Overview &overview, CursorHandling cursorHandling = IgnoreCursor);

  /*!
      Returns a ChangeSet for applying the formatting changes.
      The ChangeSet is empty if it was not possible to rewrite anything.
  */
  auto format(AST *ast) -> Utils::ChangeSet
  {
    if (ast)
      accept(ast);
    return m_changeSet;
  }

protected:
  auto visit(SimpleDeclarationAST *ast) -> bool override;
  auto visit(FunctionDefinitionAST *ast) -> bool override;
  auto visit(ParameterDeclarationAST *ast) -> bool override;
  auto visit(IfStatementAST *ast) -> bool override;
  auto visit(WhileStatementAST *ast) -> bool override;
  auto visit(ForStatementAST *ast) -> bool override;
  auto visit(ForeachStatementAST *ast) -> bool override;

private:
  class TokenRange {
  public:
    TokenRange() = default;
    TokenRange(int start, int end) : start(start), end(end) {}

    int start = 0;
    int end = 0;
  };

  auto processIfWhileForStatement(ExpressionAST *expression, Symbol *symbol) -> void;
  auto checkAndRewrite(DeclaratorAST *declarator, Symbol *symbol, TokenRange range, unsigned charactersToRemove = 0) -> void;
  auto printCandidate(AST *ast) -> void;

  const CppRefactoringFilePtr m_cppRefactoringFile;
  Overview &m_overview;
  const CursorHandling m_cursorHandling;
  Utils::ChangeSet m_changeSet;
};

} // namespace CppEditor::Internal
