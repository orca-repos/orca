// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include "clangdiagnosticconfig.hpp"
#include "compileroptionsbuilder.hpp"
#include "projectpart.hpp"

#include <texteditor/texteditor.hpp>

#include <cplusplus/ASTVisitor.h>
#include <cplusplus/CppDocument.h>
#include <cplusplus/Token.h>

QT_BEGIN_NAMESPACE
class QChar;
class QFileInfo;
class QTextCursor;
QT_END_NAMESPACE

namespace CPlusPlus {
class Macro;
class Symbol;
class LookupContext;
} // namespace CPlusPlus

namespace TextEditor {
class AssistInterface;
}

namespace CppEditor {
class CppRefactoringFile;
class ProjectInfo;

CPPEDITOR_EXPORT auto moveCursorToEndOfIdentifier(QTextCursor *tc) -> void;
CPPEDITOR_EXPORT auto moveCursorToStartOfIdentifier(QTextCursor *tc) -> void;
CPPEDITOR_EXPORT auto isQtKeyword(QStringView text) -> bool;
CPPEDITOR_EXPORT auto isValidAsciiIdentifierChar(const QChar &ch) -> bool;
CPPEDITOR_EXPORT auto isValidFirstIdentifierChar(const QChar &ch) -> bool;
CPPEDITOR_EXPORT auto isValidIdentifierChar(const QChar &ch) -> bool;
CPPEDITOR_EXPORT auto isValidIdentifier(const QString &s) -> bool;
CPPEDITOR_EXPORT auto identifierWordsUnderCursor(const QTextCursor &tc) -> QStringList;
CPPEDITOR_EXPORT auto identifierUnderCursor(QTextCursor *cursor) -> QString;
CPPEDITOR_EXPORT auto isOwnershipRAIIType(CPlusPlus::Symbol *symbol, const CPlusPlus::LookupContext &context) -> bool;
CPPEDITOR_EXPORT auto findCanonicalMacro(const QTextCursor &cursor, CPlusPlus::Document::Ptr document) -> const CPlusPlus::Macro*;
CPPEDITOR_EXPORT auto isInCommentOrString(const TextEditor::AssistInterface *interface, CPlusPlus::LanguageFeatures features) -> bool;

enum class CacheUsage {
  ReadWrite,
  ReadOnly
};

CPPEDITOR_EXPORT auto correspondingHeaderOrSource(const QString &fileName, bool *wasHeader = nullptr, CacheUsage cacheUsage = CacheUsage::ReadWrite) -> QString;
CPPEDITOR_EXPORT auto switchHeaderSource() -> void;

class CppCodeModelSettings;

CPPEDITOR_EXPORT auto codeModelSettings() -> CppCodeModelSettings*;
CPPEDITOR_EXPORT auto getPchUsage() -> UsePrecompiledHeaders;

auto indexerFileSizeLimitInMb() -> int;
auto fileSizeExceedsLimit(const QFileInfo &fileInfo, int sizeLimitInMb) -> bool;

CPPEDITOR_EXPORT auto projectForProjectInfo(const ProjectInfo &info) -> ProjectExplorer::Project*;
CPPEDITOR_EXPORT auto projectForProjectPart(const ProjectPart &part) -> ProjectExplorer::Project*;

class ClangDiagnosticConfigsModel;

CPPEDITOR_EXPORT auto diagnosticConfigsModel() -> ClangDiagnosticConfigsModel;
CPPEDITOR_EXPORT auto diagnosticConfigsModel(const ClangDiagnosticConfigs &customConfigs) -> ClangDiagnosticConfigsModel;

class CPPEDITOR_EXPORT SymbolInfo {
public:
  int startLine = 0;
  int startColumn = 0;
  int endLine = 0;
  int endColumn = 0;
  QString fileName;
  bool isResultOnlyForFallBack = false;
};

class ProjectPartInfo {
public:
  enum Hint {
    NoHint = 0,
    IsFallbackMatch = 1 << 0,
    IsAmbiguousMatch = 1 << 1,
    IsPreferredMatch = 1 << 2,
    IsFromProjectMatch = 1 << 3,
    IsFromDependenciesMatch = 1 << 4,
  };

