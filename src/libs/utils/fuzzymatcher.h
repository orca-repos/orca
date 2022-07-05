// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QVector>

QT_BEGIN_NAMESPACE
class QRegularExpression;
class QRegularExpressionMatch;
class QString;
QT_END_NAMESPACE

class ORCA_UTILS_EXPORT FuzzyMatcher {
public:
  enum class CaseSensitivity {
    CaseInsensitive,
    CaseSensitive,
    FirstLetterCaseSensitive
  };

  class HighlightingPositions {
  public:
    QVector<int> starts;
    QVector<int> lengths;
  };

  static auto createRegExp(const QString &pattern, CaseSensitivity caseSensitivity = CaseSensitivity::CaseInsensitive) -> QRegularExpression;
  static auto createRegExp(const QString &pattern, Qt::CaseSensitivity caseSensitivity) -> QRegularExpression;
  static auto highlightingPositions(const QRegularExpressionMatch &match) -> HighlightingPositions;
};
