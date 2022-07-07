// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qrcparser.hpp"

#include <utils/qtcassert.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QLoggingCategory>
#include <QMultiHash>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>
#include <QStringList>

static Q_LOGGING_CATEGORY(qrcParserLog, "qtc.qrcParser", QtWarningMsg)

namespace Utils {

namespace Internal {

class QrcParserPrivate {
  Q_DECLARE_TR_FUNCTIONS(QmlJS::QrcParser)

public:
  using SMap = QMap<QString, QStringList>;

  QrcParserPrivate(QrcParser *q);

  auto parseFile(const QString &path, const QString &contents) -> bool;
  auto firstFileAtPath(const QString &path, const QLocale &locale) const -> QString;
  auto collectFilesAtPath(const QString &path, QStringList *res, const QLocale *locale = nullptr) const -> void;
  auto hasDirAtPath(const QString &path, const QLocale *locale = nullptr) const -> bool;
  auto collectFilesInPath(const QString &path, QMap<QString, QStringList> *res, bool addDirs = false, const QLocale *locale = nullptr) const -> void;
  auto collectResourceFilesForSourceFile(const QString &sourceFile, QStringList *res, const QLocale *locale = nullptr) const -> void;
  auto errorMessages() const -> QStringList;
  auto languages() const -> QStringList;

private:
  static auto fixPrefix(const QString &prefix) -> QString;
  auto allUiLanguages(const QLocale *locale) const -> const QStringList;

