// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppeditordocument.hpp"

#include "baseeditordocumentparser.hpp"
#include "builtineditordocumentprocessor.hpp"
#include "cppcodeformatter.hpp"
#include "cppcodemodelsettings.hpp"
#include "cppeditorconstants.hpp"
#include "cppeditorplugin.hpp"
#include "cppmodelmanager.hpp"
#include "cppeditorconstants.hpp"
#include "cppeditorplugin.hpp"
#include "cpphighlighter.hpp"
#include "cppqtstyleindenter.hpp"
#include "cppquickfixassistant.hpp"

#include <core/editormanager/editormanager.hpp>

#include <projectexplorer/session.hpp>

#include <texteditor/icodestylepreferencesfactory.hpp>
#include <texteditor/storagesettings.hpp>
#include <texteditor/textdocumentlayout.hpp>
#include <texteditor/texteditorsettings.hpp>

#include <utils/executeondestruction.hpp>
#include <utils/infobar.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>

#include <QTextDocument>

namespace {

auto mm() -> CppEditor::CppModelManager*
{
  return CppEditor::CppModelManager::instance();
}

} // anonymous namespace

using namespace TextEditor;

namespace CppEditor {
namespace Internal {

enum {
  processDocumentIntervalInMs = 150
};

class CppEditorDocumentHandleImpl : public CppEditorDocumentHandle {
public:
  CppEditorDocumentHandleImpl(CppEditorDocument *cppEditorDocument) : m_cppEditorDocument(cppEditorDocument), m_registrationFilePath(cppEditorDocument->filePath().toString())
  {
    mm()->registerCppEditorDocument(this);
  }

  ~CppEditorDocumentHandleImpl() override
  {
    mm()->unregisterCppEditorDocument(m_registrationFilePath);
  }

  auto filePath() const -> QString override { return m_cppEditorDocument->filePath().toString(); }
  auto contents() const -> QByteArray override { return m_cppEditorDocument->contentsText(); }
  auto revision() const -> unsigned override { return m_cppEditorDocument->contentsRevision(); }

  auto processor() const -> BaseEditorDocumentProcessor* override
  {
    return m_cppEditorDocument->processor();
  }

