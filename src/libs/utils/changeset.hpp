// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QString>
#include <QList>

QT_FORWARD_DECLARE_CLASS(QTextCursor)

namespace Utils {

class ORCA_UTILS_EXPORT ChangeSet {
public:
  struct EditOp {
    enum Type {
      Unset,
      Replace,
      Move,
      Insert,
      Remove,
      Flip,
      Copy
    };

    EditOp() = default;
    EditOp(Type t): type(t) {}

    Type type = Unset;
    int pos1 = 0;
    int pos2 = 0;
    int length1 = 0;
    int length2 = 0;
    QString text;
  };

  struct Range {
    Range() = default;
    Range(int start, int end) : start(start), end(end) {}

    int start = 0;
    int end = 0;
  };

public:
  ChangeSet();
  ChangeSet(const QList<EditOp> &operations);

  auto isEmpty() const -> bool;
  auto operationList() const -> QList<EditOp>;
  auto clear() -> void;
  auto replace(const Range &range, const QString &replacement) -> bool;
  auto remove(const Range &range) -> bool;
  auto move(const Range &range, int to) -> bool;
  auto flip(const Range &range1, const Range &range2) -> bool;
  auto copy(const Range &range, int to) -> bool;
  auto replace(int start, int end, const QString &replacement) -> bool;
  auto remove(int start, int end) -> bool;
  auto move(int start, int end, int to) -> bool;
  auto flip(int start1, int end1, int start2, int end2) -> bool;
  auto copy(int start, int end, int to) -> bool;
  auto insert(int pos, const QString &text) -> bool;
  auto hadErrors() const -> bool;
  auto apply(QString *s) -> void;
  auto apply(QTextCursor *textCursor) -> void;

private:
  // length-based API.
  auto replace_helper(int pos, int length, const QString &replacement) -> bool;
  auto move_helper(int pos, int length, int to) -> bool;
  auto remove_helper(int pos, int length) -> bool;
  auto flip_helper(int pos1, int length1, int pos2, int length2) -> bool;
  auto copy_helper(int pos, int length, int to) -> bool;
  auto hasOverlap(int pos, int length) const -> bool;
  auto textAt(int pos, int length) -> QString;
  auto doReplace(const EditOp &replace, QList<EditOp> *replaceList) -> void;
  auto convertToReplace(const EditOp &op, QList<EditOp> *replaceList) -> void;

  auto apply_helper() -> void;

private:
  QString *m_string;
  QTextCursor *m_cursor;
  QList<EditOp> m_operationList;
  bool m_error;
};

inline auto operator<(const ChangeSet::Range &r1, const ChangeSet::Range &r2) -> bool
{
  return r1.start < r2.start;
}

} // namespace Utils