  SMap m_resources;
  SMap m_files;
  QStringList m_languages;
  QStringList m_errorMessages;
};

class QrcCachePrivate {
  Q_DECLARE_TR_FUNCTIONS(QmlJS::QrcCachePrivate)

public:
  QrcCachePrivate(QrcCache *q);
  auto addPath(const QString &path, const QString &contents) -> QrcParser::Ptr;
  auto removePath(const QString &path) -> void;
  auto updatePath(const QString &path, const QString &contents) -> QrcParser::Ptr;
  auto parsedPath(const QString &path) -> QrcParser::Ptr;
  auto clear() -> void;

private:
  QHash<QString, QPair<QrcParser::Ptr, int>> m_cache;
  QMutex m_mutex;
};
} // namespace Internal

/*!
    \class Utils::QrcParser
    \inmodule Orca
    \brief The QrcParser class parses one or more QRC files and keeps their
    content cached.

    A \l{The Qt Resource System}{Qt resource collection (QRC)} contains files
    read from the file system but organized in a possibly different way.
    To easily describe that with a simple structure, we use a map from QRC paths
    to the paths in the filesystem.
    By using a map, we can easily find all QRC paths that start with a given
    prefix, and thus loop on a QRC directory.

    QRC files also support languages, which are mapped to a prefix of the QRC
    path. For example, the French /image/bla.png (lang=fr) will have the path
    \c {fr/image/bla.png}. The empty language represents the default resource.
    Languages are looked up using the locale uiLanguages() property

    For a single QRC, a given path maps to a single file, but when one has
    multiple (platform-specific and mutually exclusive) QRC files, multiple
    files match, so QStringList are used.

    Especially, the \c collect* functions are thought of as low level interface.
 */

/*!
    \typedef QrcParser::Ptr
    Represents pointers.
 */

/*!
    \typedef QrcParser::ConstPtr
    Represents constant pointers.
*/

/*!
    Normalizes the \a path to a file in a QRC resource by dropping the \c qrc:/
    or \c : and any extra slashes in the beginning.
 */
auto QrcParser::normalizedQrcFilePath(const QString &path) -> QString
{
  QString normPath = path;
  int endPrefix = 0;
  if (path.startsWith(QLatin1String("qrc:/")))
    endPrefix = 4;
  else if (path.startsWith(QLatin1String(":/")))
    endPrefix = 1;
  if (endPrefix < path.size() && path.at(endPrefix) == QLatin1Char('/'))
    while (endPrefix + 1 < path.size() && path.at(endPrefix + 1) == QLatin1Char('/'))
      ++endPrefix;
  normPath = path.right(path.size() - endPrefix);
  if (!normPath.startsWith(QLatin1Char('/')))
    normPath.insert(0, QLatin1Char('/'));
  return normPath;
}

/*!
    Returns the path to a directory normalized to \a path in a QRC resource by
    dropping the \c qrc:/ or \c : and any extra slashes at the beginning, and
    by ensuring that the path ends with a slash
 */
auto QrcParser::normalizedQrcDirectoryPath(const QString &path) -> QString
{
  QString normPath = normalizedQrcFilePath(path);
  if (!normPath.endsWith(QLatin1Char('/')))
    normPath.append(QLatin1Char('/'));
  return normPath;
}

/*!
    Returns the QRC directory path for \a file.
*/
auto QrcParser::qrcDirectoryPathForQrcFilePath(const QString &file) -> QString
{
  return file.left(file.lastIndexOf(QLatin1Char('/')));
}

QrcParser::QrcParser()
{
  d = new Internal::QrcParserPrivate(this);
}

/*!
    \internal
*/
QrcParser::~QrcParser()
{
  delete d;
}

/*!
    Parses the QRC file at \a path. If \a contents is not empty, it is used as
    the file contents instead of reading it from the file system.

    Returns whether the parsing succeeded.

    \sa errorMessages(), parseQrcFile()
*/
auto QrcParser::parseFile(const QString &path, const QString &contents) -> bool
{
  return d->parseFile(path, contents);
}

/*!
    Returns the file system path of the first (active) file at the given QRC
    \a path and \a locale.
 */
auto QrcParser::firstFileAtPath(const QString &path, const QLocale &locale) const -> QString
{
  return d->firstFileAtPath(path, locale);
}

/*!
    Adds the file system paths for the given QRC \a path to \a res.

    If \a locale is null, all possible files are added. Otherwise, just
    the first one that matches the locale is added.
 */
auto QrcParser::collectFilesAtPath(const QString &path, QStringList *res, const QLocale *locale) const -> void
{
  d->collectFilesAtPath(path, res, locale);
}

/*!
    Returns \c true if \a path is a non-empty directory and matches \a locale.

 */
auto QrcParser::hasDirAtPath(const QString &path, const QLocale *locale) const -> bool
{
  return d->hasDirAtPath(path, locale);
}

/*!
    Adds the directory contents of the given QRC \a path to \a res if \a addDirs
    is set to \c true.

    Adds the QRC filename to file system path associations contained in the
    given \a path to \a res. If addDirs() is \c true, directories are also
    added.

    If \a locale is null, all possible files are added. Otherwise, just the
    first file with a matching the locale is added.
 */
auto QrcParser::collectFilesInPath(const QString &path, QMap<QString, QStringList> *res, bool addDirs, const QLocale *locale) const -> void
{
  d->collectFilesInPath(path, res, addDirs, locale);
}

/*!
    Adds the resource files from the QRC file \a sourceFile to \a res.

    If \a locale is null, all possible files are added. Otherwise, just
    the first file with a matching the locale is added.
 */
auto QrcParser::collectResourceFilesForSourceFile(const QString &sourceFile, QStringList *res, const QLocale *locale) const -> void
{
  d->collectResourceFilesForSourceFile(sourceFile, res, locale);
}

/*!
    Returns the errors found while parsing.
 */
auto QrcParser::errorMessages() const -> QStringList
{
  return d->errorMessages();
}

/*!
    Returns all languages used in this QRC.
 */
auto QrcParser::languages() const -> QStringList
{
  return d->languages();
}

/*!
    Indicates whether the QRC contents are valid.

    Returns an error if the QRC is empty.
 */
auto QrcParser::isValid() const -> bool
{
  return errorMessages().isEmpty();
}

/*!
    Returns the \a contents of the QRC file at \a path.
*/
auto QrcParser::parseQrcFile(const QString &path, const QString &contents) -> QrcParser::Ptr
{
  Ptr res(new QrcParser);
  if (!path.isEmpty())
    res->parseFile(path, contents);
  return res;
}

// ----------------

/*!
    \class Utils::QrcCache
    \inmodule Orca
    \brief The QrcCache class caches the contents of parsed QRC files.

    \sa Utils::QrcParser
*/

QrcCache::QrcCache()
{
  d = new Internal::QrcCachePrivate(this);
}

/*!
    \internal
*/
QrcCache::~QrcCache()
{
  delete d;
}

/*!
    Parses the QRC file at \a path and caches the parser. If \a contents is not
    empty, it is used as the file contents instead of reading it from the file
    system.

    Returns whether the parsing succeeded.

    \sa QrcParser::errorMessages(), QrcParser::parseQrcFile()
*/
auto QrcCache::addPath(const QString &path, const QString &contents) -> QrcParser::ConstPtr
{
  return d->addPath(path, contents);
}

/*!
    Removes \a path from the cache.
*/
auto QrcCache::removePath(const QString &path) -> void
{
  d->removePath(path);
}

/*!
    Reparses the QRC file at \a path and returns the \a contents of the file.
*/
auto QrcCache::updatePath(const QString &path, const QString &contents) -> QrcParser::ConstPtr
{
  return d->updatePath(path, contents);
}

/*!
    Returns the cached QRC parser for the QRC file at \a path.
*/
auto QrcCache::parsedPath(const QString &path) -> QrcParser::ConstPtr
{
  return d->parsedPath(path);
}

/*!
    Clears the contents of the cache.
*/
auto QrcCache::clear() -> void
{
  d->clear();
}

// --------------------

namespace Internal {

QrcParserPrivate::QrcParserPrivate(QrcParser *) { }

auto QrcParserPrivate::parseFile(const QString &path, const QString &contents) -> bool
{
  QDomDocument doc;
  QDir baseDir(QFileInfo(path).path());

  if (contents.isEmpty()) {
    // Regular file
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      m_errorMessages.append(file.errorString());
      return false;
    }

    QString error_msg;
    int error_line, error_col;
    if (!doc.setContent(&file, &error_msg, &error_line, &error_col)) {
      m_errorMessages.append(tr("XML error on line %1, col %2: %3").arg(error_line).arg(error_col).arg(error_msg));
      return false;
    }
  } else {
    // Virtual file from qmake evaluator
    QString error_msg;
    int error_line, error_col;
    if (!doc.setContent(contents, &error_msg, &error_line, &error_col)) {
      m_errorMessages.append(tr("XML error on line %1, col %2: %3").arg(error_line).arg(error_col).arg(error_msg));
      return false;
    }
  }

