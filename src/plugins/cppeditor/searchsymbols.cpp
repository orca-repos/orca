// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "searchsymbols.hpp"
#include "stringtable.hpp"

#include <cplusplus/Icons.h>
#include <cplusplus/LookupContext.h>
#include <utils/qtcassert.hpp>
#include <utils/scopedswap.hpp>

#include <QDebug>

using namespace CPlusPlus;

namespace CppEditor {

using ScopedIndexItemPtr = Utils::ScopedSwap<IndexItem::Ptr>;
using ScopedScope = Utils::ScopedSwap<QString>;

SearchSymbols::SymbolTypes SearchSymbols::AllTypes = SymbolSearcher::Classes | SymbolSearcher::Functions | SymbolSearcher::Enums | SymbolSearcher::Declarations;

SearchSymbols::SearchSymbols() : symbolsToSearchFor(SymbolSearcher::Classes | SymbolSearcher::Functions | SymbolSearcher::Enums)
{
  overview.showTemplateParameters = true;
}

auto SearchSymbols::setSymbolsToSearchFor(const SymbolTypes &types) -> void
{
  symbolsToSearchFor = types;
}

auto SearchSymbols::operator()(Document::Ptr doc, const QString &scope) -> IndexItem::Ptr
{
  IndexItem::Ptr root = IndexItem::create(Internal::StringTable::insert(doc->fileName()), 100);

  {
    // RAII scope
    ScopedIndexItemPtr parentRaii(_parent, root);
    auto newScope = scope;
    ScopedScope scopeRaii(_scope, newScope);

    QTC_ASSERT(_parent, return IndexItem::Ptr());
    QTC_ASSERT(root, return IndexItem::Ptr());
    QTC_ASSERT(_parent->fileName() == Internal::StringTable::insert(doc->fileName()), return IndexItem::Ptr());

    for (int i = 0, ei = doc->globalSymbolCount(); i != ei; ++i)
      accept(doc->globalSymbolAt(i));

    Internal::StringTable::scheduleGC();
    m_paths.clear();
  }

  root->squeeze();
  return root;
}

auto SearchSymbols::visit(Enum *symbol) -> bool
{
  if (!(symbolsToSearchFor & SymbolSearcher::Enums))
    return false;

  QString name = overview.prettyName(symbol->name());
  IndexItem::Ptr newParent = addChildItem(name, QString(), _scope, IndexItem::Enum, symbol);
  if (!newParent)
    newParent = _parent;
  ScopedIndexItemPtr parentRaii(_parent, newParent);

  QString newScope = scopedSymbolName(name, symbol);
  ScopedScope scopeRaii(_scope, newScope);

  for (int i = 0, ei = symbol->memberCount(); i != ei; ++i)
    accept(symbol->memberAt(i));

  return false;
}

auto SearchSymbols::visit(Function *symbol) -> bool
{
  processFunction(symbol);
  return false;
}

auto SearchSymbols::visit(Namespace *symbol) -> bool
{
  QString name = scopedSymbolName(symbol);
  QString newScope = name;
  ScopedScope raii(_scope, newScope);
  for (int i = 0; i < symbol->memberCount(); ++i) {
    accept(symbol->memberAt(i));
  }
  return false;
}

auto SearchSymbols::visit(Declaration *symbol) -> bool
{
  if (!(symbolsToSearchFor & SymbolSearcher::Declarations)) {
    if ((symbolsToSearchFor & SymbolSearcher::TypeAliases) && symbol->type().isTypedef()) {
      // Continue.
    } else if (symbolsToSearchFor & SymbolSearcher::Functions) {
      // if we're searching for functions, still allow signal declarations to show up.
      Function *funTy = symbol->type()->asFunctionType();
      if (!funTy) {
        if (!symbol->type()->asObjCMethodType())
          return false;
      } else if (!funTy->isSignal()) {
        return false;
      }
    } else {
      return false;
    }
  }

  if (symbol->name()) {
    QString name = overview.prettyName(symbol->name());
    QString type = overview.prettyType(symbol->type());
    addChildItem(name, type, _scope, symbol->type()->asFunctionType() ? IndexItem::Function : IndexItem::Declaration, symbol);
  }

  return false;
}

auto SearchSymbols::visit(Class *symbol) -> bool
{
  processClass(symbol);

  return false;
}

auto SearchSymbols::visit(UsingNamespaceDirective *) -> bool
{
  return false;
}

auto SearchSymbols::visit(UsingDeclaration *) -> bool
{
  return false;
}

auto SearchSymbols::visit(NamespaceAlias *) -> bool
{
  return false;
}

auto SearchSymbols::visit(Argument *) -> bool
{
  return false;
}

auto SearchSymbols::visit(TypenameArgument *) -> bool
{
  return false;
}

auto SearchSymbols::visit(BaseClass *) -> bool
{
  return false;
}

auto SearchSymbols::visit(Template *) -> bool
{
  return true;
}

auto SearchSymbols::visit(Block *) -> bool
{
  return false;
}

auto SearchSymbols::visit(ForwardClassDeclaration *) -> bool
{
  return false;
}

auto SearchSymbols::visit(ObjCBaseClass *) -> bool
{
  return false;
}

auto SearchSymbols::visit(ObjCBaseProtocol *) -> bool
{
  return false;
}

auto SearchSymbols::visit(ObjCClass *symbol) -> bool
{
  processClass(symbol);

  return false;
}

auto SearchSymbols::visit(ObjCForwardClassDeclaration *) -> bool
{
  return false;
}

auto SearchSymbols::visit(ObjCProtocol *symbol) -> bool
{
  processClass(symbol);

  return false;
}

auto SearchSymbols::visit(ObjCForwardProtocolDeclaration *) -> bool
{
  return false;
}

auto SearchSymbols::visit(ObjCMethod *symbol) -> bool
{
  processFunction(symbol);
  return false;
}

auto SearchSymbols::visit(ObjCPropertyDeclaration *symbol) -> bool
{
  processFunction(symbol);
  return false;
}

auto SearchSymbols::scopedSymbolName(const QString &symbolName, const Symbol *symbol) const -> QString
{
  auto name = _scope;
  if (!name.isEmpty())
    name += QLatin1String("::");
  name += scopeName(symbolName, symbol);
  return name;
}

auto SearchSymbols::scopedSymbolName(const Symbol *symbol) const -> QString
{
  return scopedSymbolName(overview.prettyName(symbol->name()), symbol);
}

auto SearchSymbols::scopeName(const QString &name, const Symbol *symbol) const -> QString
{
  if (!name.isEmpty())
    return name;

  if (symbol->isNamespace()) {
    return QLatin1String("<anonymous namespace>");
  } else if (symbol->isEnum()) {
    return QLatin1String("<anonymous enum>");
  } else if (const Class *c = symbol->asClass()) {
    if (c->isUnion())
      return QLatin1String("<anonymous union>");
    else if (c->isStruct())
      return QLatin1String("<anonymous struct>");
    else
      return QLatin1String("<anonymous class>");
  } else {
    return QLatin1String("<anonymous symbol>");
  }
}

auto SearchSymbols::addChildItem(const QString &symbolName, const QString &symbolType, const QString &symbolScope, IndexItem::ItemType itemType, Symbol *symbol) -> IndexItem::Ptr
{
  if (!symbol->name() || symbol->isGenerated())
    return IndexItem::Ptr();

  QString path = m_paths.value(symbol->fileId(), QString());
  if (path.isEmpty()) {
    path = QString::fromUtf8(symbol->fileName(), symbol->fileNameLength());
    m_paths.insert(symbol->fileId(), path);
  }

  const QIcon icon = Icons::iconForSymbol(symbol);

  IndexItem::Ptr newItem = IndexItem::create(Internal::StringTable::insert(symbolName), Internal::StringTable::insert(symbolType), Internal::StringTable::insert(symbolScope), itemType, Internal::StringTable::insert(path), symbol->line(), symbol->column() - 1, // 1-based vs 0-based column
                                             icon);
  _parent->addChild(newItem);
  return newItem;
}

template <class T>
auto SearchSymbols::processClass(T *clazz) -> void
{
  QString name = overview.prettyName(clazz->name());

  IndexItem::Ptr newParent;
  if (symbolsToSearchFor & SymbolSearcher::Classes)
    newParent = addChildItem(name, QString(), _scope, IndexItem::Class, clazz);
  if (!newParent)
    newParent = _parent;
  ScopedIndexItemPtr parentRaii(_parent, newParent);

  QString newScope = scopedSymbolName(name, clazz);
  ScopedScope scopeRaii(_scope, newScope);
  for (int i = 0, ei = clazz->memberCount(); i != ei; ++i)
    accept(clazz->memberAt(i));
}

template <class T>
auto SearchSymbols::processFunction(T *func) -> void
{
  if (!(symbolsToSearchFor & SymbolSearcher::Functions) || !func->name())
    return;
  QString name = overview.prettyName(func->name());
  QString type = overview.prettyType(func->type());
  addChildItem(name, type, _scope, IndexItem::Function, func);
}

} // namespace CppEditor
