// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#if defined(_MSC_VER)
#pragma warning(disable:4996)
#endif

#include "symbolfinder.hpp"

#include "cppmodelmanager.hpp"

#include <cplusplus/LookupContext.h>

#include <utils/qtcassert.hpp>

#include <QDebug>
#include <QPair>

#include <algorithm>
#include <utility>

using namespace CPlusPlus;

namespace CppEditor {
namespace {

struct Hit {
  Hit(Function *func, bool exact) : func(func), exact(exact) {}
  Hit() = default;

  Function *func = nullptr;
  bool exact = false;
};

class FindMatchingDefinition : public SymbolVisitor {
  Symbol *_declaration = nullptr;
  const OperatorNameId *_oper = nullptr;
  const ConversionNameId *_conv = nullptr;
  const bool _strict;
  QList<Hit> _result;

public:
  explicit FindMatchingDefinition(Symbol *declaration, bool strict) : _declaration(declaration), _strict(strict)
  {
    if (_declaration->name()) {
      _oper = _declaration->name()->asOperatorNameId();
      _conv = _declaration->name()->asConversionNameId();
    }
  }

  auto result() const -> const QList<Hit> { return _result; }

  using SymbolVisitor::visit;

  auto visit(Function *fun) -> bool override
  {
    if (_oper || _conv) {
      if (const Name *name = fun->unqualifiedName()) {
        if ((_oper && _oper->match(name)) || (_conv && _conv->match(name)))
          _result.append({fun, true});
      }
    } else if (Function *decl = _declaration->type()->asFunctionType()) {
      if (fun->match(decl)) {
        _result.prepend({fun, true});
      } else if (!_strict && Matcher::match(fun->unqualifiedName(), decl->unqualifiedName())) {
        _result.append({fun, false});
      }
    }

    return false;
  }

  auto visit(Block *) -> bool override
  {
    return false;
  }
};

class FindMatchingVarDefinition : public SymbolVisitor {
  Symbol *_declaration = nullptr;
  QList<Declaration*> _result;
  const Identifier *_className = nullptr;

public:
  explicit FindMatchingVarDefinition(Symbol *declaration) : _declaration(declaration)
  {
    if (declaration->isStatic() && declaration->enclosingScope()->asClass() && declaration->enclosingClass()->asClass()->name()) {
      _className = declaration->enclosingScope()->name()->identifier();
    }
  }

  auto result() const -> const QList<Declaration*> { return _result; }

  using SymbolVisitor::visit;

  auto visit(Declaration *decl) -> bool override
  {
    if (!decl->type()->match(_declaration->type().type()))
      return false;
    if (!_declaration->identifier()->equalTo(decl->identifier()))
      return false;
    if (_className) {
      const QualifiedNameId *const qualName = decl->name()->asQualifiedNameId();
      if (!qualName)
        return false;
      if (!qualName->base() || !qualName->base()->identifier()->equalTo(_className))
        return false;
    }
    _result.append(decl);
    return false;
  }