  auto resetProcessor() -> void override
  {
    m_cppEditorDocument->resetProcessor();
  }

private:
  CppEditor::Internal::CppEditorDocument *const m_cppEditorDocument;
  // The file path of the editor document can change (e.g. by "Save As..."), so make sure
  // that un-registration happens with the path the document was registered.
  const QString m_registrationFilePath;
};

CppEditorDocument::CppEditorDocument() : m_minimizableInfoBars(*infoBar())
{
  setId(CppEditor::Constants::CPPEDITOR_ID);
  setSyntaxHighlighter(new CppHighlighter);

  auto factory = TextEditorSettings::codeStyleFactory(Constants::CPP_SETTINGS_ID);
  setIndenter(factory->createIndenter(document()));

  connect(this, &TextEditor::TextDocument::tabSettingsChanged, this, &CppEditorDocument::invalidateFormatterCache);
  connect(this, &Core::IDocument::mimeTypeChanged, this, &CppEditorDocument::onMimeTypeChanged);

  connect(this, &Core::IDocument::aboutToReload, this, &CppEditorDocument::onAboutToReload);
  connect(this, &Core::IDocument::reloadFinished, this, &CppEditorDocument::onReloadFinished);
  connect(this, &IDocument::filePathChanged, this, &CppEditorDocument::onFilePathChanged);

  connect(&m_parseContextModel, &ParseContextModel::preferredParseContextChanged, this, &CppEditorDocument::reparseWithPreferredParseContext);

  // See also onFilePathChanged() for more initialization
}

auto CppEditorDocument::isObjCEnabled() const -> bool
{
  return m_isObjCEnabled;
}

auto CppEditorDocument::setCompletionAssistProvider(TextEditor::CompletionAssistProvider *provider) -> void
{
  TextDocument::setCompletionAssistProvider(provider);
  m_completionAssistProvider = nullptr;
}

auto CppEditorDocument::setFunctionHintAssistProvider(TextEditor::CompletionAssistProvider *provider) -> void
{
  TextDocument::setFunctionHintAssistProvider(provider);
  m_functionHintAssistProvider = nullptr;
}

auto CppEditorDocument::completionAssistProvider() const -> CompletionAssistProvider*
{
  return m_completionAssistProvider ? m_completionAssistProvider : TextDocument::completionAssistProvider();
}

auto CppEditorDocument::functionHintAssistProvider() const -> CompletionAssistProvider*
{
  return m_functionHintAssistProvider ? m_functionHintAssistProvider : TextDocument::functionHintAssistProvider();
}

auto CppEditorDocument::quickFixAssistProvider() const -> TextEditor::IAssistProvider*
{
  return CppEditorPlugin::instance()->quickFixProvider();
}

auto CppEditorDocument::recalculateSemanticInfoDetached() -> void
{
  auto p = processor();
  QTC_ASSERT(p, return);
  p->recalculateSemanticInfoDetached(true);
}

auto CppEditorDocument::recalculateSemanticInfo() -> SemanticInfo
{
  auto p = processor();
  QTC_ASSERT(p, return SemanticInfo());
  return p->recalculateSemanticInfo();
}

auto CppEditorDocument::contentsText() const -> QByteArray
{
  QMutexLocker locker(&m_cachedContentsLock);

  const auto currentRevision = document()->revision();
  if (m_cachedContentsRevision != currentRevision && !m_fileIsBeingReloaded) {
    m_cachedContentsRevision = currentRevision;
    m_cachedContents = plainText().toUtf8();
  }

  return m_cachedContents;
}

auto CppEditorDocument::applyFontSettings() -> void
{
  if (auto highlighter = syntaxHighlighter())
    highlighter->clearAllExtraFormats(); // Clear all additional formats since they may have changed
  TextDocument::applyFontSettings();     // rehighlights and updates additional formats
  if (m_processor)
    m_processor->semanticRehighlight();
}

auto CppEditorDocument::invalidateFormatterCache() -> void
{
  QtStyleCodeFormatter formatter;
  formatter.invalidateCache(document());
}

auto CppEditorDocument::onMimeTypeChanged() -> void
{
  const auto &mt = mimeType();
  m_isObjCEnabled = (mt == QLatin1String(Constants::OBJECTIVE_C_SOURCE_MIMETYPE) || mt == QLatin1String(Constants::OBJECTIVE_CPP_SOURCE_MIMETYPE));
  m_completionAssistProvider = mm()->completionAssistProvider();
  m_functionHintAssistProvider = mm()->functionHintAssistProvider();

  initializeTimer();
}

auto CppEditorDocument::onAboutToReload() -> void
{
  QTC_CHECK(!m_fileIsBeingReloaded);
  m_fileIsBeingReloaded = true;

  processor()->invalidateDiagnostics();
}

auto CppEditorDocument::onReloadFinished() -> void
{
  QTC_CHECK(m_fileIsBeingReloaded);
  m_fileIsBeingReloaded = false;

  m_processorRevision = document()->revision();
  processDocument();
}

auto CppEditorDocument::reparseWithPreferredParseContext(const QString &parseContextId) -> void
{
  // Update parser
  setPreferredParseContext(parseContextId);

  // Remember the setting
  const QString key = Constants::PREFERRED_PARSE_CONTEXT + filePath().toString();
  ProjectExplorer::SessionManager::setValue(key, parseContextId);

  // Reprocess
  scheduleProcessDocument();
}

auto CppEditorDocument::onFilePathChanged(const Utils::FilePath &oldPath, const Utils::FilePath &newPath) -> void
{
  Q_UNUSED(oldPath)

  if (!newPath.isEmpty()) {
    indenter()->setFileName(newPath);
    setMimeType(Utils::mimeTypeForFile(newPath.toFileInfo()).name());

    connect(this, &Core::IDocument::contentsChanged, this, &CppEditorDocument::scheduleProcessDocument, Qt::UniqueConnection);

    // Un-Register/Register in ModelManager
    m_editorDocumentHandle.reset();
    m_editorDocumentHandle.reset(new CppEditorDocumentHandleImpl(this));

    resetProcessor();
    applyPreferredParseContextFromSettings();
    applyExtraPreprocessorDirectivesFromSettings();
    m_processorRevision = document()->revision();
    processDocument();
  }
}

auto CppEditorDocument::scheduleProcessDocument() -> void
{
  if (m_fileIsBeingReloaded)
    return;

  m_processorRevision = document()->revision();
  m_processorTimer.start();
  processor()->editorDocumentTimerRestarted();
}

auto CppEditorDocument::processDocument() -> void
{
  processor()->invalidateDiagnostics();

  if (processor()->isParserRunning() || m_processorRevision != contentsRevision()) {
    m_processorTimer.start();
    processor()->editorDocumentTimerRestarted();
    return;
  }

  m_processorTimer.stop();
  if (m_fileIsBeingReloaded || filePath().isEmpty())
    return;

  processor()->run();
}

auto CppEditorDocument::resetProcessor() -> void
{
  releaseResources();
  processor(); // creates a new processor
}

auto CppEditorDocument::applyPreferredParseContextFromSettings() -> void
{
  if (filePath().isEmpty())
    return;

  const QString key = Constants::PREFERRED_PARSE_CONTEXT + filePath().toString();
  const auto parseContextId = ProjectExplorer::SessionManager::value(key).toString();

  setPreferredParseContext(parseContextId);
}

auto CppEditorDocument::applyExtraPreprocessorDirectivesFromSettings() -> void
{
  if (filePath().isEmpty())
    return;

  const QString key = Constants::EXTRA_PREPROCESSOR_DIRECTIVES + filePath().toString();
  const auto directives = ProjectExplorer::SessionManager::value(key).toString().toUtf8();

  setExtraPreprocessorDirectives(directives);
}

auto CppEditorDocument::setExtraPreprocessorDirectives(const QByteArray &directives) -> void
{
  const auto parser = processor()->parser();
  QTC_ASSERT(parser, return);

  auto config = parser->configuration();
  if (config.editorDefines != directives) {
    config.editorDefines = directives;
    processor()->setParserConfig(config);

    emit preprocessorSettingsChanged(!directives.trimmed().isEmpty());
  }
}

auto CppEditorDocument::setPreferredParseContext(const QString &parseContextId) -> void
{
  const auto parser = processor()->parser();
  QTC_ASSERT(parser, return);

  auto config = parser->configuration();
  if (config.preferredProjectPartId != parseContextId) {
    config.preferredProjectPartId = parseContextId;
    processor()->setParserConfig(config);
  }
}

auto CppEditorDocument::contentsRevision() const -> unsigned
{
  return document()->revision();
}

auto CppEditorDocument::releaseResources() -> void
{
  if (m_processor)
    disconnect(m_processor.data(), nullptr, this, nullptr);
  m_processor.reset();
}

auto CppEditorDocument::showHideInfoBarAboutMultipleParseContexts(bool show) -> void
{
  const Utils::Id id = Constants::MULTIPLE_PARSE_CONTEXTS_AVAILABLE;

  if (show) {
    Utils::InfoBarEntry info(id, tr("Note: Multiple parse contexts are available for this file. " "Choose the preferred one from the editor toolbar."), Utils::InfoBarEntry::GlobalSuppression::Enabled);
    info.removeCancelButton();
    if (infoBar()->canInfoBeAdded(id))
      infoBar()->addInfo(info);
  } else {
    infoBar()->removeInfo(id);
  }
}

auto CppEditorDocument::initializeTimer() -> void
{
  m_processorTimer.setSingleShot(true);
  m_processorTimer.setInterval(processDocumentIntervalInMs);

  connect(&m_processorTimer, &QTimer::timeout, this, &CppEditorDocument::processDocument, Qt::UniqueConnection);
}

auto CppEditorDocument::parseContextModel() -> ParseContextModel&
{
  return m_parseContextModel;
}

auto CppEditorDocument::cursorInfo(const CursorInfoParams &params) -> QFuture<CursorInfo>
{
  return processor()->cursorInfo(params);
}

auto CppEditorDocument::minimizableInfoBars() const -> const MinimizableInfoBars&
{
  return m_minimizableInfoBars;
}

auto CppEditorDocument::processor() -> BaseEditorDocumentProcessor*
{
  if (!m_processor) {
    m_processor.reset(mm()->createEditorDocumentProcessor(this));
    connect(m_processor.data(), &BaseEditorDocumentProcessor::projectPartInfoUpdated, [this](const ProjectPartInfo &info) {
      const auto hasProjectPart = !(info.hints & ProjectPartInfo::IsFallbackMatch);
      m_minimizableInfoBars.processHasProjectPart(hasProjectPart);
      m_parseContextModel.update(info);
      const bool isAmbiguous = info.hints & ProjectPartInfo::IsAmbiguousMatch;
      const bool isProjectFile = info.hints & ProjectPartInfo::IsFromProjectMatch;
      showHideInfoBarAboutMultipleParseContexts(isAmbiguous && isProjectFile);
    });
    connect(m_processor.data(), &BaseEditorDocumentProcessor::codeWarningsUpdated, [this](unsigned revision, const QList<QTextEdit::ExtraSelection> selections, const std::function<QWidget*()> &creator, const TextEditor::RefactorMarkers &refactorMarkers) {
      emit codeWarningsUpdated(revision, selections, refactorMarkers);
      m_minimizableInfoBars.processHeaderDiagnostics(creator);
    });
    connect(m_processor.data(), &BaseEditorDocumentProcessor::ifdefedOutBlocksUpdated, this, &CppEditorDocument::ifdefedOutBlocksUpdated);
    connect(m_processor.data(), &BaseEditorDocumentProcessor::cppDocumentUpdated, [this](const CPlusPlus::Document::Ptr document) {
      // Update syntax highlighter
      auto *highlighter = qobject_cast<CppHighlighter*>(syntaxHighlighter());
      highlighter->setLanguageFeatures(document->languageFeatures());

      // Forward signal
      emit cppDocumentUpdated(document);

    });
    connect(m_processor.data(), &BaseEditorDocumentProcessor::semanticInfoUpdated, this, &CppEditorDocument::semanticInfoUpdated);
  }

  return m_processor.data();
}

auto CppEditorDocument::tabSettings() const -> TextEditor::TabSettings
{
  return indenter()->tabSettings().value_or(TextEditor::TextDocument::tabSettings());
}

auto CppEditorDocument::save(QString *errorString, const Utils::FilePath &filePath, bool autoSave) -> bool
{
  Utils::ExecuteOnDestruction resetSettingsOnScopeExit;

  if (indenter()->formatOnSave() && !autoSave) {
    auto *layout = qobject_cast<TextEditor::TextDocumentLayout*>(document()->documentLayout());
    const auto documentRevision = layout->lastSaveRevision;

    TextEditor::RangesInLines editedRanges;
    TextEditor::RangeInLines lastRange{-1, -1};
    for (auto i = 0; i < document()->blockCount(); ++i) {
      const auto block = document()->findBlockByNumber(i);
      if (block.revision() == documentRevision) {
        if (lastRange.startLine != -1)
          editedRanges.push_back(lastRange);

        lastRange.startLine = lastRange.endLine = -1;
        continue;
      }

      // block.revision() != documentRevision
      if (lastRange.startLine == -1)
        lastRange.startLine = block.blockNumber() + 1;
      lastRange.endLine = block.blockNumber() + 1;
    }

    if (lastRange.startLine != -1)
      editedRanges.push_back(lastRange);

    if (!editedRanges.empty()) {
      QTextCursor cursor(document());
      cursor.beginEditBlock();
      indenter()->format(editedRanges);
      cursor.endEditBlock();
    }

    auto settings = storageSettings();
    resetSettingsOnScopeExit.reset([this, defaultSettings = settings]() { setStorageSettings(defaultSettings); });
    settings.m_cleanWhitespace = false;
    setStorageSettings(settings);
  }

  return TextEditor::TextDocument::save(errorString, filePath, autoSave);
}

} // namespace Internal
} // namespace CppEditor
