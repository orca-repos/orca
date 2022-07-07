// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "changeset.hpp"

#include <QTextCursor>

namespace Utils {

ChangeSet::ChangeSet() : m_string(nullptr), m_cursor(nullptr), m_error(false) {}

ChangeSet::ChangeSet(const QList<EditOp> &operations) : m_string(nullptr), m_cursor(nullptr), m_operationList(operations), m_error(false) {}

static auto overlaps(int posA, int lengthA, int posB, int lengthB) -> bool
{
  if (lengthB > 0) {
    return // right edge of B contained in A
      (posA < posB + lengthB && posA + lengthA >= posB + lengthB)
      // left edge of B contained in A
      || (posA <= posB && posA + lengthA > posB)
      // A contained in B
      || (posB < posA && posB + lengthB > posA + lengthA);
  } else {
    return (posB > posA && posB < posA + lengthA);
  }
}

auto ChangeSet::hasOverlap(int pos, int length) const -> bool
{
  for (const EditOp &cmd : m_operationList) {

    switch (cmd.type) {
    case EditOp::Replace:
      if (overlaps(pos, length, cmd.pos1, cmd.length1))
        return true;
      break;

    case EditOp::Move:
      if (overlaps(pos, length, cmd.pos1, cmd.length1))
        return true;
      if (cmd.pos2 > pos && cmd.pos2 < pos + length)
        return true;
      break;

    case EditOp::Insert:
      if (cmd.pos1 > pos && cmd.pos1 < pos + length)
        return true;
      break;

    case EditOp::Remove:
      if (overlaps(pos, length, cmd.pos1, cmd.length1))
        return true;
      break;

    case EditOp::Flip:
      if (overlaps(pos, length, cmd.pos1, cmd.length1))
        return true;
      if (overlaps(pos, length, cmd.pos2, cmd.length2))
        return true;
      break;

    case EditOp::Copy:
      if (overlaps(pos, length, cmd.pos1, cmd.length1))
        return true;
      if (cmd.pos2 > pos && cmd.pos2 < pos + length)
        return true;
      break;

    case EditOp::Unset:
      break;
    }
  }

  return false;
}

auto ChangeSet::isEmpty() const -> bool
{
  return m_operationList.isEmpty();
}

auto ChangeSet::operationList() const -> QList<ChangeSet::EditOp>
{
  return m_operationList;
}

auto ChangeSet::clear() -> void
{
  m_string = nullptr;
  m_cursor = nullptr;
  m_operationList.clear();
  m_error = false;
}

auto ChangeSet::replace_helper(int pos, int length, const QString &replacement) -> bool
{
  if (hasOverlap(pos, length))
    m_error = true;

  EditOp cmd(EditOp::Replace);
  cmd.pos1 = pos;
  cmd.length1 = length;
  cmd.text = replacement;
  m_operationList += cmd;

  return !m_error;
}

auto ChangeSet::move_helper(int pos, int length, int to) -> bool
{
  if (hasOverlap(pos, length) || hasOverlap(to, 0) || overlaps(pos, length, to, 0)) {
    m_error = true;
  }

  EditOp cmd(EditOp::Move);
  cmd.pos1 = pos;
  cmd.length1 = length;
  cmd.pos2 = to;
  m_operationList += cmd;

  return !m_error;
}

auto ChangeSet::insert(int pos, const QString &text) -> bool
{
  Q_ASSERT(pos >= 0);

  if (hasOverlap(pos, 0))
    m_error = true;

  EditOp cmd(EditOp::Insert);
  cmd.pos1 = pos;
  cmd.text = text;
  m_operationList += cmd;

  return !m_error;
}

auto ChangeSet::replace(const Range &range, const QString &replacement) -> bool { return replace(range.start, range.end, replacement); }
auto ChangeSet::remove(const Range &range) -> bool { return remove(range.start, range.end); }
auto ChangeSet::move(const Range &range, int to) -> bool { return move(range.start, range.end, to); }
auto ChangeSet::flip(const Range &range1, const Range &range2) -> bool { return flip(range1.start, range1.end, range2.start, range2.end); }
auto ChangeSet::copy(const Range &range, int to) -> bool { return copy(range.start, range.end, to); }
auto ChangeSet::replace(int start, int end, const QString &replacement) -> bool { return replace_helper(start, end - start, replacement); }
auto ChangeSet::remove(int start, int end) -> bool { return remove_helper(start, end - start); }
auto ChangeSet::move(int start, int end, int to) -> bool { return move_helper(start, end - start, to); }
auto ChangeSet::flip(int start1, int end1, int start2, int end2) -> bool { return flip_helper(start1, end1 - start1, start2, end2 - start2); }
auto ChangeSet::copy(int start, int end, int to) -> bool { return copy_helper(start, end - start, to); }

auto ChangeSet::remove_helper(int pos, int length) -> bool
{
  if (hasOverlap(pos, length))
    m_error = true;

  EditOp cmd(EditOp::Remove);
  cmd.pos1 = pos;
  cmd.length1 = length;
  m_operationList += cmd;

  return !m_error;
}

auto ChangeSet::flip_helper(int pos1, int length1, int pos2, int length2) -> bool
{
  if (hasOverlap(pos1, length1) || hasOverlap(pos2, length2) || overlaps(pos1, length1, pos2, length2)) {
    m_error = true;
  }

  EditOp cmd(EditOp::Flip);
  cmd.pos1 = pos1;
  cmd.length1 = length1;
  cmd.pos2 = pos2;
  cmd.length2 = length2;
  m_operationList += cmd;

  return !m_error;
}

auto ChangeSet::copy_helper(int pos, int length, int to) -> bool
{
  if (hasOverlap(pos, length) || hasOverlap(to, 0) || overlaps(pos, length, to, 0)) {
    m_error = true;
  }

  EditOp cmd(EditOp::Copy);
  cmd.pos1 = pos;
  cmd.length1 = length;
  cmd.pos2 = to;
  m_operationList += cmd;

  return !m_error;
}

auto ChangeSet::doReplace(const EditOp &op, QList<EditOp> *replaceList) -> void
{
  Q_ASSERT(op.type == EditOp::Replace);

  {
    for (EditOp &c : *replaceList) {
      if (op.pos1 <= c.pos1)
        c.pos1 += op.text.size();
      if (op.pos1 < c.pos1)
        c.pos1 -= op.length1;
    }
  }

  if (m_string) {
    m_string->replace(op.pos1, op.length1, op.text);
  } else if (m_cursor) {
    m_cursor->setPosition(op.pos1);
    m_cursor->setPosition(op.pos1 + op.length1, QTextCursor::KeepAnchor);
    m_cursor->insertText(op.text);
  }
}

auto ChangeSet::convertToReplace(const EditOp &op, QList<EditOp> *replaceList) -> void
{
  EditOp replace1(EditOp::Replace);
  EditOp replace2(EditOp::Replace);

  switch (op.type) {
  case EditOp::Replace:
    replaceList->append(op);
    break;

  case EditOp::Move:
    replace1.pos1 = op.pos1;
    replace1.length1 = op.length1;
    replaceList->append(replace1);

    replace2.pos1 = op.pos2;
    replace2.text = textAt(op.pos1, op.length1);
    replaceList->append(replace2);
    break;

  case EditOp::Insert:
    replace1.pos1 = op.pos1;
    replace1.text = op.text;
    replaceList->append(replace1);
    break;

  case EditOp::Remove:
    replace1.pos1 = op.pos1;
    replace1.length1 = op.length1;
    replaceList->append(replace1);
    break;

  case EditOp::Flip:
    replace1.pos1 = op.pos1;
    replace1.length1 = op.length1;
    replace1.text = textAt(op.pos2, op.length2);
    replaceList->append(replace1);

    replace2.pos1 = op.pos2;
    replace2.length1 = op.length2;
    replace2.text = textAt(op.pos1, op.length1);
    replaceList->append(replace2);
    break;

  case EditOp::Copy:
    replace1.pos1 = op.pos2;
    replace1.text = textAt(op.pos1, op.length1);
    replaceList->append(replace1);
    break;

  case EditOp::Unset:
    break;
  }
}

auto ChangeSet::hadErrors() const -> bool
{
  return m_error;
}

auto ChangeSet::apply(QString *s) -> void
{
  m_string = s;
  apply_helper();
  m_string = nullptr;
}

auto ChangeSet::apply(QTextCursor *textCursor) -> void
{
  m_cursor = textCursor;
  apply_helper();
  m_cursor = nullptr;
}

auto ChangeSet::textAt(int pos, int length) -> QString
{
  if (m_string) {
    return m_string->mid(pos, length);
  } else if (m_cursor) {
    m_cursor->setPosition(pos);
    m_cursor->setPosition(pos + length, QTextCursor::KeepAnchor);
    return m_cursor->selectedText();
  }
  return QString();
}

auto ChangeSet::apply_helper() -> void
{
  // convert all ops to replace
  QList<EditOp> replaceList;
  {
    while (!m_operationList.isEmpty())
      convertToReplace(m_operationList.takeFirst(), &replaceList);
  }

  // execute replaces
  if (m_cursor)
    m_cursor->beginEditBlock();

  while (!replaceList.isEmpty())
    doReplace(replaceList.takeFirst(), &replaceList);

  if (m_cursor)
    m_cursor->endEditBlock();
}

} // end namespace Utils
