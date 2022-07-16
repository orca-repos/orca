// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <cplusplus/CppDocument.h>
#include <cplusplus/Overview.h>

#include <QFutureInterface>
#include <QList>
#include <QSet>

#include <set>

namespace CPlusPlus {
class LookupContext;
class LookupItem;
class Name;
class Scope;
}

namespace CppEditor::Internal {

class TypeHierarchy {
  friend class TypeHierarchyBuilder;

public:
  TypeHierarchy();
  explicit TypeHierarchy(CPlusPlus::Symbol *symbol);

  auto symbol() const -> CPlusPlus::Symbol*;
  auto hierarchy() const -> const QList<TypeHierarchy>&;
  auto operator==(const TypeHierarchy &other) const -> bool { return _symbol == other._symbol; }

private:
  CPlusPlus::Symbol *_symbol = nullptr;
  QList<TypeHierarchy> _hierarchy;
};

class TypeHierarchyBuilder {
public:
  static auto buildDerivedTypeHierarchy(CPlusPlus::Symbol *symbol, const CPlusPlus::Snapshot &snapshot) -> TypeHierarchy;
  static auto buildDerivedTypeHierarchy(QFutureInterfaceBase &futureInterface, CPlusPlus::Symbol *symbol, const CPlusPlus::Snapshot &snapshot) -> TypeHierarchy;
  static auto followTypedef(const CPlusPlus::LookupContext &context, const CPlusPlus::Name *symbolName, CPlusPlus::Scope *enclosingScope, std::set<const CPlusPlus::Symbol*> typedefs = {}) -> CPlusPlus::LookupItem;

private:
  TypeHierarchyBuilder() = default;
  auto buildDerived(QFutureInterfaceBase &futureInterface, TypeHierarchy *typeHierarchy, const CPlusPlus::Snapshot &snapshot, QHash<QString, QHash<QString, QString>> &cache, int depth = 0) -> void;

  QSet<CPlusPlus::Symbol*> _visited;
  QHash<Utils::FilePath, QSet<QString>> _candidates;
  CPlusPlus::Overview _overview;
};

} // CppEditor::Internal
