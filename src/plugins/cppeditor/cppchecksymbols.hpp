// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cppsemanticinfo.hpp"
#include "semantichighlighter.hpp"

#include <cplusplus/TypeOfExpression.h>

#include <QFuture>
#include <QSet>
#include <QThreadPool>

namespace CppEditor {

class CPPEDITOR_EXPORT CheckSymbols : public QObject, protected CPlusPlus::ASTVisitor, public QRunnable, public QFutureInterface<TextEditor::HighlightingResult> {
  Q_OBJECT

public:
  ~CheckSymbols() override;

  using Result = TextEditor::HighlightingResult;
  using Kind = SemanticHighlighter::Kind;
  using Future = QFuture<Result>;

  auto run() -> void override;

  auto start() -> Future
  {
    this->setRunnable(this);
    this->reportStarted();
    auto future = this->future();
    QThreadPool::globalInstance()->start(this, QThread::LowestPriority);
    return future;
  }

  static auto go(CPlusPlus::Document::Ptr doc, const CPlusPlus::LookupContext &context, const QList<Result> &macroUses) -> Future;
  static auto create(CPlusPlus::Document::Ptr doc, const CPlusPlus::LookupContext &context, const QList<Result> &macroUses) -> CheckSymbols*;

  static auto chunks(const QFuture<Result> &future, int from, int to) -> QMap<int, QVector<Result>>
  {
    QMap<int, QVector<Result>> chunks;

    for (auto i = from; i < to; ++i) {
      const auto use = future.resultAt(i);
      if (use.isInvalid())
        continue;

      const auto blockNumber = use.line - 1;
      chunks[blockNumber].append(use);
    }

    return chunks;
  }

signals:
  auto codeWarningsUpdated(CPlusPlus::Document::Ptr document, const QList<CPlusPlus::Document::DiagnosticMessage> &selections) -> void;

protected:
  using ASTVisitor::visit;
  using ASTVisitor::endVisit;

  enum FunctionKind {
    FunctionDeclaration,
    FunctionCall
  };

  CheckSymbols(CPlusPlus::Document::Ptr doc, const CPlusPlus::LookupContext &context, const QList<Result> &otherUses);

