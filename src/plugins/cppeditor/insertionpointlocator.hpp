// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cpprefactoringchanges.hpp"

namespace CPlusPlus {
class Namespace;
class NamespaceAST;
class Symbol;
} // namespace CPlusPlus

namespace CppEditor {

class CPPEDITOR_EXPORT InsertionLocation {
public:
  InsertionLocation();
  InsertionLocation(const QString &fileName, const QString &prefix, const QString &suffix, int line, int column);
  auto fileName() const -> QString { return m_fileName; }
  /// \returns The prefix to insert before any other text.
  auto prefix() const -> QString { return m_prefix; }
  /// \returns The suffix to insert after the other inserted text.
  auto suffix() const -> QString { return m_suffix; }
  /// \returns The line where to insert. The line number is 1-based.
  auto line() const -> int { return m_line; }
  /// \returns The column where to insert. The column number is 1-based.
  auto column() const -> int { return m_column; }
  auto isValid() const -> bool { return !m_fileName.isEmpty() && m_line > 0 && m_column > 0; }

private:
  QString m_fileName;
  QString m_prefix;
  QString m_suffix;
  int m_line = 0;
  int m_column = 0;
};

class CPPEDITOR_EXPORT InsertionPointLocator {
public:
  enum AccessSpec {
    Invalid = -1,
    Signals = 0,
    Public = 1,
    Protected = 2,
    Private = 3,
    SlotBit = 1 << 2,
    PublicSlot = Public | SlotBit,
    ProtectedSlot = Protected | SlotBit,
    PrivateSlot = Private | SlotBit
  };

  static auto accessSpecToString(InsertionPointLocator::AccessSpec xsSpec) -> QString;

  enum Position {
    AccessSpecBegin,
    AccessSpecEnd,
  };

  enum class ForceAccessSpec {
    Yes,
    No
  };
  
  explicit InsertionPointLocator(const CppRefactoringChanges &refactoringChanges);
  auto methodDeclarationInClass(const QString &fileName, const CPlusPlus::Class *clazz, AccessSpec xsSpec, ForceAccessSpec forceAccessSpec = ForceAccessSpec::No) const -> InsertionLocation;
  auto methodDeclarationInClass(const CPlusPlus::TranslationUnit *tu, const CPlusPlus::ClassSpecifierAST *clazz, AccessSpec xsSpec, Position positionInAccessSpec = AccessSpecEnd, ForceAccessSpec forceAccessSpec = ForceAccessSpec::No) const -> InsertionLocation;
  auto constructorDeclarationInClass(const CPlusPlus::TranslationUnit *tu, const CPlusPlus::ClassSpecifierAST *clazz, AccessSpec xsSpec, int constructorArgumentCount) const -> InsertionLocation;
  auto methodDefinition(CPlusPlus::Symbol *declaration, bool useSymbolFinder = true, const QString &destinationFile = QString()) const -> const QList<InsertionLocation>;

private:
  CppRefactoringChanges m_refactoringChanges;
};

// TODO: We should use the "CreateMissing" approach everywhere.
enum class NamespaceHandling { CreateMissing, Ignore };

CPPEDITOR_EXPORT auto insertLocationForMethodDefinition(CPlusPlus::Symbol *symbol, const bool useSymbolFinder, NamespaceHandling namespaceHandling, const CppRefactoringChanges &refactoring, const QString &fileName, QStringList *insertedNamespaces = nullptr) -> InsertionLocation;

} // namespace CppEditor
