// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mimeglobpattern_p.hpp"

#include <QRegularExpression>
#include <QStringList>
#include <QDebug>

using namespace Utils;
using namespace Utils::Internal;

/*!
    \internal
    \class MimeGlobMatchResult
    \inmodule QtCore
    \brief The MimeGlobMatchResult class accumulates results from glob matching.

    Handles glob weights, and preferring longer matches over shorter matches.
*/

auto MimeGlobMatchResult::addMatch(const QString &mimeType, int weight, const QString &pattern) -> void
{
    // Is this a lower-weight pattern than the last match? Skip this match then.
    if (weight < m_weight)
        return;
    bool replace = weight > m_weight;
    if (!replace) {
        // Compare the length of the match
        if (pattern.length() < m_matchingPatternLength)
            return; // too short, ignore
        else if (pattern.length() > m_matchingPatternLength) {
            // longer: clear any previous match (like *.bz2, when pattern is *.tar.bz2)
            replace = true;
        }
    }
    if (replace) {
        m_matchingMimeTypes.clear();
        // remember the new "longer" length
        m_matchingPatternLength = pattern.length();
        m_weight = weight;
    }
    if (!m_matchingMimeTypes.contains(mimeType)) {
        m_matchingMimeTypes.append(mimeType);
        if (pattern.startsWith(QLatin1String("*.")))
            m_foundSuffix = pattern.mid(2);
    }
}

auto MimeGlobPattern::detectPatternType(const QString &pattern) const -> MimeGlobPattern::PatternType
{
    const int patternLength = pattern.length();
    if (!patternLength)
        return OtherPattern;

    const bool starCount = pattern.count(QLatin1Char('*')) == 1;
    const bool hasSquareBracket = pattern.indexOf(QLatin1Char('[')) != -1;
    const bool hasQuestionMark = pattern.indexOf(QLatin1Char('?')) != -1;

    if (!hasSquareBracket && !hasQuestionMark) {
        if (starCount == 1) {
            // Patterns like "*~", "*.extension"
            if (pattern.at(0) == QLatin1Char('*'))
                return SuffixPattern;
            // Patterns like "README*" (well this is currently the only one like that...)
            if (pattern.at(patternLength - 1) == QLatin1Char('*'))
                return PrefixPattern;
        }
        // Names without any wildcards like "README"
        if (starCount == 0)
            return LiteralPattern;
    }

    if (pattern == QLatin1String("[0-9][0-9][0-9].vdr"))
        return VdrPattern;

    if (pattern == QLatin1String("*.anim[1-9j]"))
        return AnimPattern;

    return OtherPattern;
}

/*!
    \internal
    \class MimeGlobPattern
    \inmodule QtCore
    \brief The MimeGlobPattern class contains the glob pattern for file names for MIME type matching.

    \sa MimeType, MimeDatabase, MimeMagicRuleMatcher, MimeMagicRule
*/

auto MimeGlobPattern::matchFileName(const QString &inputFileName) const -> bool
{
    // "Applications MUST match globs case-insensitively, except when the case-sensitive
    // attribute is set to true."
    // The constructor takes care of putting case-insensitive patterns in lowercase.
    const QString fileName = m_caseSensitivity == Qt::CaseInsensitive
            ? inputFileName.toLower() : inputFileName;

    const int patternLength = m_pattern.length();
    if (!patternLength)
        return false;
    const int fileNameLength = fileName.length();

    switch (m_patternType) {
    case SuffixPattern: {
        if (fileNameLength + 1 < patternLength)
            return false;

        const QChar *c1 = m_pattern.unicode() + patternLength - 1;
        const QChar *c2 = fileName.unicode() + fileNameLength - 1;
        int cnt = 1;
        while (cnt < patternLength && *c1-- == *c2--)
            ++cnt;
        return cnt == patternLength;
    }
    case PrefixPattern: {
        if (fileNameLength + 1 < patternLength)
            return false;

        const QChar *c1 = m_pattern.unicode();
        const QChar *c2 = fileName.unicode();
        int cnt = 1;
        while (cnt < patternLength && *c1++ == *c2++)
           ++cnt;
        return cnt == patternLength;
    }
    case LiteralPattern:
        return (m_pattern == fileName);
    case VdrPattern: // "[0-9][0-9][0-9].vdr" case
        return fileNameLength == 7
                && fileName.at(0).isDigit() && fileName.at(1).isDigit() && fileName.at(2).isDigit()
                && QStringView{fileName}.mid(3, 4) == QLatin1String(".vdr");
    case AnimPattern: { // "*.anim[1-9j]" case
        if (fileNameLength < 6)
            return false;
        const QChar lastChar = fileName.at(fileNameLength - 1);
        const bool lastCharOK = (lastChar.isDigit() && lastChar != QLatin1Char('0'))
                              || lastChar == QLatin1Char('j');
        return lastCharOK && QStringView{fileName}.mid(fileNameLength - 6, 5) == QLatin1String(".anim");
    }
    case OtherPattern:
        // Other fallback patterns: slow but correct method
        const QRegularExpression rx(QRegularExpression::anchoredPattern(
                                    QRegularExpression::wildcardToRegularExpression(m_pattern)));
        return rx.match(fileName).hasMatch();
    }
    return false;
}

