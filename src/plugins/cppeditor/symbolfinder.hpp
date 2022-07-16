// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "cppfileiterationorder.hpp"

#include <QHash>
#include <QSet>
#include <QStringList>

#include <set>

namespace CPlusPlus {
class Class;
class Declaration;
class Function;
class LookupContext;
class Snapshot;
class Symbol;
}

namespace CppEditor {

class CPPEDITOR_EXPORT SymbolFinder {
public:
  SymbolFinder();

  auto findMatchingDefinition(CPlusPlus::Symbol *symbol, const CPlusPlus::Snapshot &snapshot, bool strict = false) -> CPlusPlus::Function*;
  auto findMatchingVarDefinition(CPlusPlus::Symbol *declaration, const CPlusPlus::Snapshot &snapshot) -> CPlusPlus::Symbol*;
  auto findMatchingClassDeclaration(CPlusPlus::Symbol *declaration, const CPlusPlus::Snapshot &snapshot) -> CPlusPlus::Class*;
  auto findMatchingDeclaration(const CPlusPlus::LookupContext &context, CPlusPlus::Function *functionType, QList<CPlusPlus::Declaration*> *typeMatch, QList<CPlusPlus::Declaration*> *argumentCountMatch, QList<CPlusPlus::Declaration*> *nameMatch) -> void;
  auto findMatchingDeclaration(const CPlusPlus::LookupContext &context, CPlusPlus::Function *functionType) -> QList<CPlusPlus::Declaration*>;
  auto clearCache() -> void;

private:
  auto fileIterationOrder(const QString &referenceFile, const CPlusPlus::Snapshot &snapshot) -> QStringList;
  auto checkCacheConsistency(const QString &referenceFile, const CPlusPlus::Snapshot &snapshot) -> void;
  auto clearCache(const QString &referenceFile, const QString &comparingFile) -> void;
  auto insertCache(const QString &referenceFile, const QString &comparingFile) -> void;
  auto trackCacheUse(const QString &referenceFile) -> void;

  QHash<QString, FileIterationOrder> m_filePriorityCache;
  QHash<QString, QSet<QString>> m_fileMetaCache;
  QStringList m_recent;
};

} // namespace CppEditor
