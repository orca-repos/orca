// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "baseeditordocumentprocessor.hpp"
#include "builtineditordocumentparser.hpp"
#include "cppeditor_global.hpp"
#include "cppsemanticinfoupdater.hpp"
#include "semantichighlighter.hpp"

namespace CppEditor {

class CPPEDITOR_EXPORT BuiltinEditorDocumentProcessor : public BaseEditorDocumentProcessor {
  Q_OBJECT

public:
  BuiltinEditorDocumentProcessor(TextEditor::TextDocument *document, bool enableSemanticHighlighter = true);
  ~BuiltinEditorDocumentProcessor() override;

  // BaseEditorDocumentProcessor interface
  auto runImpl(const BaseEditorDocumentParser::UpdateParams &updateParams) -> void override;
  auto recalculateSemanticInfoDetached(bool force) -> void override;
  auto semanticRehighlight() -> void override;
  auto recalculateSemanticInfo() -> SemanticInfo override;
  auto parser() -> BaseEditorDocumentParser::Ptr override;
  auto snapshot() -> CPlusPlus::Snapshot override;
  auto isParserRunning() const -> bool override;
  auto cursorInfo(const CursorInfoParams &params) -> QFuture<CursorInfo> override;
  auto requestLocalReferences(const QTextCursor &) -> QFuture<CursorInfo> override;
  auto requestFollowSymbol(int, int) -> QFuture<SymbolInfo> override;

private:
  auto onParserFinished(CPlusPlus::Document::Ptr document, CPlusPlus::Snapshot snapshot) -> void;
  auto onSemanticInfoUpdated(const SemanticInfo semanticInfo) -> void;
  auto onCodeWarningsUpdated(CPlusPlus::Document::Ptr document, const QList<CPlusPlus::Document::DiagnosticMessage> &codeWarnings) -> void;
  auto createSemanticInfoSource(bool force) const -> SemanticInfo::Source;
  
  BuiltinEditorDocumentParser::Ptr m_parser;
  QFuture<void> m_parserFuture;
  CPlusPlus::Snapshot m_documentSnapshot;
  QList<QTextEdit::ExtraSelection> m_codeWarnings;
  bool m_codeWarningsUpdated;
  SemanticInfoUpdater m_semanticInfoUpdater;
  QScopedPointer<SemanticHighlighter> m_semanticHighlighter;
};

} // namespace CppEditor
