// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "codegeneration.hpp"

#include "algorithm.hpp"

#include <QTextStream>
#include <QSet>
#include <QStringList>
#include <QFileInfo>

namespace Utils {

ORCA_UTILS_EXPORT auto fileNameToCppIdentifier(const QString &s) -> QString
{
  QString rc;
  const int len = s.size();
  const QChar underscore = QLatin1Char('_');
  const QChar dot = QLatin1Char('.');

  for (int i = 0; i < len; i++) {
    const QChar c = s.at(i);
    if (c == underscore || c.isLetterOrNumber())
      rc += c;
    else if (c == dot)
      rc += underscore;
  }
  return rc;
}

ORCA_UTILS_EXPORT auto headerGuard(const QString &file) -> QString
{
  return headerGuard(file, QStringList());
}

ORCA_UTILS_EXPORT auto headerGuard(const QString &file, const QStringList &namespaceList) -> QString
{
  const QChar underscore = QLatin1Char('_');
  QString rc;
  for (int i = 0; i < namespaceList.count(); i++)
    rc += namespaceList.at(i).toUpper() + underscore;

  const QFileInfo fi(file);
  rc += fileNameToCppIdentifier(fi.fileName()).toUpper();
  return rc;
}

ORCA_UTILS_EXPORT auto writeIncludeFileDirective(const QString &file, bool globalInclude, QTextStream &str) -> void
{
  const QChar opening = globalInclude ? QLatin1Char('<') : QLatin1Char('"');
  const QChar closing = globalInclude ? QLatin1Char('>') : QLatin1Char('"');
  str << QLatin1String("#include ") << opening << file << closing << QLatin1Char('\n');
}

ORCA_UTILS_EXPORT auto writeBeginQtVersionCheck(QTextStream &str) -> void
{
  str << QLatin1String("#if QT_VERSION >= 0x050000\n");
}

static auto qtSection(const QStringList &qtIncludes, QTextStream &str) -> void
{
  QStringList sorted = qtIncludes;
  Utils::sort(sorted);
  for (const QString &inc : qAsConst(sorted)) {
    if (!inc.isEmpty())
      str << QStringLiteral("#include <%1>\n").arg(inc);
  }
}

ORCA_UTILS_EXPORT auto writeQtIncludeSection(const QStringList &qt4, const QStringList &qt5, bool addQtVersionCheck, bool includeQtModule, QTextStream &str) -> void
{
  std::function<QString(const QString &)> trans;
  if (includeQtModule)
    trans = [](const QString &i) { return i; };
  else
    trans = [](const QString &i) { return i.mid(i.indexOf(QLatin1Char('/')) + 1); };

  QSet<QString> qt4Only = Utils::transform<QSet>(qt4, trans);
  QSet<QString> qt5Only = Utils::transform<QSet>(qt5, trans);

  if (addQtVersionCheck) {
    QSet<QString> common = qt4Only;
    common.intersect(qt5Only);

    // qglobal.h is needed for QT_VERSION
    if (includeQtModule)
      common.insert(QLatin1String("QtCore/qglobal.hpp"));
    else
      common.insert(QLatin1String("qglobal.hpp"));

    qt4Only.subtract(common);
    qt5Only.subtract(common);

    qtSection(Utils::toList(common), str);

    if (!qt4Only.isEmpty() || !qt5Only.isEmpty()) {
      if (addQtVersionCheck)
        writeBeginQtVersionCheck(str);
      qtSection(Utils::toList(qt5Only), str);
      if (addQtVersionCheck)
        str << QLatin1String("#else\n");
      qtSection(Utils::toList(qt4Only), str);
      if (addQtVersionCheck)
        str << QLatin1String("#endif\n");
    }
  } else {
    if (!qt5Only.isEmpty()) // default to Qt5
      qtSection(Utils::toList(qt5Only), str);
    else
      qtSection(Utils::toList(qt4Only), str);
  }
}

ORCA_UTILS_EXPORT auto writeOpeningNameSpaces(const QStringList &l, const QString &indent, QTextStream &str) -> QString
{
  const int count = l.size();
  QString rc;
  if (count) {
    str << '\n';
    for (int i = 0; i < count; i++) {
      str << rc << "namespace " << l.at(i) << " {\n";
      rc += indent;
    }
  }
  return rc;
}

ORCA_UTILS_EXPORT auto writeClosingNameSpaces(const QStringList &l, const QString &indent, QTextStream &str) -> void
{
  if (!l.empty())
    str << '\n';
  for (int i = l.size() - 1; i >= 0; i--) {
    if (i)
      str << QString(indent.size() * i, QLatin1Char(' '));
    str << "} // namespace " << l.at(i) << '\n';
  }
}

} // namespace Utils
