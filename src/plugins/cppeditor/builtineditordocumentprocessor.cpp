// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "builtineditordocumentprocessor.hpp"

#include "builtincursorinfo.hpp"
#include "cppchecksymbols.hpp"
#include "cppcodemodelsettings.hpp"
#include "cppeditorplugin.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolsreuse.hpp"
#include "cppworkingcopy.hpp"

#include <texteditor/fontsettings.hpp>
#include <texteditor/refactoroverlay.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <cplusplus/CppDocument.h>
#include <cplusplus/SimpleLexer.h>

#include <utils/textutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>

#include <QLoggingCategory>
#include <QTextBlock>

static Q_LOGGING_CATEGORY(log, "qtc.cppeditor.builtineditordocumentprocessor", QtWarningMsg)

namespace CppEditor {
namespace {

auto toTextEditorSelections(const QList<CPlusPlus::Document::DiagnosticMessage> &diagnostics, QTextDocument *textDocument) -> QList<QTextEdit::ExtraSelection>
{
  const auto &fontSettings = TextEditor::TextEditorSettings::fontSettings();

  auto warningFormat = fontSettings.toTextCharFormat(TextEditor::C_WARNING);
  auto errorFormat = fontSettings.toTextCharFormat(TextEditor::C_ERROR);

  QList<QTextEdit::ExtraSelection> result;
  foreach(const CPlusPlus::Document::DiagnosticMessage &m, diagnostics) {
    QTextEdit::ExtraSelection sel;
    if (m.isWarning())
      sel.format = warningFormat;
    else
      sel.format = errorFormat;

    QTextCursor c(textDocument->findBlockByNumber(m.line() - 1));
    const QString text = c.block().text();
    const int startPos = m.column() > 0 ? m.column() - 1 : 0;
    if (m.length() > 0 && startPos + m.length() <= text.size()) {
      c.setPosition(c.position() + startPos);
      c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, m.length());
    } else {
      for (auto i = 0; i < text.size(); ++i) {
        if (!text.at(i).isSpace()) {
          c.setPosition(c.position() + i);
          break;
        }
      }
      c.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
    }
    sel.cursor = c;
    sel.format.setToolTip(m.text());
    result.append(sel);
  }

  return result;
}

auto createHighlighter(const CPlusPlus::Document::Ptr &doc, const CPlusPlus::Snapshot &snapshot, QTextDocument *textDocument) -> CheckSymbols*
{
  QTC_ASSERT(doc, return nullptr);
  QTC_ASSERT(doc->translationUnit(), return nullptr);
  QTC_ASSERT(doc->translationUnit()->ast(), return nullptr);
  QTC_ASSERT(textDocument, return nullptr);

  using namespace CPlusPlus;
  using Result = TextEditor::HighlightingResult;
  QList<Result> macroUses;

  using Utils::Text::convertPosition;

  // Get macro definitions
  foreach(const CPlusPlus::Macro& macro, doc->definedMacros()) {
    int line, column;
    convertPosition(textDocument, macro.utf16CharOffset(), &line, &column);

    Result use(line, column, macro.nameToQString().size(), SemanticHighlighter::MacroUse);
    macroUses.append(use);
  }

  const LanguageFeatures features = doc->languageFeatures();

  // Get macro uses
  foreach(const Document::MacroUse &macro, doc->macroUses()) {
    const QString name = macro.macro().nameToQString();

    //Filter out QtKeywords
    if (features.qtKeywordsEnabled && isQtKeyword(name))
      continue;

    SimpleLexer tokenize;
    tokenize.setLanguageFeatures(features);

    // Filter out C++ keywords
    const Tokens tokens = tokenize(name);
    if (!tokens.isEmpty() && (tokens.at(0).isKeyword() || tokens.at(0).isObjCAtKeyword()))
      continue;

    int line, column;
    convertPosition(textDocument, macro.utf16charsBegin(), &line, &column);

    Result use(line, column, name.size(), SemanticHighlighter::MacroUse);
    macroUses.append(use);
  }

  LookupContext context(doc, snapshot);
  return CheckSymbols::create(doc, context, macroUses);
}

auto toTextEditorBlocks(const QList<CPlusPlus::Document::Block> &skippedBlocks) -> QList<TextEditor::BlockRange>
{
  QList<TextEditor::BlockRange> result;
  result.reserve(skippedBlocks.size());
  foreach(const CPlusPlus::Document::Block &block, skippedBlocks)
    result.append(TextEditor::BlockRange(block.utf16charsBegin(), block.utf16charsEnd()));
  return result;
}

} // anonymous namespace

