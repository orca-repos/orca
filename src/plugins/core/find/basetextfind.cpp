// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "basetextfind.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/filesearch.hpp>

#include <QPointer>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>

namespace Core {

auto BaseTextFind::regularExpression(const QString &txt, const FindFlags flags) -> QRegularExpression
{
  return QRegularExpression(flags & FindRegularExpression ? txt : QRegularExpression::escape(txt), flags & FindCaseSensitively ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);
}

struct BaseTextFindPrivate {
  explicit BaseTextFindPrivate(QPlainTextEdit *editor);
  explicit BaseTextFindPrivate(QTextEdit *editor);

  QPointer<QTextEdit> m_editor;
  QPointer<QPlainTextEdit> m_plaineditor;
  QPointer<QWidget> m_widget;
  Utils::MultiTextCursor m_scope;
  std::function<Utils::MultiTextCursor()> m_cursor_provider;
  int m_incremental_start_pos;
  bool m_incremental_wrapped_state;
};

BaseTextFindPrivate::BaseTextFindPrivate(QTextEdit *editor) : m_editor(editor), m_widget(editor), m_incremental_start_pos(-1), m_incremental_wrapped_state(false) {}
BaseTextFindPrivate::BaseTextFindPrivate(QPlainTextEdit *editor) : m_plaineditor(editor), m_widget(editor), m_incremental_start_pos(-1), m_incremental_wrapped_state(false) {}

/*!
    \class Core::BaseTextFind
    \inheaderfile coreplugin/find/basetextfind.h
    \inmodule Orca

    \brief The BaseTextFind class implements a find filter for QPlainTextEdit
    and QTextEdit based widgets.

    \sa Core::IFindFilter
*/

/*!
    \fn void Core::BaseTextFind::findScopeChanged(const Utils::MultiTextCursor &cursor)

    This signal is emitted when the search
    scope changes to \a cursor.
*/

/*!
    \fn void Core::BaseTextFind::highlightAllRequested(const QString &txt, Core::FindFlags findFlags)

    This signal is emitted when the search results for \a txt using the given
    \a findFlags should be highlighted in the editor widget.
*/

/*!
    \internal
*/
BaseTextFind::BaseTextFind(QTextEdit *editor) : d(new BaseTextFindPrivate(editor)) {}

/*!
    \internal
*/
BaseTextFind::BaseTextFind(QPlainTextEdit *editor) : d(new BaseTextFindPrivate(editor)) {}

/*!
    \internal
*/
BaseTextFind::~BaseTextFind()
{
  delete d;
}

auto BaseTextFind::textCursor() const -> QTextCursor
{
  QTC_ASSERT(d->m_editor || d->m_plaineditor, return QTextCursor());
  return d->m_editor ? d->m_editor->textCursor() : d->m_plaineditor->textCursor();
}

auto BaseTextFind::setTextCursor(const QTextCursor &cursor) const -> void
{
  QTC_ASSERT(d->m_editor || d->m_plaineditor, return);
  d->m_editor ? d->m_editor->setTextCursor(cursor) : d->m_plaineditor->setTextCursor(cursor);
}

auto BaseTextFind::document() const -> QTextDocument*
{
  QTC_ASSERT(d->m_editor || d->m_plaineditor, return nullptr);
  return d->m_editor ? d->m_editor->document() : d->m_plaineditor->document();
}

auto BaseTextFind::isReadOnly() const -> bool
{
  QTC_ASSERT(d->m_editor || d->m_plaineditor, return true);
  return d->m_editor ? d->m_editor->isReadOnly() : d->m_plaineditor->isReadOnly();
}

/*!
    \reimp
*/
auto BaseTextFind::supportsReplace() const -> bool
{
  return !isReadOnly();
}

/*!
    \reimp
*/
auto BaseTextFind::supportedFindFlags() const -> FindFlags
{
  return FindBackward | FindCaseSensitively | FindRegularExpression | FindWholeWords | FindPreserveCase;
}

/*!
    \reimp
*/
auto BaseTextFind::resetIncrementalSearch() -> void
{
  d->m_incremental_start_pos = -1;
  d->m_incremental_wrapped_state = false;
}

/*!
    \reimp
*/
auto BaseTextFind::clearHighlights() -> void
{
  highlightAll(QString(), {});
}

/*!
    \reimp
*/
auto BaseTextFind::currentFindString() const -> QString
{
  auto cursor = textCursor();
  if (cursor.hasSelection() && cursor.block() != cursor.document()->findBlock(cursor.anchor()))
    return {}; // multi block selection

  if (cursor.hasSelection())
    return cursor.selectedText();

  if (!cursor.atBlockEnd() && !cursor.hasSelection()) {
    cursor.movePosition(QTextCursor::StartOfWord);
    cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
    auto s = cursor.selectedText();
    foreach(QChar c, s) {
      if (!c.isLetterOrNumber() && c != QLatin1Char('_')) {
        s.clear();
        break;
      }
    }
    return s;
  }

  return {};
}

/*!
    \reimp
*/
auto BaseTextFind::completedFindString() const -> QString
{
  auto cursor = textCursor();
  cursor.setPosition(textCursor().selectionStart());
  cursor.movePosition(QTextCursor::EndOfWord, QTextCursor::KeepAnchor);
  return cursor.selectedText();
}

/*!
    \reimp
*/
auto BaseTextFind::findIncremental(const QString &txt, const FindFlags find_flags) -> Result
{
  auto cursor = textCursor();

  if (d->m_incremental_start_pos < 0)
    d->m_incremental_start_pos = cursor.selectionStart();

  cursor.setPosition(d->m_incremental_start_pos);
  auto wrapped = false;
  const auto found = find(txt, find_flags, cursor, &wrapped);

  if (wrapped != d->m_incremental_wrapped_state && found) {
    d->m_incremental_wrapped_state = wrapped;
    showWrapIndicator(d->m_widget);
  }

  if (found)
    highlightAll(txt, find_flags);
  else
    highlightAll(QString(), {});

  return found ? Found : NotFound;
}

/*!
    \reimp
*/
auto BaseTextFind::findStep(const QString &txt, const FindFlags find_flags) -> Result
{
  auto wrapped = false;
  const auto found = find(txt, find_flags, textCursor(), &wrapped);

  if (wrapped)
    showWrapIndicator(d->m_widget);

  if (found) {
    d->m_incremental_start_pos = textCursor().selectionStart();
    d->m_incremental_wrapped_state = false;
  }

  return found ? Found : NotFound;
}

/*!
    \reimp
*/
auto BaseTextFind::replace(const QString &before, const QString &after, const FindFlags find_flags) -> void
{
  const auto cursor = replaceInternal(before, after, find_flags);
  setTextCursor(cursor);
}

// QTextCursor::insert moves all other QTextCursors that are the the insertion point forward.
// We do not want that for the replace operation, because then e.g. the find scope would move when
// replacing a match at the start.
static auto insertTextAfterSelection(const QString &text, QTextCursor &cursor) -> void
{
  // first insert after the cursor's selection end, then remove selection
  const auto start = cursor.selectionStart();
  const auto end = cursor.selectionEnd();
  auto insert_cursor = cursor;
  insert_cursor.beginEditBlock();
  insert_cursor.setPosition(end);
  insert_cursor.insertText(text);
  // change cursor to be behind the inserted text, like it would be when directly inserting
  cursor = insert_cursor;
  // redo the selection, because that changed when inserting the text at the end...
  insert_cursor.setPosition(start);
  insert_cursor.setPosition(end, QTextCursor::KeepAnchor);
  insert_cursor.removeSelectedText();
  insert_cursor.endEditBlock();
}

auto BaseTextFind::replaceInternal(const QString &before, const QString &after, const FindFlags find_flags) const -> QTextCursor
{
  auto cursor = textCursor();
  const bool uses_reg_exp = find_flags & FindRegularExpression;
  const bool preserve_case = find_flags & FindPreserveCase;
  const auto regexp = regularExpression(before, find_flags);
  if (const auto match = regexp.match(cursor.selectedText()); match.hasMatch()) {
    QString real_after;
    if (uses_reg_exp)
      real_after = Utils::expandRegExpReplacement(after, match.capturedTexts());
    else if (preserve_case)
      real_after = Utils::matchCaseReplacement(cursor.selectedText(), after);
    else
      real_after = after;

    const auto start = cursor.selectionStart();
    insertTextAfterSelection(real_after, cursor);

    if ((find_flags & FindBackward) != 0)
      cursor.setPosition(start);
  }

  return cursor;
}

auto BaseTextFind::multiTextCursor() const -> Utils::MultiTextCursor
{
  if (d->m_cursor_provider)
    return d->m_cursor_provider();
  return Utils::MultiTextCursor({textCursor()});
}

/*!
    \reimp
*/
auto BaseTextFind::replaceStep(const QString &before, const QString &after, const FindFlags find_flags) -> bool
{
  const auto cursor = replaceInternal(before, after, find_flags);
  auto wrapped = false;
  const auto found = find(before, find_flags, cursor, &wrapped);

  if (wrapped)
    showWrapIndicator(d->m_widget);

  return found;
}

/*!
    \reimp
    Returns the number of search hits replaced.
*/
auto BaseTextFind::replaceAll(const QString &before, const QString &after, const FindFlags find_flags) -> int
{
  auto edit_cursor = textCursor();

  if (find_flags.testFlag(FindBackward))
    edit_cursor.movePosition(QTextCursor::End);
  else
    edit_cursor.movePosition(QTextCursor::Start);

  edit_cursor.movePosition(QTextCursor::Start);
  edit_cursor.beginEditBlock();

  auto count = 0;
  const bool uses_reg_exp = find_flags & FindRegularExpression;
  const bool preserve_case = find_flags & FindPreserveCase;
  const auto regexp = regularExpression(before, find_flags);
  auto found = findOne(regexp, edit_cursor, textDocumentFlagsForFindFlags(find_flags));
  auto first = true;

  while (!found.isNull()) {
    if (found == edit_cursor && !first) {
      if (edit_cursor.atEnd())
        break;
      // If the newly found QTextCursor is the same as recently edit one we have to move on,
      // otherwise we would run into an endless loop for some regular expressions
      // like ^ or \b.
      auto& new_pos_cursor = edit_cursor;
      new_pos_cursor.movePosition(find_flags & FindBackward ? QTextCursor::PreviousCharacter : QTextCursor::NextCharacter);
      found = findOne(regexp, new_pos_cursor, textDocumentFlagsForFindFlags(find_flags));
      continue;
    }
    if (first)
      first = false;

    ++count;
    edit_cursor.setPosition(found.selectionStart());
    edit_cursor.setPosition(found.selectionEnd(), QTextCursor::KeepAnchor);
    auto match = regexp.match(found.selectedText());

    QString real_after;

    if (uses_reg_exp)
      real_after = Utils::expandRegExpReplacement(after, match.capturedTexts());
    else if (preserve_case)
      real_after = Utils::matchCaseReplacement(found.selectedText(), after);
    else
      real_after = after;

    insertTextAfterSelection(real_after, edit_cursor);
    found = findOne(regexp, edit_cursor, textDocumentFlagsForFindFlags(find_flags));
  }
  edit_cursor.endEditBlock();
  return count;
}

auto BaseTextFind::find(const QString &txt, const FindFlags find_flags, QTextCursor start, bool *wrapped) const -> bool
{
  if (txt.isEmpty()) {
    setTextCursor(start);
    return true;
  }

  const auto regexp = regularExpression(txt, find_flags);
  auto found = findOne(regexp, start, textDocumentFlagsForFindFlags(find_flags));

  if (wrapped)
    *wrapped = false;

  if (found.isNull()) {
    if ((find_flags & FindBackward) == 0)
      start.movePosition(QTextCursor::Start);
    else
      start.movePosition(QTextCursor::End);

    found = findOne(regexp, start, textDocumentFlagsForFindFlags(find_flags));

    if (found.isNull())
      return false;

    if (wrapped)
      *wrapped = true;
  }
  setTextCursor(found);
  return true;
}

auto BaseTextFind::findOne(const QRegularExpression &expr, QTextCursor from, const QTextDocument::FindFlags options) const -> QTextCursor
{
  auto found = document()->find(expr, from, options);
  while (!found.isNull() && !inScope(found)) {
    if (!found.hasSelection()) {
      if (found.movePosition(options & QTextDocument::FindBackward ? QTextCursor::PreviousCharacter : QTextCursor::NextCharacter)) {
        from = found;
      } else {
        return {};
      }
    } else {
      from.setPosition(options & QTextDocument::FindBackward ? found.selectionStart() : found.selectionEnd());
    }
    found = document()->find(expr, from, options);
  }

  return found;
}

auto BaseTextFind::inScope(const QTextCursor &candidate) const -> bool
{
  if (candidate.isNull())
    return false;
  if (d->m_scope.isNull())
    return true;
  return anyOf(d->m_scope, [candidate](const QTextCursor &scope) {
    return candidate.selectionStart() >= scope.selectionStart() && candidate.selectionEnd() <= scope.selectionEnd();
  });
}

/*!
    \reimp
*/
auto BaseTextFind::defineFindScope() -> void
{
  auto multi_cursor = multiTextCursor();
  auto found_selection = false;
  for (const auto &c : multi_cursor) {
    if (c.hasSelection()) {
      if (found_selection || c.block() != c.document()->findBlock(c.anchor())) {
        auto sorted_cursors = multi_cursor.cursors();
        Utils::sort(sorted_cursors);
        d->m_scope = Utils::MultiTextCursor(sorted_cursors);
        auto cursor = textCursor();
        cursor.clearSelection();
        setTextCursor(cursor);
        emit findScopeChanged(d->m_scope);
        return;
      }
      found_selection = true;
    }
  }
  clearFindScope();
}

/*!
    \reimp
*/
auto BaseTextFind::clearFindScope() -> void
{
  d->m_scope = Utils::MultiTextCursor();
  emit findScopeChanged(d->m_scope);
}

/*!
    \reimp
    Emits highlightAllRequested().
*/
auto BaseTextFind::highlightAll(const QString &txt, const FindFlags find_flags) -> void
{
  emit highlightAllRequested(txt, find_flags);
}

auto BaseTextFind::setMultiTextCursorProvider(const cursor_provider &provider) const -> void
{
  d->m_cursor_provider = provider;
}

} // namespace Core
