// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpplocalsymbols.hpp"

#include "cppsemanticinfo.hpp"
#include "semantichighlighter.hpp"

using namespace CPlusPlus;

namespace CppEditor::Internal {

namespace {

class FindLocalSymbols : protected ASTVisitor {
public:
  explicit FindLocalSymbols(Document::Ptr doc) : ASTVisitor(doc->translationUnit()) { }

  // local and external uses.
  SemanticInfo::LocalUseMap localUses;

  auto operator()(DeclarationAST *ast) -> void
  {
    localUses.clear();

    if (!ast)
      return;

    if (FunctionDefinitionAST *def = ast->asFunctionDefinition()) {
      if (def->symbol) {
        accept(ast);
      }
    } else if (ObjCMethodDeclarationAST *decl = ast->asObjCMethodDeclaration()) {
      if (decl->method_prototype->symbol) {
        accept(ast);
      }
    }
  }

protected:
  using ASTVisitor::visit;
  using ASTVisitor::endVisit;

  using HighlightingResult = TextEditor::HighlightingResult;

  auto enterScope(Scope *scope) -> void
  {
    _scopeStack.append(scope);

    for (auto i = 0; i < scope->memberCount(); ++i) {
      if (Symbol *member = scope->memberAt(i)) {
        if (member->isTypedef())
          continue;
        if (!member->isGenerated() && (member->isDeclaration() || member->isArgument())) {
          if (member->name() && member->name()->isNameId()) {
            const Token token = tokenAt(member->sourceLocation());
            int line, column;
            getPosition(token.utf16charsBegin(), &line, &column);
            localUses[member].append(HighlightingResult(line, column, token.utf16chars(), SemanticHighlighter::LocalUse));
          }
        }
      }
    }
  }

  auto checkLocalUse(NameAST *nameAst, int firstToken) -> bool
  {
    if (SimpleNameAST *simpleName = nameAst->asSimpleName()) {
      const Token token = tokenAt(simpleName->identifier_token);
      if (token.generated())
        return false;
      const Identifier *id = identifier(simpleName->identifier_token);
      for (int i = _scopeStack.size() - 1; i != -1; --i) {
        if (Symbol *member = _scopeStack.at(i)->find(id)) {
          if (member->isTypedef() || !(member->isDeclaration() || member->isArgument()))
            continue;
          if (!member->isGenerated() && (member->sourceLocation() < firstToken || member->enclosingScope()->isFunction())) {
            int line, column;
            getTokenStartPosition(simpleName->identifier_token, &line, &column);
            localUses[member].append(HighlightingResult(line, column, token.utf16chars(), SemanticHighlighter::LocalUse));
            return false;
          }
        }
      }
    }

    return true;
  }

  auto visit(CaptureAST *ast) -> bool override
  {
    return checkLocalUse(ast->identifier, ast->firstToken());
  }

  auto visit(IdExpressionAST *ast) -> bool override
  {
    return checkLocalUse(ast->name, ast->firstToken());
  }

  auto visit(SizeofExpressionAST *ast) -> bool override
  {
    if (ast->expression && ast->expression->asTypeId()) {
      TypeIdAST *typeId = ast->expression->asTypeId();
      if (!typeId->declarator && typeId->type_specifier_list && !typeId->type_specifier_list->next) {
        if (NamedTypeSpecifierAST *namedTypeSpec = typeId->type_specifier_list->value->asNamedTypeSpecifier()) {
          if (checkLocalUse(namedTypeSpec->name, namedTypeSpec->firstToken()))
            return false;
        }
      }
    }

    return true;
  }

  auto visit(CastExpressionAST *ast) -> bool override
  {
    if (ast->expression && ast->expression->asUnaryExpression()) {
      TypeIdAST *typeId = ast->type_id->asTypeId();
      if (typeId && !typeId->declarator && typeId->type_specifier_list && !typeId->type_specifier_list->next) {
        if (NamedTypeSpecifierAST *namedTypeSpec = typeId->type_specifier_list->value->asNamedTypeSpecifier()) {
          if (checkLocalUse(namedTypeSpec->name, namedTypeSpec->firstToken())) {
            accept(ast->expression);
            return false;
          }
        }
      }
    }

    return true;
  }

  auto visit(FunctionDefinitionAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(FunctionDefinitionAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(LambdaExpressionAST *ast) -> bool override
  {
    if (ast->lambda_declarator && ast->lambda_declarator->symbol)
      enterScope(ast->lambda_declarator->symbol);
    return true;
  }

  auto endVisit(LambdaExpressionAST *ast) -> void override
  {
    if (ast->lambda_declarator && ast->lambda_declarator->symbol)
      _scopeStack.removeLast();
  }

  auto visit(CompoundStatementAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(CompoundStatementAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(IfStatementAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(IfStatementAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(WhileStatementAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(WhileStatementAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(ForStatementAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(ForStatementAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(ForeachStatementAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(ForeachStatementAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(RangeBasedForStatementAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(RangeBasedForStatementAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(SwitchStatementAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(SwitchStatementAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(CatchClauseAST *ast) -> bool override
  {
    if (ast->symbol)
      enterScope(ast->symbol);
    return true;
  }

  auto endVisit(CatchClauseAST *ast) -> void override
  {
    if (ast->symbol)
      _scopeStack.removeLast();
  }

  auto visit(ExpressionOrDeclarationStatementAST *ast) -> bool override
  {
    accept(ast->declaration);
    return false;
  }

private:
  QList<Scope*> _scopeStack;
};

} // end of anonymous namespace

LocalSymbols::LocalSymbols(Document::Ptr doc, DeclarationAST *ast)
{
  FindLocalSymbols findLocalSymbols(doc);
  findLocalSymbols(ast);
  uses = findLocalSymbols.localUses;
}

} // namespace CppEditor::Internal
