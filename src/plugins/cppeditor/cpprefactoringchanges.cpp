// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cpprefactoringchanges.hpp"

#include "cppqtstyleindenter.hpp"
#include "cppcodeformatter.hpp"
#include "cppeditorconstants.hpp"
#include "cppmodelmanager.hpp"
#include "cppworkingcopy.hpp"

#include <projectexplorer/editorconfiguration.hpp>

#include <utils/qtcassert.hpp>

#include <texteditor/icodestylepreferencesfactory.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <QTextDocument>

using namespace CPlusPlus;

namespace CppEditor {

class CppRefactoringChangesData : public TextEditor::RefactoringChangesData {
  static auto createIndenter(const Utils::FilePath &filePath, QTextDocument *textDocument) -> std::unique_ptr<TextEditor::Indenter>
  {
    auto factory = TextEditor::TextEditorSettings::codeStyleFactory(Constants::CPP_SETTINGS_ID);
    std::unique_ptr<TextEditor::Indenter> indenter(factory->createIndenter(textDocument));
    indenter->setFileName(filePath);
    return indenter;
  }

public:
  explicit CppRefactoringChangesData(const Snapshot &snapshot) : m_snapshot(snapshot), m_modelManager(CppModelManager::instance()), m_workingCopy(m_modelManager->workingCopy()) {}

  auto indentSelection(const QTextCursor &selection, const Utils::FilePath &filePath, const TextEditor::TextDocument *textDocument) const -> void override
  {
    if (textDocument) {
      // use the indenter from the textDocument if there is one, can be ClangFormat
      textDocument->indenter()->indent(selection, QChar::Null, textDocument->tabSettings());
    } else {
      const auto &tabSettings = ProjectExplorer::actualTabSettings(filePath.toString(), textDocument);
      auto indenter = createIndenter(filePath, selection.document());
      indenter->indent(selection, QChar::Null, tabSettings);
    }
  }

  auto reindentSelection(const QTextCursor &selection, const Utils::FilePath &filePath, const TextEditor::TextDocument *textDocument) const -> void override
  {
    if (textDocument) {
      // use the indenter from the textDocument if there is one, can be ClangFormat
      textDocument->indenter()->reindent(selection, textDocument->tabSettings());
    } else {
      const auto &tabSettings = ProjectExplorer::actualTabSettings(filePath.toString(), textDocument);
      auto indenter = createIndenter(filePath, selection.document());
      indenter->reindent(selection, tabSettings);
    }
  }

  auto fileChanged(const Utils::FilePath &filePath) -> void override
  {
    m_modelManager->updateSourceFiles({filePath.toString()});
  }