BuiltinEditorDocumentProcessor::BuiltinEditorDocumentProcessor(TextEditor::TextDocument *document, bool enableSemanticHighlighter) : BaseEditorDocumentProcessor(document->document(), document->filePath().toString()), m_parser(new BuiltinEditorDocumentParser(document->filePath().toString(), indexerFileSizeLimitInMb())), m_codeWarningsUpdated(false), m_semanticHighlighter(enableSemanticHighlighter ? new SemanticHighlighter(document) : nullptr)
{
  using namespace Internal;

  const CppCodeModelSettings *cms = CppEditorPlugin::instance()->codeModelSettings();

  auto config = m_parser->configuration();
  config.usePrecompiledHeaders = cms->pchUsage() != CppCodeModelSettings::PchUse_None;
  m_parser->setConfiguration(config);

  if (m_semanticHighlighter) {
    m_semanticHighlighter->setHighlightingRunner([this]() -> QFuture<TextEditor::HighlightingResult> {
      const auto semanticInfo = m_semanticInfoUpdater.semanticInfo();
      CheckSymbols *checkSymbols = createHighlighter(semanticInfo.doc, semanticInfo.snapshot, textDocument());
      QTC_ASSERT(checkSymbols, return QFuture<TextEditor::HighlightingResult>());
      connect(checkSymbols, &CheckSymbols::codeWarningsUpdated, this, &BuiltinEditorDocumentProcessor::onCodeWarningsUpdated);
      return checkSymbols->start();
    });
  }

  connect(m_parser.data(), &BuiltinEditorDocumentParser::projectPartInfoUpdated, this, &BaseEditorDocumentProcessor::projectPartInfoUpdated);
  connect(m_parser.data(), &BuiltinEditorDocumentParser::finished, this, &BuiltinEditorDocumentProcessor::onParserFinished);
  connect(&m_semanticInfoUpdater, &SemanticInfoUpdater::updated, this, &BuiltinEditorDocumentProcessor::onSemanticInfoUpdated);
}

BuiltinEditorDocumentProcessor::~BuiltinEditorDocumentProcessor()
{
  m_parserFuture.cancel();
}

auto BuiltinEditorDocumentProcessor::runImpl(const BaseEditorDocumentParser::UpdateParams &updateParams) -> void
{
  m_parserFuture = Utils::runAsync(CppModelManager::instance()->sharedThreadPool(), runParser, parser(), updateParams);
}

auto BuiltinEditorDocumentProcessor::parser() -> BaseEditorDocumentParser::Ptr
{
  return m_parser;
}

auto BuiltinEditorDocumentProcessor::snapshot() -> CPlusPlus::Snapshot
{
  return m_parser->snapshot();
}

auto BuiltinEditorDocumentProcessor::recalculateSemanticInfoDetached(bool force) -> void
{
  const auto source = createSemanticInfoSource(force);
  m_semanticInfoUpdater.updateDetached(source);
}

auto BuiltinEditorDocumentProcessor::semanticRehighlight() -> void
{
  if (m_semanticHighlighter && m_semanticInfoUpdater.semanticInfo().doc) {
    if (const CPlusPlus::Document::Ptr document = m_documentSnapshot.document(filePath())) {
      m_codeWarnings = toTextEditorSelections(document->diagnosticMessages(), textDocument());
      m_codeWarningsUpdated = false;
    }

    m_semanticHighlighter->updateFormatMapFromFontSettings();
    m_semanticHighlighter->run();
  }
}

