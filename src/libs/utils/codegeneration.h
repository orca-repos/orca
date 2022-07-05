// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QStringList>

QT_BEGIN_NAMESPACE
class QTextStream;
QT_END_NAMESPACE

namespace Utils {

// Convert a file name to a Cpp identifier (stripping invalid characters
// or replacing them by an underscore).
ORCA_UTILS_EXPORT auto fileNameToCppIdentifier(const QString &s) -> QString;
ORCA_UTILS_EXPORT auto headerGuard(const QString &file) -> QString;
ORCA_UTILS_EXPORT auto headerGuard(const QString &file, const QStringList &namespaceList) -> QString;
ORCA_UTILS_EXPORT auto writeIncludeFileDirective(const QString &file, bool globalInclude, QTextStream &str) -> void;
ORCA_UTILS_EXPORT auto writeBeginQtVersionCheck(QTextStream &str) -> void;
ORCA_UTILS_EXPORT auto writeQtIncludeSection(const QStringList &qt4, const QStringList &qt5, bool addQtVersionCheck, bool includeQtModule, QTextStream &str) -> void;

// Write opening namespaces and return an indentation string to be used
// in the following code if there are any.
ORCA_UTILS_EXPORT auto writeOpeningNameSpaces(const QStringList &namespaces, const QString &indent, QTextStream &str) -> QString;

// Close namespacesnamespaces
ORCA_UTILS_EXPORT auto writeClosingNameSpaces(const QStringList &namespaces, const QString &indent, QTextStream &str) -> void;

} // namespace Utils
