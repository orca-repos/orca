// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "porting.h"

#include <QList>
#include <QString>

#include <functional>

QT_BEGIN_NAMESPACE
class QJsonValue;
QT_END_NAMESPACE

namespace Utils {

// Create a usable settings key from a category,
// for example Editor|C++ -> Editor_C__
ORCA_UTILS_EXPORT auto settingsKey(const QString &category) -> QString;
// Return the common prefix part of a string list:
// "C:\foo\bar1" "C:\foo\bar2"  -> "C:\foo\bar"
ORCA_UTILS_EXPORT auto commonPrefix(const QStringList &strings) -> QString;
// Return the common path of a list of files:
// "C:\foo\bar1" "C:\foo\bar2"  -> "C:\foo"
ORCA_UTILS_EXPORT auto commonPath(const QStringList &files) -> QString;
// On Linux/Mac replace user's home path with ~
// Uses cleaned path and tries to use absolute path of "path" if possible
// If path is not sub of home path, or when running on Windows, returns the input
ORCA_UTILS_EXPORT auto withTildeHomePath(const QString &path) -> QString;
// Removes first unescaped ampersand in text
ORCA_UTILS_EXPORT auto stripAccelerator(const QString &text) -> QString;
// Quotes all ampersands
ORCA_UTILS_EXPORT auto quoteAmpersands(const QString &text) -> QString;
ORCA_UTILS_EXPORT auto readMultiLineString(const QJsonValue &value, QString *out) -> bool;

// Compare case insensitive and use case sensitive comparison in case of that being equal.
ORCA_UTILS_EXPORT auto caseFriendlyCompare(const QString &a, const QString &b) -> int;

class ORCA_UTILS_EXPORT AbstractMacroExpander
{
public:
    virtual ~AbstractMacroExpander() {}
    // Not const, as it may change the state of the expander.
    //! Find an expando to replace and provide a replacement string.
    //! \param str The string to scan
    //! \param pos Position to start scan on input, found position on output
    //! \param ret Replacement string on output
    //! \return Length of string part to replace, zero if no (further) matches found
    virtual auto findMacro(const QString &str, int *pos, QString *ret) -> int;
    //! Provide a replacement string for an expando
    //! \param name The name of the expando
    //! \param ret Replacement string on output
    //! \return True if the expando was found
    virtual auto resolveMacro(const QString &name, QString *ret, QSet<AbstractMacroExpander*> &seen) -> bool = 0;
private:
    auto expandNestedMacros(const QString &str, int *pos, QString *ret) -> bool;
};

ORCA_UTILS_EXPORT auto expandMacros(QString *str, AbstractMacroExpander *mx) -> void;
ORCA_UTILS_EXPORT auto expandMacros(const QString &str, AbstractMacroExpander *mx) -> QString;
ORCA_UTILS_EXPORT auto parseUsedPortFromNetstatOutput(const QByteArray &line) -> int;

template <typename T>
auto makeUniquelyNumbered(const T &preferred, const std::function<bool(const T &)> &isOk) -> T
{
  if (isOk(preferred))
    return preferred;
  int i = 2;
  T tryName = preferred + QString::number(i);
  while (!isOk(tryName))
    tryName = preferred + QString::number(++i);
  return tryName;
}

template <typename T, typename Container>
auto makeUniquelyNumbered(const T &preferred, const Container &reserved) -> T
{
  const std::function<bool(const T &)> isOk = [&reserved](const T &v) { return !reserved.contains(v); };
  return makeUniquelyNumbered(preferred, isOk);
}

ORCA_UTILS_EXPORT auto formatElapsedTime(qint64 elapsed) -> QString;
/* This function is only necessary if you need to match the wildcard expression against a
 * string that might contain path separators - otherwise
 * QRegularExpression::wildcardToRegularExpression() can be used.
 * Working around QRegularExpression::wildcardToRegularExpression() taking native separators
 * into account and handling them to disallow matching a wildcard characters.
 */
ORCA_UTILS_EXPORT auto wildcardToRegularExpression(const QString &original) -> QString;
ORCA_UTILS_EXPORT auto languageNameFromLanguageCode(const QString &languageCode) -> QString;

} // namespace Utils