  auto visit(Block *) -> bool override { return false; }
};

} // end of anonymous namespace

static const int kMaxCacheSize = 10;

SymbolFinder::SymbolFinder() = default;

// strict means the returned symbol has to match exactly,
// including argument count, argument types, constness and volatileness.
auto SymbolFinder::findMatchingDefinition(Symbol *declaration, const Snapshot &snapshot, bool strict) -> Function*
{
  if (!declaration)
    return nullptr;

  auto declFile = QString::fromUtf8(declaration->fileName(), declaration->fileNameLength());

  Document::Ptr thisDocument = snapshot.document(declFile);
  if (!thisDocument) {
    qWarning() << "undefined document:" << declaration->fileName();
    return nullptr;
  }

  Function *declarationTy = declaration->type()->asFunctionType();
  if (!declarationTy) {
    qWarning() << "not a function:" << declaration->fileName() << declaration->line() << declaration->column();
    return nullptr;
  }

  Hit best;
  foreach(const QString &fileName, fileIterationOrder(declFile, snapshot)) {
    Document::Ptr doc = snapshot.document(fileName);
    if (!doc) {
      clearCache(declFile, fileName);
      continue;
    }

    const Identifier *id = declaration->identifier();
    if (id && !doc->control()->findIdentifier(id->chars(), id->size()))
      continue;

    if (!id) {
      const Name *const name = declaration->name();
      if (!name)
        continue;
      if (const OperatorNameId *const oper = name->asOperatorNameId()) {
        if (!doc->control()->findOperatorNameId(oper->kind()))
          continue;
      } else if (const ConversionNameId *const conv = name->asConversionNameId()) {
        if (!doc->control()->findConversionNameId(conv->type()))
          continue;
      } else {
        continue;
      }
    }

    FindMatchingDefinition candidates(declaration, strict);
    candidates.accept(doc->globalNamespace());

    const auto result = candidates.result();
    if (result.isEmpty())
      continue;

    LookupContext context(doc, snapshot);
    ClassOrNamespace *enclosingType = context.lookupType(declaration);
    if (!enclosingType)
      continue; // nothing to do

    for (const auto &hit : result) {
      QTC_CHECK(!strict || hit.exact);

      const QList<LookupItem> declarations = context.lookup(hit.func->name(), hit.func->enclosingScope());
      if (declarations.isEmpty())
        continue;
      if (enclosingType != context.lookupType(declarations.first().declaration()))
        continue;

      if (hit.exact)
        return hit.func;

      if (!best.func || hit.func->argumentCount() == declarationTy->argumentCount())
        best = hit;
    }
  }

  QTC_CHECK(!best.exact);
  return strict ? nullptr : best.func;
}

auto SymbolFinder::findMatchingVarDefinition(Symbol *declaration, const Snapshot &snapshot) -> Symbol*
{
  if (!declaration)
    return nullptr;
  for (const Scope *s = declaration->enclosingScope(); s; s = s->enclosingScope()) {
    if (s->asBlock())
      return nullptr;
  }

  auto declFile = QString::fromUtf8(declaration->fileName(), declaration->fileNameLength());
  const Document::Ptr thisDocument = snapshot.document(declFile);
  if (!thisDocument) {
    qWarning() << "undefined document:" << declaration->fileName();
    return nullptr;
  }

  using SymbolWithPriority = QPair<Symbol*, bool>;
  QList<SymbolWithPriority> candidates;
  QList<SymbolWithPriority> fallbacks;
  foreach(const QString &fileName, fileIterationOrder(declFile, snapshot)) {
    Document::Ptr doc = snapshot.document(fileName);
    if (!doc) {
      clearCache(declFile, fileName);
      continue;
    }

    const Identifier *id = declaration->identifier();
    if (id && !doc->control()->findIdentifier(id->chars(), id->size()))
      continue;

    FindMatchingVarDefinition finder(declaration);
    finder.accept(doc->globalNamespace());
    if (finder.result().isEmpty())
      continue;

    LookupContext context(doc, snapshot);
    ClassOrNamespace *const enclosingType = context.lookupType(declaration);
    for (Symbol *const symbol : finder.result()) {
      const QList<LookupItem> items = context.lookup(symbol->name(), symbol->enclosingScope());
      auto addFallback = true;
      for (const LookupItem &item : items) {
        if (item.declaration() == symbol)
          addFallback = false;
        candidates << qMakePair(item.declaration(), context.lookupType(item.declaration()) == enclosingType);
      }
      // TODO: This is a workaround for static member definitions not being found by
      //       the lookup() function.
      if (addFallback)
        fallbacks << qMakePair(symbol, context.lookupType(symbol) == enclosingType);
    }
  }

  candidates << fallbacks;
  SymbolWithPriority best;
  for (const auto &candidate : qAsConst(candidates)) {
    if (candidate.first == declaration)
      continue;
    if (QLatin1String(candidate.first->fileName()) == declFile && candidate.first->sourceLocation() == declaration->sourceLocation())
      continue;
    if (!candidate.first->asDeclaration())
      continue;
    if (declaration->isExtern() && candidate.first->isStatic())
      continue;
    if (!best.first) {
      best = candidate;
      continue;
    }
    if (!best.second && candidate.second) {
      best = candidate;
      continue;
    }
    if (best.first->isExtern() && !candidate.first->isExtern())
      best = candidate;
  }

  return best.first;
}

auto SymbolFinder::findMatchingClassDeclaration(Symbol *declaration, const Snapshot &snapshot) -> Class*
{
  if (!declaration->identifier())
    return nullptr;

  auto declFile = QString::fromUtf8(declaration->fileName(), declaration->fileNameLength());

  foreach(const QString &file, fileIterationOrder(declFile, snapshot)) {
    Document::Ptr doc = snapshot.document(file);
    if (!doc) {
      clearCache(declFile, file);
      continue;
    }

    if (!doc->control()->findIdentifier(declaration->identifier()->chars(), declaration->identifier()->size()))
      continue;

    LookupContext context(doc, snapshot);

    ClassOrNamespace *type = context.lookupType(declaration);
    if (!type)
      continue;

    foreach(Symbol *s, type->symbols()) {
      if (Class *c = s->asClass())
        return c;
    }
  }

  return nullptr;
}

static auto findDeclarationOfSymbol(Symbol *s, Function *functionType, QList<Declaration*> *typeMatch, QList<Declaration*> *argumentCountMatch, QList<Declaration*> *nameMatch) -> void
{
  if (Declaration *decl = s->asDeclaration()) {
    if (Function *declFunTy = decl->type()->asFunctionType()) {
      if (functionType->match(declFunTy))
        typeMatch->prepend(decl);
      else if (functionType->argumentCount() == declFunTy->argumentCount())
        argumentCountMatch->prepend(decl);
      else
        nameMatch->append(decl);
    }
  }
}

auto SymbolFinder::findMatchingDeclaration(const LookupContext &context, Function *functionType, QList<Declaration*> *typeMatch, QList<Declaration*> *argumentCountMatch, QList<Declaration*> *nameMatch) -> void
{
  if (!functionType)
    return;

  Scope *enclosingScope = functionType->enclosingScope();
  while (!(enclosingScope->isNamespace() || enclosingScope->isClass()))
    enclosingScope = enclosingScope->enclosingScope();
  QTC_ASSERT(enclosingScope != nullptr, return);

  const Name *functionName = functionType->name();
  if (!functionName)
    return;

  ClassOrNamespace *binding = nullptr;
  const QualifiedNameId *qName = functionName->asQualifiedNameId();
  if (qName) {
    if (qName->base())
      binding = context.lookupType(qName->base(), enclosingScope);
    else
      binding = context.globalNamespace();
    functionName = qName->name();
  }

  if (!binding) {
    // declaration for a global function
    binding = context.lookupType(enclosingScope);

    if (!binding)
      return;
  }

  const Identifier *funcId = functionName->identifier();
  OperatorNameId::Kind operatorNameId = OperatorNameId::InvalidOp;

  if (!funcId) {
    if (!qName)
      return;
    const OperatorNameId *const onid = qName->name()->asOperatorNameId();
    if (!onid)
      return;
    operatorNameId = onid->kind();
  }

  foreach(Symbol *s, binding->symbols()) {
    Scope *scope = s->asScope();
    if (!scope)
      continue;

    if (funcId) {
      for (Symbol *s = scope->find(funcId); s; s = s->next()) {
        if (!s->name() || !funcId->match(s->identifier()) || !s->type()->isFunctionType())
          continue;
        findDeclarationOfSymbol(s, functionType, typeMatch, argumentCountMatch, nameMatch);
      }
    } else {
      for (Symbol *s = scope->find(operatorNameId); s; s = s->next()) {
        if (!s->name() || !s->type()->isFunctionType())
          continue;
        findDeclarationOfSymbol(s, functionType, typeMatch, argumentCountMatch, nameMatch);
      }
    }
  }
}

auto SymbolFinder::findMatchingDeclaration(const LookupContext &context, Function *functionType) -> QList<Declaration*>
{
  QList<Declaration*> result;
  if (!functionType)
    return result;

  QList<Declaration*> nameMatch, argumentCountMatch, typeMatch;
  findMatchingDeclaration(context, functionType, &typeMatch, &argumentCountMatch, &nameMatch);
  result.append(typeMatch);

  // For member functions not defined inline, add fuzzy matches as fallbacks. We cannot do
  // this for free functions, because there is no guarantee that there's a separate declaration.
  auto fuzzyMatches = argumentCountMatch + nameMatch;
  if (!functionType->enclosingScope() || !functionType->enclosingScope()->isClass()) {
    for (const auto d : fuzzyMatches) {
      if (d->enclosingScope() && d->enclosingScope()->isClass())
        result.append(d);
    }
  }
  return result;
}

auto SymbolFinder::fileIterationOrder(const QString &referenceFile, const Snapshot &snapshot) -> QStringList
{
  if (m_filePriorityCache.contains(referenceFile)) {
    checkCacheConsistency(referenceFile, snapshot);
  } else {
    foreach(Document::Ptr doc, snapshot)
      insertCache(referenceFile, doc->fileName());
  }

  auto files = m_filePriorityCache.value(referenceFile).toStringList();

  trackCacheUse(referenceFile);

  return files;
}

auto SymbolFinder::clearCache() -> void
{
  m_filePriorityCache.clear();
  m_fileMetaCache.clear();
  m_recent.clear();
}

auto SymbolFinder::checkCacheConsistency(const QString &referenceFile, const Snapshot &snapshot) -> void
{
  // We only check for "new" files, which which are in the snapshot but not in the cache.
  // The counterpart validation for "old" files is done when one tries to access the
  // corresponding document and notices it's now null.
  const auto &meta = m_fileMetaCache.value(referenceFile);
  foreach(const Document::Ptr &doc, snapshot) {
    if (!meta.contains(doc->fileName()))
      insertCache(referenceFile, doc->fileName());
  }
}

auto projectPartIdForFile(const QString &filePath) -> const QString
{
  const auto parts = CppModelManager::instance()->projectPart(filePath);
  if (!parts.isEmpty())
    return parts.first()->id();
  return QString();
}

auto SymbolFinder::clearCache(const QString &referenceFile, const QString &comparingFile) -> void
{
  m_filePriorityCache[referenceFile].remove(comparingFile, projectPartIdForFile(comparingFile));
  m_fileMetaCache[referenceFile].remove(comparingFile);
}

auto SymbolFinder::insertCache(const QString &referenceFile, const QString &comparingFile) -> void
{
  auto &order = m_filePriorityCache[referenceFile];
  if (!order.isValid()) {
    const auto projectPartId = projectPartIdForFile(referenceFile);
    order.setReference(referenceFile, projectPartId);
  }
  order.insert(comparingFile, projectPartIdForFile(comparingFile));

  m_fileMetaCache[referenceFile].insert(comparingFile);
}

auto SymbolFinder::trackCacheUse(const QString &referenceFile) -> void
{
  if (!m_recent.isEmpty()) {
    if (m_recent.last() == referenceFile)
      return;
    m_recent.removeOne(referenceFile);
  }

  m_recent.append(referenceFile);

  // We don't want this to grow too much.
  if (m_recent.size() > kMaxCacheSize) {
    const auto &oldest = m_recent.takeFirst();
    m_filePriorityCache.remove(oldest);
    m_fileMetaCache.remove(oldest);
  }
}

} // namespace CppEditor
