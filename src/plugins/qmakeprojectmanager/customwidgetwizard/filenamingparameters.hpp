// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QString>
#include <QFileInfo>

namespace QmakeProjectManager {
namespace Internal {

/* Helper struct specifying how to generate file names
 * from class names according to the CppEditor settings. */

struct FileNamingParameters {
  FileNamingParameters(const QString &headerSuffixIn = QString(QLatin1Char('h')), const QString &sourceSuffixIn = QLatin1String("cpp"), bool lowerCaseIn = true) : headerSuffix(headerSuffixIn), sourceSuffix(sourceSuffixIn), lowerCase(lowerCaseIn) {}

  auto sourceFileName(const QString &className) const -> QString
  {
    auto rc = lowerCase ? className.toLower() : className;
    rc += QLatin1Char('.');
    rc += sourceSuffix;
    return rc;
  }

  auto headerFileName(const QString &className) const -> QString
  {
    auto rc = lowerCase ? className.toLower() : className;
    rc += QLatin1Char('.');
    rc += headerSuffix;
    return rc;
  }

  auto sourceToHeaderFileName(const QString &source) const -> QString
  {
    auto rc = QFileInfo(source).completeBaseName();
    rc += QLatin1Char('.');
    rc += headerSuffix;
    return rc;
  }

  auto headerToSourceFileName(const QString &header) const -> QString
  {
    auto rc = QFileInfo(header).completeBaseName();
    rc += QLatin1Char('.');
    rc += sourceSuffix;
    return rc;
  }

  QString headerSuffix;
  QString sourceSuffix;
  bool lowerCase;
};

}
}
