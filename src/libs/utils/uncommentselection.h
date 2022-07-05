// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QString>
#include <QTextCursor>

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
QT_END_NAMESPACE

namespace Utils {

class MultiTextCursor;

class ORCA_UTILS_EXPORT CommentDefinition {
public:
  static CommentDefinition CppStyle;
  static CommentDefinition HashStyle;

  CommentDefinition();
  CommentDefinition(const QString &single, const QString &multiStart = QString(), const QString &multiEnd = QString());

  auto isValid() const -> bool;
  auto hasSingleLineStyle() const -> bool;
  auto hasMultiLineStyle() const -> bool;

  bool isAfterWhiteSpaces = false;
  QString singleLine;
  QString multiLineStart;
  QString multiLineEnd;
};

ORCA_UTILS_EXPORT auto unCommentSelection(const QTextCursor &cursor, const CommentDefinition &definiton = CommentDefinition(), bool preferSingleLine = false) -> QTextCursor;
ORCA_UTILS_EXPORT auto unCommentSelection(const MultiTextCursor &cursor, const CommentDefinition &definiton = CommentDefinition(), bool preferSingleLine = false) -> MultiTextCursor;

} // namespace Utils