  auto hasVirtualDestructor(CPlusPlus::Class *klass) const -> bool;
  auto hasVirtualDestructor(CPlusPlus::ClassOrNamespace *binding) const -> bool;
  auto warning(unsigned line, unsigned column, const QString &text, unsigned length = 0) -> bool;
  auto warning(CPlusPlus::AST *ast, const QString &text) -> bool;
  auto textOf(CPlusPlus::AST *ast) const -> QByteArray;
  auto maybeType(const CPlusPlus::Name *name) const -> bool;
  auto maybeField(const CPlusPlus::Name *name) const -> bool;
  auto maybeStatic(const CPlusPlus::Name *name) const -> bool;
  auto maybeFunction(const CPlusPlus::Name *name) const -> bool;
  auto checkNamespace(CPlusPlus::NameAST *name) -> void;
  auto checkName(CPlusPlus::NameAST *ast, CPlusPlus::Scope *scope = nullptr) -> void;
  auto checkNestedName(CPlusPlus::QualifiedNameAST *ast) -> CPlusPlus::ClassOrNamespace*;
  auto addUse(const Result &use) -> void;
  auto addUse(unsigned tokenIndex, Kind kind) -> void;
  auto addUse(CPlusPlus::NameAST *name, Kind kind) -> void;
  auto addType(CPlusPlus::ClassOrNamespace *b, CPlusPlus::NameAST *ast) -> void;
  auto maybeAddTypeOrStatic(const QList<CPlusPlus::LookupItem> &candidates, CPlusPlus::NameAST *ast) -> bool;
  auto maybeAddField(const QList<CPlusPlus::LookupItem> &candidates, CPlusPlus::NameAST *ast) -> bool;
  auto maybeAddFunction(const QList<CPlusPlus::LookupItem> &candidates, CPlusPlus::NameAST *ast, int argumentCount, FunctionKind functionKind) -> bool;
  auto isTemplateClass(CPlusPlus::Symbol *s) const -> bool;
  auto enclosingScope() const -> CPlusPlus::Scope*;
  auto enclosingFunctionDefinition(bool skipTopOfStack = false) const -> CPlusPlus::FunctionDefinitionAST*;
  auto enclosingTemplateDeclaration() const -> CPlusPlus::TemplateDeclarationAST*;
  auto preVisit(CPlusPlus::AST *) -> bool override;
  auto postVisit(CPlusPlus::AST *) -> void override;
  auto visit(CPlusPlus::NamespaceAST *) -> bool override;
  auto visit(CPlusPlus::UsingDirectiveAST *) -> bool override;
  auto visit(CPlusPlus::SimpleDeclarationAST *) -> bool override;
  auto visit(CPlusPlus::TypenameTypeParameterAST *ast) -> bool override;
  auto visit(CPlusPlus::TemplateTypeParameterAST *ast) -> bool override;
  auto visit(CPlusPlus::FunctionDefinitionAST *ast) -> bool override;
  auto visit(CPlusPlus::ParameterDeclarationAST *ast) -> bool override;
  auto visit(CPlusPlus::ElaboratedTypeSpecifierAST *ast) -> bool override;
  auto visit(CPlusPlus::ObjCProtocolDeclarationAST *ast) -> bool override;
  auto visit(CPlusPlus::ObjCProtocolForwardDeclarationAST *ast) -> bool override;
  auto visit(CPlusPlus::ObjCClassDeclarationAST *ast) -> bool override;
  auto visit(CPlusPlus::ObjCClassForwardDeclarationAST *ast) -> bool override;
  auto visit(CPlusPlus::ObjCProtocolRefsAST *ast) -> bool override;
  auto visit(CPlusPlus::SimpleNameAST *ast) -> bool override;
  auto visit(CPlusPlus::DestructorNameAST *ast) -> bool override;
  auto visit(CPlusPlus::QualifiedNameAST *ast) -> bool override;
  auto visit(CPlusPlus::TemplateIdAST *ast) -> bool override;
  auto visit(CPlusPlus::MemberAccessAST *ast) -> bool override;
  auto visit(CPlusPlus::CallAST *ast) -> bool override;
  auto visit(CPlusPlus::ObjCSelectorArgumentAST *ast) -> bool override;
  auto visit(CPlusPlus::NewExpressionAST *ast) -> bool override;
  auto visit(CPlusPlus::GotoStatementAST *ast) -> bool override;
  auto visit(CPlusPlus::LabeledStatementAST *ast) -> bool override;
  auto visit(CPlusPlus::SimpleSpecifierAST *ast) -> bool override;
  auto visit(CPlusPlus::ClassSpecifierAST *ast) -> bool override;
  auto visit(CPlusPlus::MemInitializerAST *ast) -> bool override;
  auto visit(CPlusPlus::EnumeratorAST *ast) -> bool override;
  auto visit(CPlusPlus::DotDesignatorAST *ast) -> bool override;
  auto declaratorId(CPlusPlus::DeclaratorAST *ast) const -> CPlusPlus::NameAST*;
  static auto referenceToken(CPlusPlus::NameAST *name) -> unsigned;
  auto flush() -> void;

private:
  auto isConstructorDeclaration(CPlusPlus::Symbol *declaration) -> bool;

  CPlusPlus::Document::Ptr _doc;
  CPlusPlus::LookupContext _context;
  CPlusPlus::TypeOfExpression typeOfExpression;
  QString _fileName;
  QSet<QByteArray> _potentialTypes;
  QSet<QByteArray> _potentialFields;
  QSet<QByteArray> _potentialFunctions;
  QSet<QByteArray> _potentialStatics;
  QList<CPlusPlus::AST*> _astStack;
  QVector<Result> _usages;
  QList<CPlusPlus::Document::DiagnosticMessage> _diagMsgs;
  int _chunkSize;
  int _lineOfLastUsage;
  QList<Result> _macroUses;
};

} // namespace CppEditor