static auto isFastPattern(const QString &pattern) -> bool
{
   // starts with "*.", has no other '*' and no other '.'
   return pattern.lastIndexOf(QLatin1Char('*')) == 0
      && pattern.lastIndexOf(QLatin1Char('.')) == 1
      // and contains no other special character
      && !pattern.contains(QLatin1Char('?'))
      && !pattern.contains(QLatin1Char('['))
      ;
}

auto MimeAllGlobPatterns::addGlob(const MimeGlobPattern &glob) -> void
{
    const QString &pattern = glob.pattern();
    Q_ASSERT(!pattern.isEmpty());

    // Store each patterns into either m_fastPatternDict (*.txt, *.html etc. with default weight 50)
    // or for the rest, like core.*, *.tar.bz2, *~, into highWeightPatternOffset (>50)
    // or lowWeightPatternOffset (<=50)

    if (glob.weight() == 50 && isFastPattern(pattern) && !glob.isCaseSensitive()) {
        // The bulk of the patterns is *.foo with weight 50 --> those go into the fast patterns hash.
        const QString extension = pattern.mid(2).toLower();
        QStringList &patterns = m_fastPatterns[extension]; // find or create
        if (!patterns.contains(glob.mimeType()))
            patterns.append(glob.mimeType());
    } else {
        if (glob.weight() > 50) {
            if (!m_highWeightGlobs.hasPattern(glob.mimeType(), glob.pattern()))
                m_highWeightGlobs.append(glob);
        } else {
            if (!m_lowWeightGlobs.hasPattern(glob.mimeType(), glob.pattern()))
                m_lowWeightGlobs.append(glob);
        }
    }
}

auto MimeAllGlobPatterns::removeMimeType(const QString &mimeType) -> void
{
    for (QStringList &x : m_fastPatterns)
        x.removeAll(mimeType);

    m_highWeightGlobs.removeMimeType(mimeType);
    m_lowWeightGlobs.removeMimeType(mimeType);
}

auto MimeGlobPatternList::match(MimeGlobMatchResult &result, const QString &fileName) const -> void
{
    for (const MimeGlobPattern &glob : *this) {
        if (glob.matchFileName(fileName))
            result.addMatch(glob.mimeType(), glob.weight(), glob.pattern());
    }
}

auto MimeAllGlobPatterns::matchingGlobs(const QString &fileName, QString *foundSuffix) const -> QStringList
{
    // First try the high weight matches (>50), if any.
    MimeGlobMatchResult result;
    m_highWeightGlobs.match(result, fileName);
    if (result.m_matchingMimeTypes.isEmpty()) {

        // Now use the "fast patterns" dict, for simple *.foo patterns with weight 50
        // (which is most of them, so this optimization is definitely worth it)
        const int lastDot = fileName.lastIndexOf(QLatin1Char('.'));
        if (lastDot != -1) { // if no '.', skip the extension lookup
            const int ext_len = fileName.length() - lastDot - 1;
            const QString simpleExtension = fileName.right(ext_len).toLower();
            // (toLower because fast patterns are always case-insensitive and saved as lowercase)

            const QStringList matchingMimeTypes = m_fastPatterns.value(simpleExtension);
            for (const QString &mime : matchingMimeTypes)
                result.addMatch(mime, 50, QLatin1String("*.") + simpleExtension);
            // Can't return yet; *.tar.bz2 has to win over *.bz2, so we need the low-weight mimetypes anyway,
            // at least those with weight 50.
        }

        // Finally, try the low weight matches (<=50)
        m_lowWeightGlobs.match(result, fileName);
    }
    if (foundSuffix)
        *foundSuffix = result.m_foundSuffix;
    return result.m_matchingMimeTypes;
}

auto MimeAllGlobPatterns::clear() -> void
{
    m_fastPatterns.clear();
    m_highWeightGlobs.clear();
    m_lowWeightGlobs.clear();
}