auto BuiltinEditorDocumentProcessor::recalculateSemanticInfo() -> SemanticInfo
{
  const auto source = createSemanticInfoSource(false);
  return m_semanticInfoUpdater.update(source);
}

auto BuiltinEditorDocumentProcessor::isParserRunning() const -> bool
{
  return m_parserFuture.isRunning();
}

auto BuiltinEditorDocumentProcessor::cursorInfo(const CursorInfoParams &params) -> QFuture<CursorInfo>
{
  return BuiltinCursorInfo::run(params);
}

auto BuiltinEditorDocumentProcessor::requestLocalReferences(const QTextCursor &) -> QFuture<CursorInfo>
{
  QFutureInterface<CursorInfo> futureInterface;
  futureInterface.reportResult(CursorInfo());
  futureInterface.reportFinished();

  return futureInterface.future();
}

auto BuiltinEditorDocumentProcessor::requestFollowSymbol(int, int) -> QFuture<SymbolInfo>
{
  QFutureInterface<SymbolInfo> futureInterface;
  futureInterface.reportResult(SymbolInfo());
  futureInterface.reportFinished();

  return futureInterface.future();
}

auto BuiltinEditorDocumentProcessor::onParserFinished(CPlusPlus::Document::Ptr document, CPlusPlus::Snapshot snapshot) -> void
{
  if (document.isNull())
    return;

  if (document->fileName() != filePath())
    return; // some other document got updated

  if (document->editorRevision() != revision())
    return; // outdated content, wait for a new document to be parsed

  qCDebug(log) << "document parsed" << document->fileName() << document->editorRevision();

  // Emit ifdefed out blocks
  const auto ifdefoutBlocks = toTextEditorBlocks(document->skippedBlocks());
  emit ifdefedOutBlocksUpdated(revision(), ifdefoutBlocks);

  // Store parser warnings
  m_codeWarnings = toTextEditorSelections(document->diagnosticMessages(), textDocument());
  m_codeWarningsUpdated = false;

  emit cppDocumentUpdated(document);

  m_documentSnapshot = snapshot;
  const auto source = createSemanticInfoSource(false);
  QTC_CHECK(source.snapshot.contains(document->fileName()));
  m_semanticInfoUpdater.updateDetached(source);
}

auto BuiltinEditorDocumentProcessor::onSemanticInfoUpdated(const SemanticInfo semanticInfo) -> void
{
  qCDebug(log) << "semantic info updated" << semanticInfo.doc->fileName() << semanticInfo.revision << semanticInfo.complete;

  emit semanticInfoUpdated(semanticInfo);

  if (m_semanticHighlighter)
    m_semanticHighlighter->run();
}

auto BuiltinEditorDocumentProcessor::onCodeWarningsUpdated(CPlusPlus::Document::Ptr document, const QList<CPlusPlus::Document::DiagnosticMessage> &codeWarnings) -> void
{
  if (document.isNull())
    return;

  if (document->fileName() != filePath())
    return; // some other document got updated

  if (document->editorRevision() != revision())
    return; // outdated content, wait for a new document to be parsed

  if (m_codeWarningsUpdated)
    return; // code warnings already updated

  m_codeWarnings += toTextEditorSelections(codeWarnings, textDocument());
  m_codeWarningsUpdated = true;
  emit codeWarningsUpdated(revision(), m_codeWarnings, HeaderErrorDiagnosticWidgetCreator(), TextEditor::RefactorMarkers());
}

auto BuiltinEditorDocumentProcessor::createSemanticInfoSource(bool force) const -> SemanticInfo::Source
{
  const auto workingCopy = CppModelManager::instance()->workingCopy();
  const auto path = filePath();
  return SemanticInfo::Source(path, workingCopy.source(path), workingCopy.revision(path), m_documentSnapshot, force);
}

} // namespace CppEditor
