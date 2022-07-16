// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppcursorinfo.hpp"
#include "cppsemanticinfo.hpp"

#include <QFutureWatcher>
#include <QTextEdit>
#include <QTimer>

namespace CppEditor {
class CppEditorWidget;

namespace Internal {

class CppUseSelectionsUpdater : public QObject {
  Q_OBJECT Q_DISABLE_COPY(CppUseSelectionsUpdater)

public:
  explicit CppUseSelectionsUpdater(CppEditorWidget *editorWidget);
  ~CppUseSelectionsUpdater() override;

  auto scheduleUpdate() -> void;
  auto abortSchedule() -> void;

  enum class CallType {
    Synchronous,
    Asynchronous
  };

  enum class RunnerInfo {
    AlreadyUpToDate,
    Started,
    FailedToStart,
    Invalid
  }; // For async case.
  auto update(CallType callType = CallType::Asynchronous) -> RunnerInfo;

signals:
  auto finished(SemanticInfo::LocalUseMap localUses, bool success) -> void;
  auto selectionsForVariableUnderCursorUpdated(const QList<QTextEdit::ExtraSelection> &) -> void;

private:
  CppUseSelectionsUpdater();

  using ExtraSelections = QList<QTextEdit::ExtraSelection>;

  auto isSameIdentifierAsBefore(const QTextCursor &cursorAtWordStart) const -> bool;
  auto processResults(const CursorInfo &result) -> void;
  auto onFindUsesFinished() -> void;
  auto toExtraSelections(const CursorInfo::Ranges &ranges, TextEditor::TextStyle style) -> ExtraSelections;
  auto currentUseSelections() const -> ExtraSelections;
  auto updateUseSelections(const CursorInfo::Ranges &selections) -> ExtraSelections;
  auto updateUnusedSelections(const CursorInfo::Ranges &selections) -> void;

  CppEditorWidget *const m_editorWidget;
  QTimer m_timer;
  QScopedPointer<QFutureWatcher<CursorInfo>> m_runnerWatcher;
  int m_runnerRevision = -1;
  int m_runnerWordStartPosition = -1;
  bool m_updateSelections = true;
};

} // namespace Internal
} // namespace CppEditor
