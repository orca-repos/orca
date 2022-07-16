// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "baseeditordocumentprocessor.hpp"
#include "cppcompletionassistprovider.hpp"
#include "cppminimizableinfobars.hpp"
#include "cppmodelmanager.hpp"
#include "cppparsecontext.hpp"
#include "cppsemanticinfo.hpp"
#include "editordocumenthandle.hpp"

#include <texteditor/textdocument.hpp>

#include <QMutex>
#include <QTimer>

namespace CppEditor {
namespace Internal {

class CppEditorDocument : public TextEditor::TextDocument {
  Q_OBJECT friend class CppEditorDocumentHandleImpl;

public:
  explicit CppEditorDocument();

  auto isObjCEnabled() const -> bool;
  auto setCompletionAssistProvider(TextEditor::CompletionAssistProvider *provider) -> void override;
  auto setFunctionHintAssistProvider(TextEditor::CompletionAssistProvider *provider) -> void override;
  auto completionAssistProvider() const -> TextEditor::CompletionAssistProvider* override;
  auto functionHintAssistProvider() const -> TextEditor::CompletionAssistProvider* override;
  auto quickFixAssistProvider() const -> TextEditor::IAssistProvider* override;
  auto recalculateSemanticInfoDetached() -> void;
  auto recalculateSemanticInfo() -> SemanticInfo; // TODO: Remove me
  auto setPreferredParseContext(const QString &parseContextId) -> void;
  auto setExtraPreprocessorDirectives(const QByteArray &directives) -> void;
  auto scheduleProcessDocument() -> void;
  auto minimizableInfoBars() const -> const MinimizableInfoBars&;
  auto parseContextModel() -> ParseContextModel&;
  auto cursorInfo(const CursorInfoParams &params) -> QFuture<CursorInfo>;
  auto tabSettings() const -> TextEditor::TabSettings override;
  auto save(QString *errorString, const Utils::FilePath &filePath = Utils::FilePath(), bool autoSave = false) -> bool override;

signals:
  auto codeWarningsUpdated(unsigned contentsRevision, const QList<QTextEdit::ExtraSelection> selections, const TextEditor::RefactorMarkers &refactorMarkers) -> void;
  auto ifdefedOutBlocksUpdated(unsigned contentsRevision, const QList<TextEditor::BlockRange> ifdefedOutBlocks) -> void;
  auto cppDocumentUpdated(const CPlusPlus::Document::Ptr document) -> void; // TODO: Remove me
  auto semanticInfoUpdated(const SemanticInfo semanticInfo) -> void;        // TODO: Remove me
  auto preprocessorSettingsChanged(bool customSettings) -> void;

protected:
  auto applyFontSettings() -> void override;

private:
  auto invalidateFormatterCache() -> void;
  auto onFilePathChanged(const Utils::FilePath &oldPath, const Utils::FilePath &newPath) -> void;
  auto onMimeTypeChanged() -> void;
  auto onAboutToReload() -> void;
  auto onReloadFinished() -> void;
  auto reparseWithPreferredParseContext(const QString &id) -> void;
  auto processDocument() -> void;
  auto contentsText() const -> QByteArray;
  auto contentsRevision() const -> unsigned;
  auto processor() -> BaseEditorDocumentProcessor*;
  auto resetProcessor() -> void;
  auto applyPreferredParseContextFromSettings() -> void;
  auto applyExtraPreprocessorDirectivesFromSettings() -> void;
  auto releaseResources() -> void;
  auto showHideInfoBarAboutMultipleParseContexts(bool show) -> void;
  auto initializeTimer() -> void;

  bool m_fileIsBeingReloaded = false;
  bool m_isObjCEnabled = false;

  // Caching contents
  mutable QMutex m_cachedContentsLock;
  mutable QByteArray m_cachedContents;
  mutable int m_cachedContentsRevision = -1;
  unsigned m_processorRevision = 0;
  QTimer m_processorTimer;
  QScopedPointer<BaseEditorDocumentProcessor> m_processor;
  CppCompletionAssistProvider *m_completionAssistProvider = nullptr;
  CppCompletionAssistProvider *m_functionHintAssistProvider = nullptr;
  // (Un)Registration in CppModelManager
  QScopedPointer<CppEditorDocumentHandle> m_editorDocumentHandle;
  MinimizableInfoBars m_minimizableInfoBars;
  ParseContextModel m_parseContextModel;
};

} // namespace Internal
} // namespace CppEditor
