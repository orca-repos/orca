// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <texteditor/codeassist/assistenums.hpp>
#include <texteditor/texteditor.hpp>
#include "texteditor/blockrange.hpp"

#include <QScopedPointer>


namespace TextEditor {
class IAssistProposal;
class IAssistProvider;
}

namespace CppEditor {
class FollowSymbolInterface;
class SemanticInfo;
class ProjectPart;

namespace Internal {
class CppEditorDocument;
class CppEditorOutline;
class CppEditorWidgetPrivate;
class FunctionDeclDefLink;
} // namespace Internal

class CPPEDITOR_EXPORT CppEditorWidget : public TextEditor::TextEditorWidget {
  Q_OBJECT

public:
  CppEditorWidget();
  ~CppEditorWidget() override;

  auto cppEditorDocument() const -> Internal::CppEditorDocument*;
  auto outline() const -> Internal::CppEditorOutline*;
  auto isSemanticInfoValidExceptLocalUses() const -> bool;
  auto isSemanticInfoValid() const -> bool;
  auto isRenaming() const -> bool;
  auto declDefLink() const -> QSharedPointer<Internal::FunctionDeclDefLink>;
  auto applyDeclDefLinkChanges(bool jumpToMatch) -> void;
  auto createAssistInterface(TextEditor::AssistKind kind, TextEditor::AssistReason reason) const -> TextEditor::AssistInterface* override;
  auto encourageApply() -> void override;
  auto paste() -> void override;
  auto cut() -> void override;
  auto selectAll() -> void override;
  auto switchDeclarationDefinition(bool inNextSplit) -> void;
  auto showPreProcessorWidget() -> void;
  auto findUsages() -> void override;
  auto findUsages(QTextCursor cursor) -> void;
  auto renameUsages(const QString &replacement = QString(), QTextCursor cursor = QTextCursor()) -> void;
  auto renameSymbolUnderCursor() -> void override;
  auto selectBlockUp() -> bool override;
  auto selectBlockDown() -> bool override;

  static auto updateWidgetHighlighting(QWidget *widget, bool highlight) -> void;
  static auto isWidgetHighlighted(QWidget *widget) -> bool;

  auto semanticInfo() const -> SemanticInfo;
  auto updateSemanticInfo() -> void;
  auto invokeTextEditorWidgetAssist(TextEditor::AssistKind assistKind, TextEditor::IAssistProvider *provider) -> void;

  static auto unselectLeadingWhitespace(const QList<QTextEdit::ExtraSelection> &selections) -> const QList<QTextEdit::ExtraSelection>;

  auto isInTestMode() const -> bool;
  auto setProposals(const TextEditor::IAssistProposal *immediateProposal, const TextEditor::IAssistProposal *finalProposal) -> void;
  #ifdef WITH_TESTS
    void enableTestMode();
signals:
    void proposalsReady(const TextEditor::IAssistProposal *immediateProposal,
                        const TextEditor::IAssistProposal *finalProposal);
  #endif

protected:
  auto event(QEvent *e) -> bool override;
  auto contextMenuEvent(QContextMenuEvent *) -> void override;
  auto keyPressEvent(QKeyEvent *e) -> void override;
  auto handleStringSplitting(QKeyEvent *e) const -> bool;
  auto findLinkAt(const QTextCursor &cursor, Utils::ProcessLinkCallback &&processLinkCallback, bool resolveTarget = true, bool inNextSplit = false) -> void override;
  auto slotCodeStyleSettingsChanged(const QVariant &) -> void override;

private:
  auto updateFunctionDeclDefLink() -> void;
  auto updateFunctionDeclDefLinkNow() -> void;
  auto abortDeclDefLink() -> void;
  auto onFunctionDeclDefLinkFound(QSharedPointer<Internal::FunctionDeclDefLink> link) -> void;
  auto onCppDocumentUpdated() -> void;
  auto onCodeWarningsUpdated(unsigned revision, const QList<QTextEdit::ExtraSelection> selections, const TextEditor::RefactorMarkers &refactorMarkers) -> void;
  auto onIfdefedOutBlocksUpdated(unsigned revision, const QList<TextEditor::BlockRange> ifdefedOutBlocks) -> void;
  auto onShowInfoBarAction(const Utils::Id &id, bool show) -> void;
  auto updateSemanticInfo(const SemanticInfo &semanticInfo, bool updateUseSelectionSynchronously = false) -> void;
  auto updatePreprocessorButtonTooltip() -> void;
  auto processKeyNormally(QKeyEvent *e) -> void;
  auto finalizeInitialization() -> void override;
  auto finalizeInitializationAfterDuplication(TextEditorWidget *other) -> void override;
  auto documentRevision() const -> unsigned;
  auto createRefactorMenu(QWidget *parent) const -> QMenu*;
  auto followSymbolInterface() const -> FollowSymbolInterface&;
  auto projectPart() const -> const ProjectPart*;

  QScopedPointer<Internal::CppEditorWidgetPrivate> d;
};

} // namespace CppEditor
