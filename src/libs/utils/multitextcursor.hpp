// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QKeySequence>
#include <QTextCursor>

QT_BEGIN_NAMESPACE
class QKeyEvent;
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT MultiTextCursor {
public:
  MultiTextCursor();
  explicit MultiTextCursor(const QList<QTextCursor> &cursors);

  /// replace all cursors with \param cursors and the last one will be the new main cursors
  auto setCursors(const QList<QTextCursor> &cursors) -> void;
  auto cursors() const -> const QList<QTextCursor>;

  /// \returns whether this multi cursor contains any cursor
  auto isNull() const -> bool;
  /// \returns whether this multi cursor contains more than one cursor
  auto hasMultipleCursors() const -> bool;
  /// \returns the number of cursors handled by this cursor
  auto cursorCount() const -> int;

  /// the \param cursor that is appended by added by \brief addCursor
  /// will be interpreted as the new main cursor
  auto addCursor(const QTextCursor &cursor) -> void;
  /// convenience function that removes the old main cursor and appends
  /// \param cursor as the new main cursor
  auto replaceMainCursor(const QTextCursor &cursor) -> void;
  /// \returns the main cursor
  auto mainCursor() const -> QTextCursor;
  /// \returns the main cursor and removes it from this multi cursor
  auto takeMainCursor() -> QTextCursor;

  auto beginEditBlock() -> void;
  auto endEditBlock() -> void;
  /// merges overlapping cursors together
  auto mergeCursors() -> void;

  /// applies the move key event \param e to all cursors in this multi cursor
  auto handleMoveKeyEvent(QKeyEvent *e, QPlainTextEdit *edit, bool camelCaseNavigationEnabled) -> bool;
  /// applies the move \param operation to all cursors in this multi cursor \param n times
  /// with the move \param mode
  auto movePosition(QTextCursor::MoveOperation operation, QTextCursor::MoveMode mode, int n = 1) -> void;

  /// \returns whether any cursor has a selection
  auto hasSelection() const -> bool;
  /// \returns the selected text of all cursors that have a selection separated by
  /// a newline character
  auto selectedText() const -> QString;
  /// removes the selected text of all cursors that have a selection from the document
  auto removeSelectedText() -> void;

  /// inserts \param text into all cursors, potentially removing correctly selected text
  auto insertText(const QString &text, bool selectNewText = false) -> void;

  auto operator==(const MultiTextCursor &other) const -> bool;
  auto operator!=(const MultiTextCursor &other) const -> bool;

  using iterator = QList<QTextCursor>::iterator;
  using const_iterator = QList<QTextCursor>::const_iterator;

  auto begin() -> iterator { return m_cursors.begin(); }
  auto end() -> iterator { return m_cursors.end(); }
  auto begin() const -> const_iterator { return m_cursors.begin(); }
  auto end() const -> const_iterator { return m_cursors.end(); }
  auto constBegin() const -> const_iterator { return m_cursors.constBegin(); }
  auto constEnd() const -> const_iterator { return m_cursors.constEnd(); }

  static auto multiCursorAddEvent(QKeyEvent *e, QKeySequence::StandardKey matchKey) -> bool;

private:
  QList<QTextCursor> m_cursors;
};

} // namespace Utils
