// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <cplusplus/CppDocument.h>

#include <texteditor/refactoringchanges.hpp>

namespace CppEditor {

class CppRefactoringChanges;
class CppRefactoringFile;
class CppRefactoringChangesData;
using CppRefactoringFilePtr = QSharedPointer<CppRefactoringFile>;
using CppRefactoringFileConstPtr = QSharedPointer<const CppRefactoringFile>;

class CPPEDITOR_EXPORT CppRefactoringFile : public TextEditor::RefactoringFile {
public:
  using TextEditor::RefactoringFile::textOf;

  auto cppDocument() const -> CPlusPlus::Document::Ptr;
  auto setCppDocument(CPlusPlus::Document::Ptr document) -> void;
  auto scopeAt(unsigned index) const -> CPlusPlus::Scope*;
  auto isCursorOn(unsigned tokenIndex) const -> bool;
  auto isCursorOn(const CPlusPlus::AST *ast) const -> bool;
  auto range(int start, int end) const -> Range;
  auto range(unsigned tokenIndex) const -> Range;
  auto range(const CPlusPlus::AST *ast) const -> Range;
  auto tokenAt(unsigned index) const -> const CPlusPlus::Token&;
  auto startOf(unsigned index) const -> int;
  auto startOf(const CPlusPlus::AST *ast) const -> int;
  auto endOf(unsigned index) const -> int;
  auto endOf(const CPlusPlus::AST *ast) const -> int;
  auto startAndEndOf(unsigned index, int *start, int *end) const -> void;
  auto textOf(const CPlusPlus::AST *ast) const -> QString;

protected:
  CppRefactoringFile(const Utils::FilePath &filePath, const QSharedPointer<TextEditor::RefactoringChangesData> &data);
  CppRefactoringFile(QTextDocument *document, const Utils::FilePath &filePath);
  explicit CppRefactoringFile(TextEditor::TextEditorWidget *editor);

  auto data() const -> CppRefactoringChangesData*;
  auto fileChanged() -> void override;

  mutable CPlusPlus::Document::Ptr m_cppDocument;

  friend class CppRefactoringChanges; // for access to constructor
};

class CPPEDITOR_EXPORT CppRefactoringChanges : public TextEditor::RefactoringChanges {
public:
  explicit CppRefactoringChanges(const CPlusPlus::Snapshot &snapshot);

  static auto file(TextEditor::TextEditorWidget *editor, const CPlusPlus::Document::Ptr &document) -> CppRefactoringFilePtr;
  auto file(const Utils::FilePath &filePath) const -> CppRefactoringFilePtr;
  // safe to use from non-gui threads
  auto fileNoEditor(const Utils::FilePath &filePath) const -> CppRefactoringFileConstPtr;
  auto snapshot() const -> const CPlusPlus::Snapshot&;

private:
  auto data() const -> CppRefactoringChangesData*;
};

} // namespace CppEditor
