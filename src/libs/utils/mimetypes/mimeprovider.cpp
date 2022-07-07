// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "mimeprovider_p.hpp"

#include "mimetypeparser_p.hpp"
#include <qstandardpaths.h>
#include "mimemagicrulematcher_p.hpp"

#include <QXmlStreamReader>
#include <QDir>
#include <QFile>
#include <QByteArrayMatcher>
#include <QDebug>
#include <QDateTime>
#include <QtEndian>
#include <QtGlobal>

using namespace Utils;
using namespace Utils::Internal;

static auto fallbackParent(const QString &mimeTypeName) -> QString
{
  const QString myGroup = mimeTypeName.left(mimeTypeName.indexOf(QLatin1Char('/')));
  // All text/* types are subclasses of text/plain.
  if (myGroup == QLatin1String("text") && mimeTypeName != QLatin1String("text/plain"))
    return QLatin1String("text/plain");
  // All real-file mimetypes implicitly derive from application/octet-stream
  if (myGroup != QLatin1String("inode") &&
    // ignore non-file extensions
    myGroup != QLatin1String("all") && myGroup != QLatin1String("fonts") && myGroup != QLatin1String("print") && myGroup != QLatin1String("uri") && mimeTypeName != QLatin1String("application/octet-stream")) {
    return QLatin1String("application/octet-stream");
  }
  return QString();
}

MimeProviderBase::MimeProviderBase(MimeDatabasePrivate *db) : m_db(db) {}

static int mime_secondsBetweenChecks = 5;

auto MimeProviderBase::shouldCheck() -> bool
{
  const QDateTime now = QDateTime::currentDateTime();
  if (m_lastCheck.isValid() && m_lastCheck.secsTo(now) < mime_secondsBetweenChecks)
    return false;
  m_lastCheck = now;
  return true;
}

MimeXMLProvider::MimeXMLProvider(MimeDatabasePrivate *db) : MimeProviderBase(db), m_loaded(false) {}

auto MimeXMLProvider::isValid() -> bool
{
  return true;
}

auto MimeXMLProvider::mimeTypeForName(const QString &name) -> MimeType
{
  ensureLoaded();

  return m_nameMimeTypeMap.value(name);
}

auto MimeXMLProvider::findByFileName(const QString &fileName, QString *foundSuffix) -> QStringList
{
  ensureLoaded();

  const QStringList matchingMimeTypes = m_mimeTypeGlobs.matchingGlobs(fileName, foundSuffix);
  return matchingMimeTypes;
}

auto MimeXMLProvider::findByMagic(const QByteArray &data, int *accuracyPtr) -> MimeType
{
  ensureLoaded();

  QString candidate;

  for (const MimeMagicRuleMatcher &matcher : qAsConst(m_magicMatchers)) {
    if (matcher.matches(data)) {
      const int priority = matcher.priority();
      if (priority > *accuracyPtr) {
        *accuracyPtr = priority;
        candidate = matcher.mimetype();
      }
    }
  }
  return mimeTypeForName(candidate);
}

auto MimeXMLProvider::magicRulesForMimeType(const MimeType &mimeType) -> QMap<int, QList<MimeMagicRule>>
{
  QMap<int, QList<MimeMagicRule>> result;
  for (const MimeMagicRuleMatcher &matcher : qAsConst(m_magicMatchers)) {
    if (mimeType.matchesName(matcher.mimetype()))
      result[matcher.priority()].append(matcher.magicRules());
  }
  return result;
}

auto MimeXMLProvider::setGlobPatternsForMimeType(const MimeType &mimeType, const QStringList &patterns) -> void
{
  // remove all previous globs
  m_mimeTypeGlobs.removeMimeType(mimeType.name());
  // add new patterns as case-insensitive default-weight patterns
  for (const QString &pattern : patterns)
    addGlobPattern(MimeGlobPattern(pattern, mimeType.name()));
  mimeType.d->globPatterns = patterns;
}

auto MimeXMLProvider::setMagicRulesForMimeType(const MimeType &mimeType, const QMap<int, QList<MimeMagicRule>> &rules) -> void
{
  // remove all previous rules
  for (int i = 0; i < m_magicMatchers.size(); ++i) {
    if (m_magicMatchers.at(i).mimetype() == mimeType.name())
      m_magicMatchers.removeAt(i--);
  }
  // add new rules
  for (auto it = rules.cbegin(); it != rules.cend(); ++it) {
    MimeMagicRuleMatcher matcher(mimeType.name(), it.key()/*priority*/);
    matcher.addRules(it.value());
    addMagicMatcher(matcher);
  }
}

