// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "baseeditordocumentprocessor.hpp"

#include "cppcodemodelsettings.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolsreuse.hpp"
#include "editordocumenthandle.hpp"

#include <projectexplorer/session.hpp>
#include <texteditor/quickfix.hpp>

namespace CppEditor {

/*!
    \class CppEditor::BaseEditorDocumentProcessor

    \brief The BaseEditorDocumentProcessor class controls and executes all
           document relevant actions (reparsing, semantic highlighting, additional
           semantic calculations) after a text document has changed.
*/

BaseEditorDocumentProcessor::BaseEditorDocumentProcessor(QTextDocument *textDocument, const QString &filePath) : m_filePath(filePath), m_textDocument(textDocument) {}

BaseEditorDocumentProcessor::~BaseEditorDocumentProcessor() = default;

auto BaseEditorDocumentProcessor::run(bool projectsUpdated) -> void
{
  const auto languagePreference = codeModelSettings()->interpretAmbigiousHeadersAsCHeaders() ? Utils::Language::C : Utils::Language::Cxx;

  runImpl({CppModelManager::instance()->workingCopy(), ProjectExplorer::SessionManager::startupProject(), languagePreference, projectsUpdated});
}

auto BaseEditorDocumentProcessor::extraRefactoringOperations(const TextEditor::AssistInterface &) -> TextEditor::QuickFixOperations
{
  return TextEditor::QuickFixOperations();
}

auto BaseEditorDocumentProcessor::editorDocumentTimerRestarted() -> void {}

auto BaseEditorDocumentProcessor::invalidateDiagnostics() -> void {}

auto BaseEditorDocumentProcessor::setParserConfig(const BaseEditorDocumentParser::Configuration &config) -> void
{
  parser()->setConfiguration(config);
}

auto BaseEditorDocumentProcessor::toolTipInfo(const QByteArray &/*codecName*/, int /*line*/, int /*column*/) -> QFuture<ToolTipInfo>
{
  return QFuture<ToolTipInfo>();
}

auto BaseEditorDocumentProcessor::runParser(QFutureInterface<void> &future, BaseEditorDocumentParser::Ptr parser, BaseEditorDocumentParser::UpdateParams updateParams) -> void
{
  future.setProgressRange(0, 1);
  if (future.isCanceled()) {
    future.setProgressValue(1);
    return;
  }

  parser->update(future, updateParams);
  CppModelManager::instance()->finishedRefreshingSourceFiles({parser->filePath()});

  future.setProgressValue(1);
}

} // namespace CppEditor