  Snapshot m_snapshot;
  CppModelManager *m_modelManager;
  WorkingCopy m_workingCopy;

};

CppRefactoringChanges::CppRefactoringChanges(const Snapshot &snapshot) : RefactoringChanges(new CppRefactoringChangesData(snapshot)) {}

auto CppRefactoringChanges::data() const -> CppRefactoringChangesData*
{
  return static_cast<CppRefactoringChangesData*>(m_data.data());
}

auto CppRefactoringChanges::file(TextEditor::TextEditorWidget *editor, const Document::Ptr &document) -> CppRefactoringFilePtr
{
  CppRefactoringFilePtr result(new CppRefactoringFile(editor));
  result->setCppDocument(document);
  return result;
}

auto CppRefactoringChanges::file(const Utils::FilePath &filePath) const -> CppRefactoringFilePtr
{
  CppRefactoringFilePtr result(new CppRefactoringFile(filePath, m_data));
  return result;
}

auto CppRefactoringChanges::fileNoEditor(const Utils::FilePath &filePath) const -> CppRefactoringFileConstPtr
{
  QTextDocument *document = nullptr;
  const auto fileName = filePath.toString();
  if (data()->m_workingCopy.contains(fileName))
    document = new QTextDocument(QString::fromUtf8(data()->m_workingCopy.source(fileName)));
  CppRefactoringFilePtr result(new CppRefactoringFile(document, filePath));
  result->m_data = m_data;

  return result;
}

auto CppRefactoringChanges::snapshot() const -> const Snapshot&
{
  return data()->m_snapshot;
}

CppRefactoringFile::CppRefactoringFile(const Utils::FilePath &filePath, const QSharedPointer<TextEditor::RefactoringChangesData> &data) : RefactoringFile(filePath, data)
{
  const Snapshot &snapshot = this->data()->m_snapshot;
  m_cppDocument = snapshot.document(filePath.toString());
}

CppRefactoringFile::CppRefactoringFile(QTextDocument *document, const Utils::FilePath &filePath) : RefactoringFile(document, filePath) { }

CppRefactoringFile::CppRefactoringFile(TextEditor::TextEditorWidget *editor) : RefactoringFile(editor) { }

auto CppRefactoringFile::cppDocument() const -> Document::Ptr
{
  if (!m_cppDocument || !m_cppDocument->translationUnit() || !m_cppDocument->translationUnit()->ast()) {
    const auto source = document()->toPlainText().toUtf8();
    const Snapshot &snapshot = data()->m_snapshot;

    m_cppDocument = snapshot.preprocessedDocument(source, filePath());
    m_cppDocument->check();
  }

  return m_cppDocument;
}

auto CppRefactoringFile::setCppDocument(Document::Ptr document) -> void
{
  m_cppDocument = document;
}

auto CppRefactoringFile::scopeAt(unsigned index) const -> Scope*
{
  int line, column;
  cppDocument()->translationUnit()->getTokenStartPosition(index, &line, &column);
  return cppDocument()->scopeAt(line, column);
}

auto CppRefactoringFile::isCursorOn(unsigned tokenIndex) const -> bool
{
  auto tc = cursor();
  auto cursorBegin = tc.selectionStart();

  int start = startOf(tokenIndex);
  int end = endOf(tokenIndex);

  return cursorBegin >= start && cursorBegin <= end;
}

auto CppRefactoringFile::isCursorOn(const AST *ast) const -> bool
{
  auto tc = cursor();
  auto cursorBegin = tc.selectionStart();

  int start = startOf(ast);
  int end = endOf(ast);

  return cursorBegin >= start && cursorBegin <= end;
}

auto CppRefactoringFile::range(unsigned tokenIndex) const -> Utils::ChangeSet::Range
{
  const Token &token = tokenAt(tokenIndex);
  int line, column;
  cppDocument()->translationUnit()->getPosition(token.utf16charsBegin(), &line, &column);
  const auto start = document()->findBlockByNumber(line - 1).position() + column - 1;
  return {start, start + token.utf16chars()};
}

auto CppRefactoringFile::range(const AST *ast) const -> Utils::ChangeSet::Range
{
  return {startOf(ast), endOf(ast)};
}

auto CppRefactoringFile::startOf(unsigned index) const -> int
{
  int line, column;
  cppDocument()->translationUnit()->getPosition(tokenAt(index).utf16charsBegin(), &line, &column);
  return document()->findBlockByNumber(line - 1).position() + column - 1;
}

auto CppRefactoringFile::startOf(const AST *ast) const -> int
{
  int firstToken = ast->firstToken();
  const int lastToken = ast->lastToken();
  while (tokenAt(firstToken).generated() && firstToken < lastToken)
    ++firstToken;
  return startOf(firstToken);
}

auto CppRefactoringFile::endOf(unsigned index) const -> int
{
  int line, column;
  cppDocument()->translationUnit()->getPosition(tokenAt(index).utf16charsEnd(), &line, &column);
  return document()->findBlockByNumber(line - 1).position() + column - 1;
}

auto CppRefactoringFile::endOf(const AST *ast) const -> int
{
  int lastToken = ast->lastToken() - 1;
  QTC_ASSERT(lastToken >= 0, return -1);
  const int firstToken = ast->firstToken();
  while (tokenAt(lastToken).generated() && lastToken > firstToken)
    --lastToken;
  return endOf(lastToken);
}

auto CppRefactoringFile::startAndEndOf(unsigned index, int *start, int *end) const -> void
{
  int line, column;
  Token token(tokenAt(index));
  cppDocument()->translationUnit()->getPosition(token.utf16charsBegin(), &line, &column);
  *start = document()->findBlockByNumber(line - 1).position() + column - 1;
  *end = *start + token.utf16chars();
}

auto CppRefactoringFile::textOf(const AST *ast) const -> QString
{
  return textOf(startOf(ast), endOf(ast));
}

auto CppRefactoringFile::tokenAt(unsigned index) const -> const Token&
{
  return cppDocument()->translationUnit()->tokenAt(index);
}

auto CppRefactoringFile::data() const -> CppRefactoringChangesData*
{
  return static_cast<CppRefactoringChangesData*>(m_data.data());
}

auto CppRefactoringFile::fileChanged() -> void
{
  m_cppDocument.clear();
  RefactoringFile::fileChanged();
}

} // namespace CppEditor
