// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QTextCursor>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Utils {

class MultiTextCursor;

class ORCA_UTILS_EXPORT CamelCaseCursor {
public:
  static auto left(QTextCursor *cursor, QPlainTextEdit *edit, QTextCursor::MoveMode mode) -> bool;
  static auto left(MultiTextCursor *cursor, QPlainTextEdit *edit, QTextCursor::MoveMode mode) -> bool;
  static auto left(QLineEdit *edit, QTextCursor::MoveMode mode) -> bool;
  static auto right(QTextCursor *cursor, QPlainTextEdit *edit, QTextCursor::MoveMode mode) -> bool;
  static auto right(MultiTextCursor *cursor, QPlainTextEdit *edit, QTextCursor::MoveMode mode) -> bool;
  static auto right(QLineEdit *edit, QTextCursor::MoveMode mode) -> bool;
};

} // namespace Utils
