// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppuseselectionsupdater.hpp"

#include "cppeditorwidget.hpp"
#include "cppeditordocument.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolsreuse.hpp"

#include <utils/textutils.hpp>

#include <QTextBlock>
#include <QTextCursor>

#include <utils/qtcassert.hpp>

enum {
  updateUseSelectionsInternalInMs = 500
};

namespace CppEditor {
namespace Internal {

CppUseSelectionsUpdater::CppUseSelectionsUpdater(CppEditorWidget *editorWidget) : m_editorWidget(editorWidget)
{
  m_timer.setSingleShot(true);
  m_timer.setInterval(updateUseSelectionsInternalInMs);
  connect(&m_timer, &QTimer::timeout, this, [this]() { update(); });
}

CppUseSelectionsUpdater::~CppUseSelectionsUpdater()
{
  if (m_runnerWatcher)
    m_runnerWatcher->cancel();
}

auto CppUseSelectionsUpdater::scheduleUpdate() -> void
{
  m_timer.start();
}

auto CppUseSelectionsUpdater::abortSchedule() -> void
{
  m_timer.stop();
}

auto CppUseSelectionsUpdater::update(CallType callType) -> CppUseSelectionsUpdater::RunnerInfo
{
  auto *cppEditorWidget = qobject_cast<CppEditorWidget*>(m_editorWidget);
  QTC_ASSERT(cppEditorWidget, return RunnerInfo::FailedToStart);

  auto *cppEditorDocument = qobject_cast<CppEditorDocument*>(cppEditorWidget->textDocument());
  QTC_ASSERT(cppEditorDocument, return RunnerInfo::FailedToStart);

  m_updateSelections = CppModelManager::instance()->supportsLocalUses(cppEditorDocument);

  CursorInfoParams params;
  params.semanticInfo = cppEditorWidget->semanticInfo();
  params.textCursor = Utils::Text::wordStartCursor(cppEditorWidget->textCursor());

  if (callType == CallType::Asynchronous) {
    if (isSameIdentifierAsBefore(params.textCursor))
      return RunnerInfo::AlreadyUpToDate;

    if (m_runnerWatcher)
      m_runnerWatcher->cancel();

    m_runnerWatcher.reset(new QFutureWatcher<CursorInfo>);
    connect(m_runnerWatcher.data(), &QFutureWatcherBase::finished, this, &CppUseSelectionsUpdater::onFindUsesFinished);

    m_runnerRevision = m_editorWidget->document()->revision();
    m_runnerWordStartPosition = params.textCursor.position();

    m_runnerWatcher->setFuture(cppEditorDocument->cursorInfo(params));
    return RunnerInfo::Started;
  } else {
    // synchronous case
    abortSchedule();

    const auto startRevision = cppEditorDocument->document()->revision();
    auto future = cppEditorDocument->cursorInfo(params);
    if (future.isCanceled())
      return RunnerInfo::Invalid;

    // QFuture::waitForFinished seems to block completely, not even
    // allowing to process events from QLocalSocket.
    while (!future.isFinished()) {
      if (future.isCanceled())
        return RunnerInfo::Invalid;

      QTC_ASSERT(startRevision == cppEditorDocument->document()->revision(), return RunnerInfo::Invalid);
      QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    }

    processResults(future.result());
    return RunnerInfo::Invalid;
  }
}

auto CppUseSelectionsUpdater::isSameIdentifierAsBefore(const QTextCursor &cursorAtWordStart) const -> bool
{
  return m_runnerRevision != -1 && m_runnerRevision == m_editorWidget->document()->revision() && m_runnerWordStartPosition == cursorAtWordStart.position();
}

auto CppUseSelectionsUpdater::processResults(const CursorInfo &result) -> void
{
  if (m_updateSelections) {
    ExtraSelections localVariableSelections;
    if (!result.useRanges.isEmpty() || !currentUseSelections().isEmpty()) {
      auto selections = updateUseSelections(result.useRanges);
      if (result.areUseRangesForLocalVariable)
        localVariableSelections = selections;
    }
    updateUnusedSelections(result.unusedVariablesRanges);
    emit selectionsForVariableUnderCursorUpdated(localVariableSelections);
  }
  emit finished(result.localUses, true);
}

auto CppUseSelectionsUpdater::onFindUsesFinished() -> void
{
  QTC_ASSERT(m_runnerWatcher, emit finished(SemanticInfo::LocalUseMap(), false); return);

  if (m_runnerWatcher->isCanceled()) {
    emit finished(SemanticInfo::LocalUseMap(), false);
    return;
  }
  if (m_runnerRevision != m_editorWidget->document()->revision()) {
    emit finished(SemanticInfo::LocalUseMap(), false);
    return;
  }
  if (m_runnerWordStartPosition != Utils::Text::wordStartCursor(m_editorWidget->textCursor()).position()) {
    emit finished(SemanticInfo::LocalUseMap(), false);
    return;
  }
  if (m_editorWidget->isRenaming()) {
    emit finished({}, false);
    return;
  }

  processResults(m_runnerWatcher->result());

  m_runnerWatcher.reset();
}

auto CppUseSelectionsUpdater::toExtraSelections(const CursorInfo::Ranges &ranges, TextEditor::TextStyle style) -> CppUseSelectionsUpdater::ExtraSelections
{
  CppUseSelectionsUpdater::ExtraSelections selections;
  selections.reserve(ranges.size());

  for (const auto &range : ranges) {
    auto document = m_editorWidget->document();
    const auto position = document->findBlockByNumber(static_cast<int>(range.line) - 1).position() + static_cast<int>(range.column) - 1;
    const auto anchor = position + static_cast<int>(range.length);

    QTextEdit::ExtraSelection sel;
    sel.format = m_editorWidget->textDocument()->fontSettings().toTextCharFormat(style);
    sel.cursor = QTextCursor(document);
    sel.cursor.setPosition(anchor);
    sel.cursor.setPosition(position, QTextCursor::KeepAnchor);

    selections.append(sel);
  }

  return selections;
}

auto CppUseSelectionsUpdater::currentUseSelections() const -> CppUseSelectionsUpdater::ExtraSelections
{
  return m_editorWidget->extraSelections(TextEditor::TextEditorWidget::CodeSemanticsSelection);
}

auto CppUseSelectionsUpdater::updateUseSelections(const CursorInfo::Ranges &ranges) -> CppUseSelectionsUpdater::ExtraSelections
{
  const auto selections = toExtraSelections(ranges, TextEditor::C_OCCURRENCES);
  m_editorWidget->setExtraSelections(TextEditor::TextEditorWidget::CodeSemanticsSelection, selections);

  return selections;
}

auto CppUseSelectionsUpdater::updateUnusedSelections(const CursorInfo::Ranges &ranges) -> void
{
  const auto selections = toExtraSelections(ranges, TextEditor::C_OCCURRENCES_UNUSED);
  m_editorWidget->setExtraSelections(TextEditor::TextEditorWidget::UnusedSymbolSelection, selections);
}

} // namespace Internal
} // namespace CppEditor
