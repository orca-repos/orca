// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "baseeditordocumentparser.hpp"
#include "cppcursorinfo.hpp"
#include "cppeditor_global.hpp"
#include "cppsemanticinfo.hpp"
#include "cpptoolsreuse.hpp"

#include <core/core-help-item.hpp>
#include <texteditor/codeassist/assistinterface.hpp>
#include <texteditor/quickfix.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/textdocument.hpp>

#include <cplusplus/CppDocument.h>

#include <QTextEdit>

#include <QVariant>

#include <functional>

namespace TextEditor {
class TextDocument;
}

namespace CppEditor {

// For clang code model only, move?
struct CPPEDITOR_EXPORT ToolTipInfo {
  QString text;
  QString briefComment;
  QStringList qDocIdCandidates;
  QString qDocMark;
  Orca::Plugin::Core::HelpItem::Category qDocCategory;
  QVariant value;
  QString sizeInBytes;
};

class CPPEDITOR_EXPORT BaseEditorDocumentProcessor : public QObject {
  Q_OBJECT

public:
  BaseEditorDocumentProcessor(QTextDocument *textDocument, const QString &filePath);
  ~BaseEditorDocumentProcessor() override;

  auto run(bool projectsUpdated = false) -> void;
  virtual auto semanticRehighlight() -> void = 0;
  virtual auto recalculateSemanticInfoDetached(bool force) -> void = 0;
  virtual auto recalculateSemanticInfo() -> SemanticInfo = 0;
  virtual auto snapshot() -> CPlusPlus::Snapshot = 0;
  virtual auto parser() -> BaseEditorDocumentParser::Ptr = 0;
  virtual auto isParserRunning() const -> bool = 0;
  virtual auto extraRefactoringOperations(const TextEditor::AssistInterface &assistInterface) -> TextEditor::QuickFixOperations;
  virtual auto invalidateDiagnostics() -> void;
  virtual auto editorDocumentTimerRestarted() -> void;
  virtual auto setParserConfig(const BaseEditorDocumentParser::Configuration &config) -> void;
  virtual auto cursorInfo(const CursorInfoParams &params) -> QFuture<CursorInfo> = 0;
  virtual auto requestLocalReferences(const QTextCursor &cursor) -> QFuture<CursorInfo> = 0;
  virtual auto requestFollowSymbol(int line, int column) -> QFuture<SymbolInfo> = 0;
  virtual auto toolTipInfo(const QByteArray &codecName, int line, int column) -> QFuture<ToolTipInfo>;

  auto filePath() const -> QString { return m_filePath; }
  
  using HeaderErrorDiagnosticWidgetCreator = std::function<QWidget*()>;

signals:
  // Signal interface to implement
  auto projectPartInfoUpdated(const ProjectPartInfo &projectPartInfo) -> void;
  auto codeWarningsUpdated(unsigned revision, const QList<QTextEdit::ExtraSelection> &selections, const HeaderErrorDiagnosticWidgetCreator &creator, const TextEditor::RefactorMarkers &refactorMarkers) -> void;
  auto ifdefedOutBlocksUpdated(unsigned revision, const QList<TextEditor::BlockRange> &ifdefedOutBlocks) -> void;
  auto cppDocumentUpdated(const CPlusPlus::Document::Ptr document) -> void; // TODO: Remove me
  auto semanticInfoUpdated(const SemanticInfo semanticInfo) -> void;        // TODO: Remove me

protected:
  static auto runParser(QFutureInterface<void> &future, BaseEditorDocumentParser::Ptr parser, BaseEditorDocumentParser::UpdateParams updateParams) -> void;

  // Convenience
  auto revision() const -> unsigned { return static_cast<unsigned>(m_textDocument->revision()); }
  auto textDocument() const -> QTextDocument* { return m_textDocument; }

private:
  virtual auto runImpl(const BaseEditorDocumentParser::UpdateParams &updateParams) -> void = 0;
  
  QString m_filePath;
  QTextDocument *m_textDocument;
};

} // namespace CppEditor