  QDomElement root = doc.firstChildElement(QLatin1String("RCC"));
  if (root.isNull()) {
    m_errorMessages.append(tr("The <RCC> root element is missing."));
    return false;
  }

  QDomElement relt = root.firstChildElement(QLatin1String("qresource"));
  for (; !relt.isNull(); relt = relt.nextSiblingElement(QLatin1String("qresource"))) {

    QString prefix = fixPrefix(relt.attribute(QLatin1String("prefix")));
    const QString language = relt.attribute(QLatin1String("lang"));
    if (!m_languages.contains(language))
      m_languages.append(language);

    QDomElement felt = relt.firstChildElement(QLatin1String("file"));
    for (; !felt.isNull(); felt = felt.nextSiblingElement(QLatin1String("file"))) {
      const QString fileName = felt.text();
      const QString alias = felt.attribute(QLatin1String("alias"));
      QString filePath = baseDir.absoluteFilePath(fileName);
      QString accessPath;
      if (!alias.isEmpty())
        accessPath = language + prefix + alias;
      else
        accessPath = language + prefix + fileName;
      QStringList &resources = m_resources[accessPath];
      if (!resources.contains(filePath))
        resources.append(filePath);
      QStringList &files = m_files[filePath];
      if (!files.contains(accessPath))
        files.append(accessPath);
    }
  }
  return true;
}

// path is assumed to be a normalized absolute path
auto QrcParserPrivate::firstFileAtPath(const QString &path, const QLocale &locale) const -> QString
{
  QTC_CHECK(path.startsWith(QLatin1Char('/')));
  for (const QString &language : allUiLanguages(&locale)) {
    if (m_languages.contains(language)) {
      SMap::const_iterator res = m_resources.find(language + path);
      if (res != m_resources.end())
        return res.value().at(0);
    }
  }
  return QString();
}

auto QrcParserPrivate::collectFilesAtPath(const QString &path, QStringList *files, const QLocale *locale) const -> void
{
  QTC_CHECK(path.startsWith(QLatin1Char('/')));
  for (const QString &language : allUiLanguages(locale)) {
    if (m_languages.contains(language)) {
      SMap::const_iterator res = m_resources.find(language + path);
      if (res != m_resources.end())
        (*files) << res.value();
    }
  }
}

// path is expected to be normalized and start and end with a slash
auto QrcParserPrivate::hasDirAtPath(const QString &path, const QLocale *locale) const -> bool
{
  QTC_CHECK(path.startsWith(QLatin1Char('/')));
  QTC_CHECK(path.endsWith(QLatin1Char('/')));
  for (const QString &language : allUiLanguages(locale)) {
    if (m_languages.contains(language)) {
      QString key = language + path;
      SMap::const_iterator res = m_resources.lowerBound(key);
      if (res != m_resources.end() && res.key().startsWith(key))
        return true;
    }
  }
  return false;
}

auto QrcParserPrivate::collectFilesInPath(const QString &path, QMap<QString, QStringList> *contents, bool addDirs, const QLocale *locale) const -> void
{
  QTC_CHECK(path.startsWith(QLatin1Char('/')));
  QTC_CHECK(path.endsWith(QLatin1Char('/')));
  SMap::const_iterator end = m_resources.end();
  for (const QString &language : allUiLanguages(locale)) {
    QString key = language + path;
    SMap::const_iterator res = m_resources.lowerBound(key);
    while (res != end && res.key().startsWith(key)) {
      const QString &actualKey = res.key();
      int endDir = actualKey.indexOf(QLatin1Char('/'), key.size());
      if (endDir == -1) {
        QString fileName = res.key().right(res.key().size() - key.size());
        QStringList &els = (*contents)[fileName];
        for (const QString &val : res.value())
          if (!els.contains(val))
            els << val;
        ++res;
      } else {
        QString dirName = res.key().mid(key.size(), endDir - key.size() + 1);
        if (addDirs)
          contents->insert(dirName, QStringList());
        QString key2 = key + dirName;
        do {
          ++res;
        } while (res != end && res.key().startsWith(key2));
      }
    }
  }
}

auto QrcParserPrivate::collectResourceFilesForSourceFile(const QString &sourceFile, QStringList *results, const QLocale *locale) const -> void
{
  // TODO: use FileName from fileutils for file paths

  const QStringList langs = allUiLanguages(locale);
  SMap::const_iterator file = m_files.find(sourceFile);
  if (file == m_files.end())
    return;
  for (const QString &resource : file.value()) {
    for (const QString &language : langs) {
      if (resource.startsWith(language) && !results->contains(resource))
        results->append(resource);
    }
  }
}

auto QrcParserPrivate::errorMessages() const -> QStringList
{
  return m_errorMessages;
}

auto QrcParserPrivate::languages() const -> QStringList
{
  return m_languages;
}

auto QrcParserPrivate::fixPrefix(const QString &prefix) -> QString
{
  const QChar slash = QLatin1Char('/');
  QString result = QString(slash);
  for (int i = 0; i < prefix.size(); ++i) {
    const QChar c = prefix.at(i);
    if (c == slash && result.at(result.size() - 1) == slash)
      continue;
    result.append(c);
  }

  if (!result.endsWith(slash))
    result.append(slash);

  return result;
}

auto QrcParserPrivate::allUiLanguages(const QLocale *locale) const -> const QStringList
{
  if (!locale)
    return languages();
  bool hasEmptyString = false;
  const QStringList langs = locale->uiLanguages();
  QStringList allLangs = langs;
  for (const QString &language : langs) {
    if (language.isEmpty())
      hasEmptyString = true;
    else if (language.contains('_') || language.contains('-')) {
      const QStringList splits = QString(language).replace('_', '-').split('-');
      if (splits.size() > 1 && !allLangs.contains(splits.at(0)))
        allLangs.append(splits.at(0));
    }
  }
  if (!hasEmptyString)
    allLangs.append(QString());
  return allLangs;
}

// ----------------

QrcCachePrivate::QrcCachePrivate(QrcCache *) { }

auto QrcCachePrivate::addPath(const QString &path, const QString &contents) -> QrcParser::Ptr
{
  QPair<QrcParser::Ptr, int> currentValue;
  {
    QMutexLocker l(&m_mutex);
    currentValue = m_cache.value(path, {QrcParser::Ptr(nullptr), 0});
    currentValue.second += 1;
    if (currentValue.second > 1) {
      m_cache.insert(path, currentValue);
      return currentValue.first;
    }
  }
  QrcParser::Ptr newParser = QrcParser::parseQrcFile(path, contents);
  if (!newParser->isValid())
    qCWarning(qrcParserLog) << "adding invalid qrc " << path << " to the cache:" << newParser->errorMessages();
  {
    QMutexLocker l(&m_mutex);
    QPair<QrcParser::Ptr, int> currentValue = m_cache.value(path, {QrcParser::Ptr(nullptr), 0});
    if (currentValue.first.isNull())
      currentValue.first = newParser;
    currentValue.second += 1;
    m_cache.insert(path, currentValue);
    return currentValue.first;
  }
}

auto QrcCachePrivate::removePath(const QString &path) -> void
{
  QPair<QrcParser::Ptr, int> currentValue;
  {
    QMutexLocker l(&m_mutex);
    currentValue = m_cache.value(path, {QrcParser::Ptr(nullptr), 0});
    if (currentValue.second == 1) {
      m_cache.remove(path);
    } else if (currentValue.second > 1) {
      currentValue.second -= 1;
      m_cache.insert(path, currentValue);
    } else {
      QTC_CHECK(!m_cache.contains(path));
    }
  }
}

auto QrcCachePrivate::updatePath(const QString &path, const QString &contents) -> QrcParser::Ptr
{
  QrcParser::Ptr newParser = QrcParser::parseQrcFile(path, contents);
  {
    QMutexLocker l(&m_mutex);
    QPair<QrcParser::Ptr, int> currentValue = m_cache.value(path, {QrcParser::Ptr(nullptr), 0});
    currentValue.first = newParser;
    if (currentValue.second == 0)
      currentValue.second = 1; // add qrc files that are not in the resources of a project
    m_cache.insert(path, currentValue);
    return currentValue.first;
  }
}

auto QrcCachePrivate::parsedPath(const QString &path) -> QrcParser::Ptr
{
  QMutexLocker l(&m_mutex);
  QPair<QrcParser::Ptr, int> currentValue = m_cache.value(path, {QrcParser::Ptr(nullptr), 0});
  return currentValue.first;
}

auto QrcCachePrivate::clear() -> void
{
  QMutexLocker l(&m_mutex);
  m_cache.clear();
}

} // namespace Internal
} // namespace QmlJS