  Q_DECLARE_FLAGS(Hints, Hint)

  ProjectPartInfo() = default;
  ProjectPartInfo(const ProjectPart::ConstPtr &projectPart, const QList<ProjectPart::ConstPtr> &projectParts, Hints hints) : projectPart(projectPart), projectParts(projectParts), hints(hints) { }

  ProjectPart::ConstPtr projectPart;
  QList<ProjectPart::ConstPtr> projectParts; // The one above as first plus alternatives.
  Hints hints = NoHint;
};

CPPEDITOR_EXPORT auto getNamespaceNames(const CPlusPlus::Namespace *firstNamespace) -> QStringList;
CPPEDITOR_EXPORT auto getNamespaceNames(const CPlusPlus::Symbol *symbol) -> QStringList;

class CPPEDITOR_EXPORT NSVisitor : public CPlusPlus::ASTVisitor {
public:
  NSVisitor(const CppRefactoringFile *file, const QStringList &namespaces, int symbolPos);

  auto remainingNamespaces() const -> const QStringList { return m_remainingNamespaces; }
  auto firstNamespace() const -> const CPlusPlus::NamespaceAST* { return m_firstNamespace; }
  auto firstToken() const -> const CPlusPlus::AST* { return m_firstToken; }
  auto enclosingNamespace() const -> const CPlusPlus::NamespaceAST* { return m_enclosingNamespace; }

private:
  auto preVisit(CPlusPlus::AST *ast) -> bool override;
  auto visit(CPlusPlus::NamespaceAST *ns) -> bool override;
  auto postVisit(CPlusPlus::AST *ast) -> void override;

  const CppRefactoringFile *const m_file;
  const CPlusPlus::NamespaceAST *m_enclosingNamespace = nullptr;
  const CPlusPlus::NamespaceAST *m_firstNamespace = nullptr;
  const CPlusPlus::AST *m_firstToken = nullptr;
  QStringList m_remainingNamespaces;
  const int m_symbolPos;
  bool m_done = false;
};

class CPPEDITOR_EXPORT NSCheckerVisitor : public CPlusPlus::ASTVisitor {
public:
  NSCheckerVisitor(const CppRefactoringFile *file, const QStringList &namespaces, int symbolPos);

  /**
   * @brief returns the names of the namespaces that are additionally needed at the symbolPos
   * @return A list of namespace names, the outermost namespace at index 0 and the innermost
   * at the last index
   */
  auto remainingNamespaces() const -> const QStringList { return m_remainingNamespaces; }

private:
  auto preVisit(CPlusPlus::AST *ast) -> bool override;
  auto postVisit(CPlusPlus::AST *ast) -> void override;
  auto visit(CPlusPlus::NamespaceAST *ns) -> bool override;
  auto visit(CPlusPlus::UsingDirectiveAST *usingNS) -> bool override;
  auto endVisit(CPlusPlus::NamespaceAST *ns) -> void override;
  auto endVisit(CPlusPlus::TranslationUnitAST *) -> void override;
  auto getName(CPlusPlus::NamespaceAST *ns) -> QString;
  auto currentNamespace() -> CPlusPlus::NamespaceAST*;

  const CppRefactoringFile *const m_file;
  QStringList m_remainingNamespaces;
  const int m_symbolPos;
  std::vector<CPlusPlus::NamespaceAST*> m_enteredNamespaces;

  // track 'using namespace ...' statements
  std::unordered_map<CPlusPlus::NamespaceAST*, QStringList> m_usingsPerNamespace;

  bool m_done = false;
};

namespace Internal {
auto decorateCppEditor(TextEditor::TextEditorWidget *editor) -> void;
} // namespace Internal

} // CppEditor
