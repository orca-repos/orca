// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qstringlist.h>
#include <QtCore/qhash.h>

namespace Utils {
namespace Internal {

struct MimeGlobMatchResult {
  auto addMatch(const QString &mimeType, int weight, const QString &pattern) -> void;

  QStringList m_matchingMimeTypes;
  int m_weight = 0;
  int m_matchingPatternLength = 0;
  QString m_foundSuffix;
};

class MimeGlobPattern {
public:
  static const unsigned MaxWeight = 100;
  static const unsigned DefaultWeight = 50;
  static const unsigned MinWeight = 1;

  explicit MimeGlobPattern(const QString &thePattern, const QString &theMimeType, unsigned theWeight = DefaultWeight, Qt::CaseSensitivity s = Qt::CaseInsensitive) : m_pattern(s == Qt::CaseInsensitive ? thePattern.toLower() : thePattern), m_mimeType(theMimeType), m_weight(theWeight), m_caseSensitivity(s), m_patternType(detectPatternType(m_pattern)) { }

  auto swap(MimeGlobPattern &other) noexcept -> void
  {
    qSwap(m_pattern, other.m_pattern);
    qSwap(m_mimeType, other.m_mimeType);
    qSwap(m_weight, other.m_weight);
    qSwap(m_caseSensitivity, other.m_caseSensitivity);
    qSwap(m_patternType, other.m_patternType);
  }

  auto matchFileName(const QString &inputFileName) const -> bool;

  auto pattern() const -> const QString& { return m_pattern; }
  auto weight() const -> unsigned { return m_weight; }
  auto mimeType() const -> const QString& { return m_mimeType; }
  auto isCaseSensitive() const -> bool { return m_caseSensitivity == Qt::CaseSensitive; }

private:
  enum PatternType {
    SuffixPattern,
    PrefixPattern,
    LiteralPattern,
    VdrPattern,
    // special handling for "[0-9][0-9][0-9].vdr" pattern
    AnimPattern,
    // special handling for "*.anim[1-9j]" pattern
    OtherPattern
  };

  auto detectPatternType(const QString &pattern) const -> PatternType;

  QString m_pattern;
  QString m_mimeType;
  int m_weight;
  Qt::CaseSensitivity m_caseSensitivity;
  PatternType m_patternType;
};

class MimeGlobPatternList : public QList<MimeGlobPattern> {
public:
  auto hasPattern(const QString &mimeType, const QString &pattern) const -> bool
  {
    const_iterator it = begin();
    const const_iterator myend = end();
    for (; it != myend; ++it)
      if ((*it).pattern() == pattern && (*it).mimeType() == mimeType)
        return true;
    return false;
  }

  /*!
      "noglobs" is very rare occurrence, so it's ok if it's slow
   */
  auto removeMimeType(const QString &mimeType) -> void
  {
    auto isMimeTypeEqual = [&mimeType](const MimeGlobPattern &pattern) {
      return pattern.mimeType() == mimeType;
    };
    erase(std::remove_if(begin(), end(), isMimeTypeEqual), end());
  }

  auto match(MimeGlobMatchResult &result, const QString &fileName) const -> void;
};

/*!
    Result of the globs parsing, as data structures ready for efficient MIME type matching.
    This contains:
    1) a map of fast regular patterns (e.g. *.txt is stored as "txt" in a qhash's key)
    2) a linear list of high-weight globs
    3) a linear list of low-weight globs
 */
class MimeAllGlobPatterns {
public:
  typedef QHash<QString, QStringList> PatternsMap; // MIME type -> patterns

  auto addGlob(const MimeGlobPattern &glob) -> void;
  auto removeMimeType(const QString &mimeType) -> void;
  auto matchingGlobs(const QString &fileName, QString *foundSuffix) const -> QStringList;
  auto clear() -> void;

  PatternsMap m_fastPatterns; // example: "doc" -> "application/msword", "text/plain"
  MimeGlobPatternList m_highWeightGlobs;
  MimeGlobPatternList m_lowWeightGlobs; // <= 50, including the non-fast 50 patterns
};

} // Internal
} // Utils
