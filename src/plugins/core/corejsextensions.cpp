// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "corejsextensions.hpp"

#include <app/app_version.hpp>

#include <utils/fileutils.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/qtcassert.hpp>

#include <QDir>
#include <QTemporaryFile>

namespace Core {
namespace Internal {

auto UtilsJsExtension::qtVersion() const -> QString
{
  return QLatin1String(qVersion());
}

auto UtilsJsExtension::orcaVersion() const -> QString
{
  return QLatin1String(Constants::IDE_VERSION_DISPLAY);
}

auto UtilsJsExtension::toNativeSeparators(const QString &in) const -> QString
{
  return QDir::toNativeSeparators(in);
}

auto UtilsJsExtension::fromNativeSeparators(const QString &in) const -> QString
{
  return QDir::fromNativeSeparators(in);
}

auto UtilsJsExtension::baseName(const QString &in) const -> QString
{
  QFileInfo fi(in);
  return fi.baseName();
}

auto UtilsJsExtension::fileName(const QString &in) const -> QString
{
  QFileInfo fi(in);
  return fi.fileName();
}

auto UtilsJsExtension::completeBaseName(const QString &in) const -> QString
{
  QFileInfo fi(in);
  return fi.completeBaseName();
}

auto UtilsJsExtension::suffix(const QString &in) const -> QString
{
  QFileInfo fi(in);
  return fi.suffix();
}

auto UtilsJsExtension::completeSuffix(const QString &in) const -> QString
{
  QFileInfo fi(in);
  return fi.completeSuffix();
}

auto UtilsJsExtension::path(const QString &in) const -> QString
{
  QFileInfo fi(in);
  return fi.path();
}

auto UtilsJsExtension::absoluteFilePath(const QString &in) const -> QString
{
  QFileInfo fi(in);
  return fi.absoluteFilePath();
}

auto UtilsJsExtension::relativeFilePath(const QString &path, const QString &base) const -> QString
{
  return QDir(base).relativeFilePath(path);
}

auto UtilsJsExtension::exists(const QString &in) const -> bool
{
  return QFileInfo::exists(in);
}

auto UtilsJsExtension::isDirectory(const QString &in) const -> bool
{
  return QFileInfo(in).isDir();
}

auto UtilsJsExtension::isFile(const QString &in) const -> bool
{
  return QFileInfo(in).isFile();
}

auto UtilsJsExtension::preferredSuffix(const QString &mimetype) const -> QString
{
  auto mt = Utils::mimeTypeForName(mimetype);
  if (mt.isValid())
    return mt.preferredSuffix();
  return {};
}

auto UtilsJsExtension::fileName(const QString &path, const QString &extension) const -> QString
{
  return Utils::FilePath::fromStringWithExtension(path, extension).toString();
}

auto UtilsJsExtension::mktemp(const QString &pattern) const -> QString
{
  auto tmp = pattern;

  if (tmp.isEmpty())
    tmp = QStringLiteral("qt_temp.XXXXXX");

  QFileInfo fi(tmp);

  if (!fi.isAbsolute()) {
    auto tempPattern = QDir::tempPath();
    if (!tempPattern.endsWith(QLatin1Char('/')))
      tempPattern += QLatin1Char('/');
    tmp = tempPattern + tmp;
  }

  QTemporaryFile file(tmp);
  file.setAutoRemove(false);
  QTC_ASSERT(file.open(), return QString());
  file.close();
  return file.fileName();
}

auto UtilsJsExtension::asciify(const QString &input) const -> QString
{
  QString result;

  for (const auto &c : input) {
    if (c.isPrint() && c.unicode() < 128)
      result.append(c);
    else
      result.append(QString::fromLatin1("u%1").arg(c.unicode(), 4, 16, QChar('0')));
  }

  return result;
}

} // namespace Internal
} // namespace Core
