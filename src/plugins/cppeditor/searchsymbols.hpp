// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cppindexingsupport.hpp"
#include "indexitem.hpp"

#include <cplusplus/CppDocument.h>
#include <cplusplus/Overview.h>

#include <QString>
#include <QSet>
#include <QHash>

namespace CppEditor {

class SearchSymbols : protected CPlusPlus::SymbolVisitor {
public:
  using SymbolTypes = SymbolSearcher::SymbolTypes;
  static SymbolTypes AllTypes;

  SearchSymbols();

  auto setSymbolsToSearchFor(const SymbolTypes &types) -> void;
  auto operator()(CPlusPlus::Document::Ptr doc) -> IndexItem::Ptr { return operator()(doc, QString()); }
  auto operator()(CPlusPlus::Document::Ptr doc, const QString &scope) -> IndexItem::Ptr;

protected:
  using SymbolVisitor::visit;

  auto accept(CPlusPlus::Symbol *symbol) -> void { CPlusPlus::Symbol::visitSymbol(symbol, this); }
  auto visit(CPlusPlus::UsingNamespaceDirective *) -> bool override;
  auto visit(CPlusPlus::UsingDeclaration *) -> bool override;
  auto visit(CPlusPlus::NamespaceAlias *) -> bool override;
  auto visit(CPlusPlus::Declaration *) -> bool override;
  auto visit(CPlusPlus::Argument *) -> bool override;
  auto visit(CPlusPlus::TypenameArgument *) -> bool override;
  auto visit(CPlusPlus::BaseClass *) -> bool override;
  auto visit(CPlusPlus::Enum *) -> bool override;
  auto visit(CPlusPlus::Function *) -> bool override;
  auto visit(CPlusPlus::Namespace *) -> bool override;
  auto visit(CPlusPlus::Template *) -> bool override;
  auto visit(CPlusPlus::Class *) -> bool override;
  auto visit(CPlusPlus::Block *) -> bool override;
  auto visit(CPlusPlus::ForwardClassDeclaration *) -> bool override;

  // Objective-C
  auto visit(CPlusPlus::ObjCBaseClass *) -> bool override;
  auto visit(CPlusPlus::ObjCBaseProtocol *) -> bool override;
  auto visit(CPlusPlus::ObjCClass *symbol) -> bool override;
  auto visit(CPlusPlus::ObjCForwardClassDeclaration *) -> bool override;
  auto visit(CPlusPlus::ObjCProtocol *symbol) -> bool override;
  auto visit(CPlusPlus::ObjCForwardProtocolDeclaration *) -> bool override;
  auto visit(CPlusPlus::ObjCMethod *symbol) -> bool override;
  auto visit(CPlusPlus::ObjCPropertyDeclaration *symbol) -> bool override;

  auto scopedSymbolName(const QString &symbolName, const CPlusPlus::Symbol *symbol) const -> QString;
  auto scopedSymbolName(const CPlusPlus::Symbol *symbol) const -> QString;
  auto scopeName(const QString &name, const CPlusPlus::Symbol *symbol) const -> QString;
  auto addChildItem(const QString &symbolName, const QString &symbolType, const QString &symbolScope, IndexItem::ItemType type, CPlusPlus::Symbol *symbol) -> IndexItem::Ptr;

private:
  template <class T>
  auto processClass(T *clazz) -> void;
  template <class T>
  auto processFunction(T *func) -> void;
  
  IndexItem::Ptr _parent;
  QString _scope;
  CPlusPlus::Overview overview;
  SymbolTypes symbolsToSearchFor;
  QHash<const CPlusPlus::StringLiteral*, QString> m_paths;
};

} // namespace CppEditor

Q_DECLARE_OPERATORS_FOR_FLAGS(CppEditor::SearchSymbols::SymbolTypes)
