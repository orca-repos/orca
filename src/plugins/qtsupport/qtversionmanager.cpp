// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtversionmanager.hpp"

#include "baseqtversion.hpp"
#include "exampleslistmodel.hpp"
#include "qtkitinformation.hpp"
#include "qtsupportconstants.hpp"
#include "qtversionfactory.hpp"

#include <core/icore.hpp>
#include <core/helpmanager.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <projectexplorer/toolchainmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/buildablehelperlibrary.hpp>
#include <utils/environment.hpp>
#include <utils/filesystemwatcher.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/persistentsettings.hpp>
#include <utils/qtcassert.hpp>
#include <utils/qtcprocess.hpp>

#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QTimer>

using namespace Utils;

namespace QtSupport {

using namespace Internal;

using  VersionMap = QMap<int, QtVersion *>;
static VersionMap m_versions;

constexpr char QTVERSION_DATA_KEY[] = "QtVersion.";
constexpr char QTVERSION_TYPE_KEY[] = "QtVersion.Type";
constexpr char QTVERSION_FILE_VERSION_KEY[] = "Version";
constexpr char QTVERSION_FILENAME[] = "qtversion.xml";
constexpr char DOCUMENTATION_SETTING_KEY[] = "QtSupport/DocumentationSetting";

static int m_idcount = 0;

// managed by QtProjectManagerPlugin
static QtVersionManager *m_instance = nullptr;
static FileSystemWatcher *m_configFileWatcher = nullptr;
static QTimer *m_fileWatcherTimer = nullptr;
static PersistentSettingsWriter *m_writer = nullptr;
static QVector<ExampleSetModel::ExtraExampleSet> m_pluginRegisteredExampleSets;

static Q_LOGGING_CATEGORY(log, "qtc.qt.versions", QtWarningMsg);

static auto globalSettingsFileName() -> FilePath
{
  return Core::ICore::installerResourcePath(QTVERSION_FILENAME);
}

static auto settingsFileName(const QString &path) -> FilePath
{
  return Core::ICore::userResourcePath(path);
}

// prefer newer qts otherwise compare on id
auto qtVersionNumberCompare(QtVersion *a, QtVersion *b) -> bool
{
  return a->qtVersion() > b->qtVersion() || (a->qtVersion() == b->qtVersion() && a->uniqueId() < b->uniqueId());
}

static auto restoreQtVersions() -> bool;
static auto findSystemQt() -> void;
static auto saveQtVersions() -> void;

auto ExampleSetModel::pluginRegisteredExampleSets() -> QVector<ExtraExampleSet>
{
  return m_pluginRegisteredExampleSets;
}

// --------------------------------------------------------------------------
// QtVersionManager
// --------------------------------------------------------------------------

QtVersionManager::QtVersionManager()
{
  m_instance = this;
  m_configFileWatcher = nullptr;
  m_fileWatcherTimer = new QTimer(this);
  m_writer = nullptr;
  m_idcount = 1;

  qRegisterMetaType<FilePath>();

  // Give the file a bit of time to settle before reading it...
  m_fileWatcherTimer->setInterval(2000);
  connect(m_fileWatcherTimer, &QTimer::timeout, this, [this] { updateFromInstaller(); });
}

auto QtVersionManager::triggerQtVersionRestore() -> void
{
  disconnect(ProjectExplorer::ToolChainManager::instance(), &ProjectExplorer::ToolChainManager::toolChainsLoaded, this, &QtVersionManager::triggerQtVersionRestore);

  const auto success = restoreQtVersions();
  m_instance->updateFromInstaller(false);
  if (!success) {
    // We did neither restore our settings or upgraded
    // in that case figure out if there's a qt in path
    // and add it to the Qt versions
    findSystemQt();
  }

  emit m_instance->qtVersionsLoaded();
  emit m_instance->qtVersionsChanged(m_versions.keys(), QList<int>(), QList<int>());
  saveQtVersions();

  const auto configFileName = globalSettingsFileName();
  if (configFileName.exists()) {
    m_configFileWatcher = new FileSystemWatcher(m_instance);
    connect(m_configFileWatcher, &FileSystemWatcher::fileChanged, m_fileWatcherTimer, QOverload<>::of(&QTimer::start));
    m_configFileWatcher->addFile(configFileName.toString(), FileSystemWatcher::WatchModifiedDate);
  } // exists

  const auto vs = versions();
  updateDocumentation(vs, {}, vs);
}

auto QtVersionManager::isLoaded() -> bool
{
  return m_writer;
}

QtVersionManager::~QtVersionManager()
{
  delete m_writer;
  qDeleteAll(m_versions);
  m_versions.clear();
}

auto QtVersionManager::initialized() -> void
{
  connect(ProjectExplorer::ToolChainManager::instance(), &ProjectExplorer::ToolChainManager::toolChainsLoaded, instance(), &QtVersionManager::triggerQtVersionRestore);
}

auto QtVersionManager::instance() -> QtVersionManager*
{
  return m_instance;
}

static auto restoreQtVersions() -> bool
{
  QTC_ASSERT(!m_writer, return false);
  m_writer = new PersistentSettingsWriter(settingsFileName(QTVERSION_FILENAME), "QtCreatorQtVersions");

  const auto factories = QtVersionFactory::allQtVersionFactories();

  PersistentSettingsReader reader;
  const auto filename = settingsFileName(QTVERSION_FILENAME);

  if (!reader.load(filename))
    return false;
  const auto data = reader.restoreValues();

  // Check version:
  const auto version = data.value(QTVERSION_FILE_VERSION_KEY, 0).toInt();
  if (version < 1)
    return false;

  const QString keyPrefix(QTVERSION_DATA_KEY);
  const auto dcend = data.constEnd();
  for (auto it = data.constBegin(); it != dcend; ++it) {
    const auto &key = it.key();
    if (!key.startsWith(keyPrefix))
      continue;
    bool ok;
    const auto count = key.mid(keyPrefix.count()).toInt(&ok);
    if (!ok || count < 0)
      continue;

    const auto qtversionMap = it.value().toMap();
    const auto type = qtversionMap.value(QTVERSION_TYPE_KEY).toString();

    auto restored = false;
    for (const auto f : factories) {
      if (f->canRestore(type)) {
        if (auto qtv = f->restore(type, qtversionMap)) {
          if (m_versions.contains(qtv->uniqueId())) {
            // This shouldn't happen, we are restoring the same id multiple times?
            qWarning() << "A Qt version with id" << qtv->uniqueId() << "already exists";
            delete qtv;
          } else {
            m_versions.insert(qtv->uniqueId(), qtv);
            m_idcount = qtv->uniqueId() > m_idcount ? qtv->uniqueId() : m_idcount;
            restored = true;
            break;
          }
        }
      }
    }
    if (!restored)
      qWarning("Warning: Unable to restore Qt version '%s' stored in %s.", qPrintable(type), qPrintable(filename.toUserOutput()));
  }
  ++m_idcount;

  return true;
}

auto QtVersionManager::updateFromInstaller(bool emitSignal) -> void
{
  m_fileWatcherTimer->stop();

  const auto path = globalSettingsFileName();
  // Handle overwritting of data:
  if (m_configFileWatcher) {
    m_configFileWatcher->removeFile(path.toString());
    m_configFileWatcher->addFile(path.toString(), FileSystemWatcher::WatchModifiedDate);
  }

  QList<int> added;
  QList<int> removed;
  QList<int> changed;

  const auto factories = QtVersionFactory::allQtVersionFactories();
  PersistentSettingsReader reader;
  QVariantMap data;
  if (reader.load(path))
    data = reader.restoreValues();

  if (log().isDebugEnabled()) {
    qCDebug(log) << "======= Existing Qt versions =======";
    for (auto version : qAsConst(m_versions)) {
      qCDebug(log) << version->qmakeFilePath().toUserOutput() << "id:" << version->uniqueId();
      qCDebug(log) << "  autodetection source:" << version->detectionSource();
      qCDebug(log) << "";
    }
    qCDebug(log) << "======= Adding sdk versions =======";
  }

  QStringList sdkVersions;

  const QString keyPrefix(QTVERSION_DATA_KEY);
  const auto dcend = data.constEnd();
  for (auto it = data.constBegin(); it != dcend; ++it) {
    const auto &key = it.key();
    if (!key.startsWith(keyPrefix))
      continue;
    bool ok;
    auto count = key.mid(keyPrefix.count()).toInt(&ok);
    if (!ok || count < 0)
      continue;

    auto qtversionMap = it.value().toMap();
    const auto type = qtversionMap.value(QTVERSION_TYPE_KEY).toString();
    const auto autoDetectionSource = qtversionMap.value("autodetectionSource").toString();
    sdkVersions << autoDetectionSource;
    auto id = -1; // see QtVersion::fromMap()
    QtVersionFactory *factory = nullptr;
    for (auto f : factories) {
      if (f->canRestore(type))
        factory = f;
    }
    if (!factory) {
      qCDebug(log, "Warning: Unable to find factory for type '%s'", qPrintable(type));
      continue;
    }
    // First try to find a existing Qt version to update
    auto restored = false;
    const auto versionsCopy = m_versions; // m_versions is modified in loop
    for (auto v : versionsCopy) {
      if (v->detectionSource() == autoDetectionSource) {
        id = v->uniqueId();
        qCDebug(log) << " Qt version found with same autodetection source" << autoDetectionSource << " => Migrating id:" << id;
        m_versions.remove(id);
        qtversionMap[Constants::QTVERSIONID] = id;
        qtversionMap[Constants::QTVERSIONNAME] = v->unexpandedDisplayName();
        delete v;

        if (auto qtv = factory->restore(type, qtversionMap)) {
          Q_ASSERT(qtv->isAutodetected());
          m_versions.insert(id, qtv);
          restored = true;
        }
        if (restored)
          changed << id;
        else
          removed << id;
      }
    }
    // Create a new qtversion
    if (!restored) {
      // didn't replace any existing versions
      qCDebug(log) << " No Qt version found matching" << autoDetectionSource << " => Creating new version";
      if (auto qtv = factory->restore(type, qtversionMap)) {
        Q_ASSERT(qtv->isAutodetected());
        m_versions.insert(qtv->uniqueId(), qtv);
        added << qtv->uniqueId();
        restored = true;
      }
    }
    if (!restored) {
      qCDebug(log, "Warning: Unable to update qtversion '%s' from sdk installer.", qPrintable(autoDetectionSource));
    }
  }

  if (log().isDebugEnabled()) {
    qCDebug(log) << "======= Before removing outdated sdk versions =======";
    for (auto version : qAsConst(m_versions)) {
      qCDebug(log) << version->qmakeFilePath().toUserOutput() << "id:" << version->uniqueId();
      qCDebug(log) << "  autodetection source:" << version->detectionSource();
      qCDebug(log) << "";
    }
  }
  const auto versionsCopy = m_versions; // m_versions is modified in loop
  for (auto qtVersion : versionsCopy) {
    if (qtVersion->detectionSource().startsWith("SDK.")) {
      if (!sdkVersions.contains(qtVersion->detectionSource())) {
        qCDebug(log) << "  removing version" << qtVersion->detectionSource();
        m_versions.remove(qtVersion->uniqueId());
        removed << qtVersion->uniqueId();
      }
    }
  }

  if (log().isDebugEnabled()) {
    qCDebug(log) << "======= End result =======";
    for (auto version : qAsConst(m_versions)) {
      qCDebug(log) << version->qmakeFilePath().toUserOutput() << "id:" << version->uniqueId();
      qCDebug(log) << "  autodetection source:" << version->detectionSource();
      qCDebug(log) << "";
    }
  }
  if (emitSignal) emit qtVersionsChanged(added, removed, changed);
}

static auto saveQtVersions() -> void
{
  if (!m_writer)
    return;

  QVariantMap data;
  data.insert(QTVERSION_FILE_VERSION_KEY, 1);

  auto count = 0;
  for (const auto qtv : qAsConst(m_versions)) {
    auto tmp = qtv->toMap();
    if (tmp.isEmpty())
      continue;
    tmp.insert(QTVERSION_TYPE_KEY, qtv->type());
    data.insert(QString::fromLatin1(QTVERSION_DATA_KEY) + QString::number(count), tmp);
    ++count;
  }
  m_writer->save(data, Core::ICore::dialogParent());
}

// Executes qtchooser with arguments in a process and returns its output
static auto runQtChooser(const QString &qtchooser, const QStringList &arguments) -> QList<QByteArray>
{
  QtcProcess p;
  p.setCommand({FilePath::fromString(qtchooser), arguments});
  p.start();
  p.waitForFinished();
  const auto success = p.exitCode() == 0;
  return success ? p.readAllStandardOutput().split('\n') : QList<QByteArray>();
}

// Asks qtchooser for the qmake path of a given version
static auto qmakePath(const QString &qtchooser, const QString &version) -> QString
{
  const auto outputs = runQtChooser(qtchooser, {QStringLiteral("-qt=%1").arg(version), QStringLiteral("-print-env")});
  for (const auto &output : outputs) {
    if (output.startsWith("QTTOOLDIR=\"")) {
      auto withoutVarName = output.mid(11); // remove QTTOOLDIR="
      withoutVarName.chop(1);               // remove trailing quote
      return QStandardPaths::findExecutable(QStringLiteral("qmake"), QStringList() << QString::fromLocal8Bit(withoutVarName));
    }
  }
  return QString();
}

static auto gatherQmakePathsFromQtChooser() -> FilePaths
{
  const auto qtchooser = QStandardPaths::findExecutable(QStringLiteral("qtchooser"));
  if (qtchooser.isEmpty())
    return FilePaths();

  const auto versions = runQtChooser(qtchooser, QStringList("-l"));
  QSet<FilePath> foundQMakes;
  for (const auto &version : versions) {
    auto possibleQMake = FilePath::fromString(qmakePath(qtchooser, QString::fromLocal8Bit(version)));
    if (!possibleQMake.isEmpty())
      foundQMakes << possibleQMake;
  }
  return toList(foundQMakes);
}

static auto findSystemQt() -> void
{
  auto systemQMakes = BuildableHelperLibrary::findQtsInEnvironment(Environment::systemEnvironment());
  systemQMakes.append(gatherQmakePathsFromQtChooser());
  for (const auto &qmakePath : qAsConst(systemQMakes)) {
    if (BuildableHelperLibrary::isQtChooser(qmakePath))
      continue;
    const auto isSameQmake = [qmakePath](const QtVersion *version) {
      return Environment::systemEnvironment().isSameExecutable(qmakePath.toString(), version->qmakeFilePath().toString());
    };
    if (contains(m_versions, isSameQmake))
      continue;
    auto version = QtVersionFactory::createQtVersionFromQMakePath(qmakePath, false, "PATH");
    if (version)
      m_versions.insert(version->uniqueId(), version);
  }
}

auto QtVersionManager::addVersion(QtVersion *version) -> void
{
  QTC_ASSERT(m_writer, return);
  QTC_ASSERT(version, return);
  if (m_versions.contains(version->uniqueId()))
    return;

  const auto uniqueId = version->uniqueId();
  m_versions.insert(uniqueId, version);

  emit m_instance->qtVersionsChanged(QList<int>() << uniqueId, QList<int>(), QList<int>());
  saveQtVersions();
}

auto QtVersionManager::removeVersion(QtVersion *version) -> void
{
  QTC_ASSERT(version, return);
  m_versions.remove(version->uniqueId());
  emit m_instance->qtVersionsChanged(QList<int>(), QList<int>() << version->uniqueId(), QList<int>());
  saveQtVersions();
  delete version;
}

auto QtVersionManager::registerExampleSet(const QString &displayName, const QString &manifestPath, const QString &examplesPath) -> void
{
  m_pluginRegisteredExampleSets.append({displayName, manifestPath, examplesPath});
}

using Path = QString;
using FileName = QString;

static auto documentationFiles(QtVersion *v) -> QList<std::pair<Path, FileName>>
{
  QList<std::pair<Path, FileName>> files;
  const auto docPaths = QStringList({v->docsPath().toString() + QChar('/'), v->docsPath().toString() + "/qch/"});
  for (const auto &docPath : docPaths) {
    const QDir versionHelpDir(docPath);
    for (const auto &helpFile : versionHelpDir.entryList(QStringList("*.qch"), QDir::Files))
      files.append({docPath, helpFile});
  }
  return files;
}

static auto documentationFiles(const QtVersions &vs, bool highestOnly = false) -> QStringList
{
  // if highestOnly is true, register each file only once per major Qt version, even if
  // multiple minor or patch releases of that major version are installed
  QHash<int, QSet<QString>> includedFileNames; // major Qt version -> names
  QSet<QString> filePaths;
  const auto versions = highestOnly ? QtVersionManager::sortVersions(vs) : vs;
  for (const auto v : versions) {
    const auto majorVersion = v->qtVersion().majorVersion;
    auto &majorVersionFileNames = includedFileNames[majorVersion];
    for (const auto &file : documentationFiles(v)) {
      if (!highestOnly || !majorVersionFileNames.contains(file.second)) {
        filePaths.insert(file.first + file.second);
        majorVersionFileNames.insert(file.second);
      }
    }
  }
  return filePaths.values();
}

auto QtVersionManager::updateDocumentation(const QtVersions &added, const QtVersions &removed, const QtVersions &allNew) -> void
{
  const auto setting = documentationSetting();
  const auto docsOfAll = setting == DocumentationSetting::None ? QStringList() : documentationFiles(allNew, setting == DocumentationSetting::HighestOnly);
  const auto docsToRemove = filtered(documentationFiles(removed), [&docsOfAll](const QString &f) {
    return !docsOfAll.contains(f);
  });
  const auto docsToAdd = filtered(documentationFiles(added), [&docsOfAll](const QString &f) {
    return docsOfAll.contains(f);
  });
  Core::HelpManager::unregisterDocumentation(docsToRemove);
  Core::HelpManager::registerDocumentation(docsToAdd);
}

auto QtVersionManager::getUniqueId() -> int
{
  return m_idcount++;
}

auto QtVersionManager::versions(const QtVersion::Predicate &predicate) -> QtVersions
{
  QtVersions versions;
  QTC_ASSERT(isLoaded(), return versions);
  if (predicate)
    return filtered(m_versions.values(), predicate);
  return m_versions.values();
}

auto QtVersionManager::sortVersions(const QtVersions &input) -> QtVersions
{
  auto result = input;
  sort(result, qtVersionNumberCompare);
  return result;
}

auto QtVersionManager::version(int id) -> QtVersion*
{
  QTC_ASSERT(isLoaded(), return nullptr);
  const auto it = m_versions.constFind(id);
  if (it == m_versions.constEnd())
    return nullptr;
  return it.value();
}

auto QtVersionManager::version(const QtVersion::Predicate &predicate) -> QtVersion*
{
  return findOrDefault(m_versions.values(), predicate);
}

// This function is really simplistic...
static auto equals(QtVersion *a, QtVersion *b) -> bool
{
  return a->equals(b);
}

auto QtVersionManager::setNewQtVersions(const QtVersions &newVersions) -> void
{
  // We want to preserve the same order as in the settings dialog
  // so we sort a copy
  auto sortedNewVersions = newVersions;
  sort(sortedNewVersions, &QtVersion::uniqueId);

  QtVersions addedVersions;
  QtVersions removedVersions;
  QList<std::pair<QtVersion*, QtVersion*>> changedVersions;
  // So we trying to find the minimal set of changed versions,
  // iterate over both sorted list

  // newVersions and oldVersions iterator
  QtVersions::const_iterator nit, nend;
  VersionMap::const_iterator oit, oend;
  nit = sortedNewVersions.constBegin();
  nend = sortedNewVersions.constEnd();
  oit = m_versions.constBegin();
  oend = m_versions.constEnd();

  while (nit != nend && oit != oend) {
    const auto nid = (*nit)->uniqueId();
    const auto oid = (*oit)->uniqueId();
    if (nid < oid) {
      addedVersions.push_back(*nit);
      ++nit;
    } else if (oid < nid) {
      removedVersions.push_back(*oit);
      ++oit;
    } else {
      if (!equals(*oit, *nit))
        changedVersions.push_back({*oit, *nit});
      ++oit;
      ++nit;
    }
  }

  while (nit != nend) {
    addedVersions.push_back(*nit);
    ++nit;
  }

  while (oit != oend) {
    removedVersions.push_back(*oit);
    ++oit;
  }

  if (!changedVersions.isEmpty() || !addedVersions.isEmpty() || !removedVersions.isEmpty()) {
    const auto changedOldVersions = Utils::transform(changedVersions, &std::pair<QtVersion*, QtVersion*>::first);
    const auto changedNewVersions = Utils::transform(changedVersions, &std::pair<QtVersion*, QtVersion*>::second);
    updateDocumentation(addedVersions + changedNewVersions, removedVersions + changedOldVersions, sortedNewVersions);
  }
  const auto addedIds = transform(addedVersions, &QtVersion::uniqueId);
  const auto removedIds = transform(removedVersions, &QtVersion::uniqueId);
  const auto changedIds = Utils::transform(changedVersions, [](std::pair<QtVersion*, QtVersion*> v) {
    return v.first->uniqueId();
  });

  qDeleteAll(m_versions);
  m_versions = Utils::transform<VersionMap>(sortedNewVersions, [](QtVersion *v) {
    return std::make_pair(v->uniqueId(), v);
  });
  saveQtVersions();

  if (!changedVersions.isEmpty() || !addedVersions.isEmpty() || !removedVersions.isEmpty()) emit m_instance->qtVersionsChanged(addedIds, removedIds, changedIds);
}

auto QtVersionManager::setDocumentationSetting(const DocumentationSetting &setting) -> void
{
  if (setting == documentationSetting())
    return;
  Core::ICore::settings()->setValueWithDefault(DOCUMENTATION_SETTING_KEY, int(setting), 0);
  // force re-evaluating which documentation should be registered
  // by claiming that all are removed and re-added
  const auto vs = versions();
  updateDocumentation(vs, vs, vs);
}

auto QtVersionManager::documentationSetting() -> DocumentationSetting
{
  return DocumentationSetting(Core::ICore::settings()->value(DOCUMENTATION_SETTING_KEY, 0).toInt());
}

} // namespace QtVersion