auto MimeXMLProvider::ensureLoaded() -> void
{
  if (!m_loaded /*|| shouldCheck()*/) {
    m_loaded = true;
    QStringList allFiles = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("mime/packages/freedesktop.org.xml"), QStandardPaths::LocateFile);

    if (allFiles.isEmpty()) {
      // System freedsktop.org.xml file not found, use our bundled copy
      const char freedesktopOrgXml[] = ":/utils/mimetypes/freedesktop.org.xml";
      allFiles.prepend(QLatin1String(freedesktopOrgXml));
    }

    m_nameMimeTypeMap.clear();
    m_aliases.clear();
    m_parents.clear();
    m_mimeTypeGlobs.clear();
    m_magicMatchers.clear();

    //qDebug() << "Loading" << m_allFiles;

    // add custom mime types first, which override any default from freedesktop.org.xml
    MimeTypeParser parser(*this);
    for (auto it = m_additionalData.constBegin(), end = m_additionalData.constEnd(); it != end; ++it) {
      QString errorMessage;
      if (!parser.parse(it.value(), it.key(), &errorMessage)) {
        qWarning("MimeDatabase: Error loading %s\n%s", qPrintable(it.key()), qPrintable(errorMessage));
      }
    }

    for (const QString &file : qAsConst(allFiles))
      load(file);
  }
}

auto MimeXMLProvider::load(const QString &fileName) -> void
{
  QString errorMessage;
  if (!load(fileName, &errorMessage))
    qWarning("MimeDatabase: Error loading %s\n%s", qPrintable(fileName), qPrintable(errorMessage));
}

auto MimeXMLProvider::load(const QString &fileName, QString *errorMessage) -> bool
{
  m_loaded = true;

  QFile file(fileName);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (errorMessage)
      *errorMessage = QString::fromLatin1("Cannot open %1: %2").arg(fileName, file.errorString());
    return false;
  }

  if (errorMessage)
    errorMessage->clear();

  const QByteArray content = file.readAll();
  MimeTypeParser parser(*this);
  return parser.parse(content, fileName, errorMessage);
}

auto MimeXMLProvider::addGlobPattern(const MimeGlobPattern &glob) -> void
{
  m_mimeTypeGlobs.addGlob(glob);
}

auto MimeXMLProvider::addMimeType(const MimeType &mt) -> void
{
  m_nameMimeTypeMap.insert(mt.name(), mt);
}

auto MimeXMLProvider::parents(const QString &mime) -> QStringList
{
  ensureLoaded();
  QStringList result = m_parents.value(mime);
  if (result.isEmpty()) {
    const QString parent = fallbackParent(mime);
    if (!parent.isEmpty())
      result.append(parent);
  }
  return result;
}

auto MimeXMLProvider::addParent(const QString &child, const QString &parent) -> void
{
  m_parents[child].append(parent);
}

auto MimeXMLProvider::listAliases(const QString &name) -> QStringList
{
  ensureLoaded();
  // Iterate through the whole hash. This method is rarely used.
  return m_aliases.keys(name);
}

auto MimeXMLProvider::resolveAlias(const QString &name) -> QString
{
  ensureLoaded();
  return m_aliases.value(name, name);
}

auto MimeXMLProvider::addAlias(const QString &alias, const QString &name) -> void
{
  m_aliases.insert(alias, name);
}

auto MimeXMLProvider::allMimeTypes() -> QList<MimeType>
{
  ensureLoaded();
  return m_nameMimeTypeMap.values();
}

auto MimeXMLProvider::addMagicMatcher(const MimeMagicRuleMatcher &matcher) -> void
{
  m_magicMatchers.append(matcher);
}

auto MimeXMLProvider::addData(const QString &id, const QByteArray &data) -> void
{
  if (m_additionalData.contains(id))
    qWarning("Overwriting data in mime database, id '%s'", qPrintable(id));
  m_additionalData.insert(id, data);
  m_loaded = false; // force reload to ensure correct load order for overridden mime types
}
