// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "exampleslistmodel.hpp"
#include "screenshotcropper.hpp"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QPixmapCache>
#include <QUrl>

#include <android/androidconstants.h>
#include <ios/iosconstants.h>
#include <core/helpmanager.hpp>
#include <core/icore.hpp>

#include <qtsupport/qtkitinformation.hpp>
#include <qtsupport/qtversionmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/filepath.hpp>
#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/stylehelper.hpp>

#include <algorithm>
#include <memory>

using namespace Utils;

namespace QtSupport {
namespace Internal {

static auto debugExamples() -> bool
{
  static auto isDebugging = qEnvironmentVariableIsSet("QTC_DEBUG_EXAMPLESMODEL");
  return isDebugging;
}

static constexpr char kSelectedExampleSetKey[] = "WelcomePage/SelectedExampleSet";

auto ExampleSetModel::writeCurrentIdToSettings(int currentIndex) const -> void
{
  QSettings *settings = Core::ICore::settings();
  settings->setValue(QLatin1String(kSelectedExampleSetKey), getId(currentIndex));
}

auto ExampleSetModel::readCurrentIndexFromSettings() const -> int
{
  const auto id = Core::ICore::settings()->value(QLatin1String(kSelectedExampleSetKey));
  for (auto i = 0; i < rowCount(); i++) {
    if (id == getId(i))
      return i;
  }
  return -1;
}

ExampleSetModel::ExampleSetModel()
{
  // read extra example sets settings
  const QSettings *settings = Core::ICore::settings();
  const auto list = settings->value("Help/InstalledExamples", QStringList()).toStringList();
  if (debugExamples())
    qWarning() << "Reading Help/InstalledExamples from settings:" << list;
  for (const auto &item : list) {
    const auto &parts = item.split(QLatin1Char('|'));
    if (parts.size() < 3) {
      if (debugExamples())
        qWarning() << "Item" << item << "has less than 3 parts (separated by '|'):" << parts;
      continue;
    }
    ExtraExampleSet set;
    set.displayName = parts.at(0);
    set.manifestPath = parts.at(1);
    set.examplesPath = parts.at(2);
    QFileInfo fi(set.manifestPath);
    if (!fi.isDir() || !fi.isReadable()) {
      if (debugExamples())
        qWarning() << "Manifest path " << set.manifestPath << "is not a readable directory, ignoring";
      continue;
    }
    if (debugExamples()) {
      qWarning() << "Adding examples set displayName=" << set.displayName << ", manifestPath=" << set.manifestPath << ", examplesPath=" << set.examplesPath;
    }
    if (!anyOf(m_extraExampleSets, [&set](const ExtraExampleSet &s) {
      return FilePath::fromString(s.examplesPath).cleanPath() == FilePath::fromString(set.examplesPath).cleanPath() && FilePath::fromString(s.manifestPath).cleanPath() == FilePath::fromString(set.manifestPath).cleanPath();
    })) {
      m_extraExampleSets.append(set);
    } else if (debugExamples()) {
      qWarning() << "Not adding, because example set with same directories exists";
    }
  }
  m_extraExampleSets += pluginRegisteredExampleSets();

  connect(QtVersionManager::instance(), &QtVersionManager::qtVersionsLoaded, this, &ExampleSetModel::qtVersionManagerLoaded);

  connect(Core::HelpManager::Signals::instance(), &Core::HelpManager::Signals::setupFinished, this, &ExampleSetModel::helpManagerInitialized);
}

auto ExampleSetModel::recreateModel(const QtVersions &qtVersions) -> void
{
  beginResetModel();
  clear();

  QSet<QString> extraManifestDirs;
  for (auto i = 0; i < m_extraExampleSets.size(); ++i) {
    const auto &set = m_extraExampleSets.at(i);
    const auto newItem = new QStandardItem();
    newItem->setData(set.displayName, Qt::DisplayRole);
    newItem->setData(set.displayName, Qt::UserRole + 1);
    newItem->setData(QVariant(), Qt::UserRole + 2);
    newItem->setData(i, Qt::UserRole + 3);
    appendRow(newItem);

    extraManifestDirs.insert(set.manifestPath);
  }

  foreach(QtVersion *version, qtVersions) {
    // sanitize away qt versions that have already been added through extra sets
    if (extraManifestDirs.contains(version->docsPath().toString())) {
      if (debugExamples()) {
        qWarning() << "Not showing Qt version because manifest path is already added through InstalledExamples settings:" << version->displayName();
      }
      continue;
    }
    const auto newItem = new QStandardItem();
    newItem->setData(version->displayName(), Qt::DisplayRole);
    newItem->setData(version->displayName(), Qt::UserRole + 1);
    newItem->setData(version->uniqueId(), Qt::UserRole + 2);
    newItem->setData(QVariant(), Qt::UserRole + 3);
    appendRow(newItem);
  }
  endResetModel();
}

auto ExampleSetModel::indexForQtVersion(QtVersion *qtVersion) const -> int
{
  // return either the entry with the same QtId, or an extra example set with same path

  if (!qtVersion)
    return -1;

  // check for Qt version
  for (auto i = 0; i < rowCount(); ++i) {
    if (getType(i) == QtExampleSet && getQtId(i) == qtVersion->uniqueId())
      return i;
  }

  // check for extra set
  const auto &documentationPath = qtVersion->docsPath().toString();
  for (auto i = 0; i < rowCount(); ++i) {
    if (getType(i) == ExtraExampleSetType && m_extraExampleSets.at(getExtraExampleSetIndex(i)).manifestPath == documentationPath)
      return i;
  }
  return -1;
}

auto ExampleSetModel::getDisplayName(int i) const -> QVariant
{
  if (i < 0 || i >= rowCount())
    return QVariant();
  return data(index(i, 0), Qt::UserRole + 1);
}

// id is either the Qt version uniqueId, or the display name of the extra example set
auto ExampleSetModel::getId(int i) const -> QVariant
{
  if (i < 0 || i >= rowCount())
    return QVariant();
  const auto modelIndex = index(i, 0);
  auto variant = data(modelIndex, Qt::UserRole + 2);
  if (variant.isValid()) // set from qt version
    return variant;
  return getDisplayName(i);
}

auto ExampleSetModel::getType(int i) const -> ExampleSetType
{
  if (i < 0 || i >= rowCount())
    return InvalidExampleSet;
  const auto modelIndex = index(i, 0);
  const auto variant = data(modelIndex, Qt::UserRole + 2); /*Qt version uniqueId*/
  if (variant.isValid())
    return QtExampleSet;
  return ExtraExampleSetType;
}

auto ExampleSetModel::getQtId(int i) const -> int
{
  QTC_ASSERT(i >= 0, return -1);
  const auto modelIndex = index(i, 0);
  const auto variant = data(modelIndex, Qt::UserRole + 2);
  QTC_ASSERT(variant.isValid(), return -1);
  QTC_ASSERT(variant.canConvert<int>(), return -1);
  return variant.toInt();
}

auto ExampleSetModel::selectedQtSupports(const Id &target) const -> bool
{
  return m_selectedQtTypes.contains(target);
}

auto ExampleSetModel::getExtraExampleSetIndex(int i) const -> int
{
  QTC_ASSERT(i >= 0, return -1);
  const auto modelIndex = index(i, 0);
  const auto variant = data(modelIndex, Qt::UserRole + 3);
  QTC_ASSERT(variant.isValid(), return -1);
  QTC_ASSERT(variant.canConvert<int>(), return -1);
  return variant.toInt();
}

ExamplesListModel::ExamplesListModel(QObject *parent) : ListModel(parent)
{
  connect(&m_exampleSetModel, &ExampleSetModel::selectedExampleSetChanged, this, &ExamplesListModel::updateExamples);
  connect(Core::HelpManager::Signals::instance(), &Core::HelpManager::Signals::documentationChanged, this, &ExamplesListModel::updateExamples);
}

static auto fixStringForTags(const QString &string) -> QString
{
  auto returnString = string;
  returnString.remove(QLatin1String("<i>"));
  returnString.remove(QLatin1String("</i>"));
  returnString.remove(QLatin1String("<tt>"));
  returnString.remove(QLatin1String("</tt>"));
  return returnString;
}

static auto trimStringList(const QStringList &stringlist) -> QStringList
{
  return transform(stringlist, [](const QString &str) { return str.trimmed(); });
}

static auto relativeOrInstallPath(const QString &path, const QString &manifestPath, const QString &installPath) -> QString
{
  const QChar slash = QLatin1Char('/');
  const QString relativeResolvedPath = manifestPath + slash + path;
  const QString installResolvedPath = installPath + slash + path;
  if (QFile::exists(relativeResolvedPath))
    return relativeResolvedPath;
  if (QFile::exists(installResolvedPath))
    return installResolvedPath;
  // doesn't exist, just return relative
  return relativeResolvedPath;
}

static auto isValidExampleOrDemo(ExampleItem *item) -> bool
{
  QTC_ASSERT(item, return false);
  static QString invalidPrefix = QLatin1String("qthelp:////"); /* means that the qthelp url
                                                                    doesn't have any namespace */
  QString reason;
  auto ok = true;
  if (!item->hasSourceCode || !QFileInfo::exists(item->projectPath)) {
    ok = false;
    reason = QString::fromLatin1("projectPath \"%1\" empty or does not exist").arg(item->projectPath);
  } else if (item->image_url.startsWith(invalidPrefix) || !QUrl(item->image_url).isValid()) {
    ok = false;
    reason = QString::fromLatin1("imageUrl \"%1\" not valid").arg(item->image_url);
  } else if (!item->docUrl.isEmpty() && (item->docUrl.startsWith(invalidPrefix) || !QUrl(item->docUrl).isValid())) {
    ok = false;
    reason = QString::fromLatin1("docUrl \"%1\" non-empty but not valid").arg(item->docUrl);
  }
  if (!ok) {
    item->tags.append(QLatin1String("broken"));
    if (debugExamples())
      qWarning() << QString::fromLatin1("ERROR: Item \"%1\" broken: %2").arg(item->name, reason);
  }
  if (debugExamples() && item->description.isEmpty())
    qWarning() << QString::fromLatin1("WARNING: Item \"%1\" has no description").arg(item->name);
  return ok || debugExamples();
}

auto ExamplesListModel::parseExamples(QXmlStreamReader *reader, const QString &projectsOffset, const QString &examplesInstallPath) -> void
{
  std::unique_ptr<ExampleItem> item;
  const QChar slash = QLatin1Char('/');
  while (!reader->atEnd()) {
    switch (reader->readNext()) {
    case QXmlStreamReader::StartElement:
      if (reader->name() == QLatin1String("example")) {
        item = std::make_unique<ExampleItem>();
        item->type = Example;
        auto attributes = reader->attributes();
        item->name = attributes.value(QLatin1String("name")).toString();
        item->projectPath = attributes.value(QLatin1String("projectPath")).toString();
        item->hasSourceCode = !item->projectPath.isEmpty();
        item->projectPath = relativeOrInstallPath(item->projectPath, projectsOffset, examplesInstallPath);
        item->image_url = attributes.value(QLatin1String("imageUrl")).toString();
        QPixmapCache::remove(item->image_url);
        item->docUrl = attributes.value(QLatin1String("docUrl")).toString();
        item->isHighlighted = attributes.value(QLatin1String("isHighlighted")).toString() == QLatin1String("true");

      } else if (reader->name() == QLatin1String("fileToOpen")) {
        const auto mainFileAttribute = reader->attributes().value(QLatin1String("mainFile")).toString();
        const auto filePath = relativeOrInstallPath(reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement), projectsOffset, examplesInstallPath);
        item->filesToOpen.append(filePath);
        if (mainFileAttribute.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
          item->mainFile = filePath;
      } else if (reader->name() == QLatin1String("description")) {
        item->description = fixStringForTags(reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement));
      } else if (reader->name() == QLatin1String("dependency")) {
        item->dependencies.append(projectsOffset + slash + reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement));
      } else if (reader->name() == QLatin1String("tags")) {
        item->tags = trimStringList(reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).split(QLatin1Char(','), Qt::SkipEmptyParts));
      } else if (reader->name() == QLatin1String("platforms")) {
        item->platforms = trimStringList(reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).split(QLatin1Char(','), Qt::SkipEmptyParts));
      }
      break;
    case QXmlStreamReader::EndElement:
      if (reader->name() == QLatin1String("example")) {
        if (isValidExampleOrDemo(item.get()))
          m_items.push_back(item.release());
      } else if (reader->name() == QLatin1String("examples")) {
        return;
      }
      break;
    default: // nothing
      break;
    }
  }
}

auto ExamplesListModel::parseDemos(QXmlStreamReader *reader, const QString &projectsOffset, const QString &demosInstallPath) -> void
{
  std::unique_ptr<ExampleItem> item;
  const QChar slash = QLatin1Char('/');
  while (!reader->atEnd()) {
    switch (reader->readNext()) {
    case QXmlStreamReader::StartElement:
      if (reader->name() == QLatin1String("demo")) {
        item = std::make_unique<ExampleItem>();
        item->type = Demo;
        auto attributes = reader->attributes();
        item->name = attributes.value(QLatin1String("name")).toString();
        item->projectPath = attributes.value(QLatin1String("projectPath")).toString();
        item->hasSourceCode = !item->projectPath.isEmpty();
        item->projectPath = relativeOrInstallPath(item->projectPath, projectsOffset, demosInstallPath);
        item->image_url = attributes.value(QLatin1String("imageUrl")).toString();
        QPixmapCache::remove(item->image_url);
        item->docUrl = attributes.value(QLatin1String("docUrl")).toString();
        item->isHighlighted = attributes.value(QLatin1String("isHighlighted")).toString() == QLatin1String("true");
      } else if (reader->name() == QLatin1String("fileToOpen")) {
        item->filesToOpen.append(relativeOrInstallPath(reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement), projectsOffset, demosInstallPath));
      } else if (reader->name() == QLatin1String("description")) {
        item->description = fixStringForTags(reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement));
      } else if (reader->name() == QLatin1String("dependency")) {
        item->dependencies.append(projectsOffset + slash + reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement));
      } else if (reader->name() == QLatin1String("tags")) {
        item->tags = reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).split(QLatin1Char(','));
      }
      break;
    case QXmlStreamReader::EndElement:
      if (reader->name() == QLatin1String("demo")) {
        if (isValidExampleOrDemo(item.get()))
          m_items.push_back(item.release());
      } else if (reader->name() == QLatin1String("demos")) {
        return;
      }
      break;
    default: // nothing
      break;
    }
  }
}

auto ExamplesListModel::parseTutorials(QXmlStreamReader *reader, const QString &projectsOffset) -> void
{
  std::unique_ptr<ExampleItem> item;
  const QChar slash = QLatin1Char('/');
  while (!reader->atEnd()) {
    switch (reader->readNext()) {
    case QXmlStreamReader::StartElement:
      if (reader->name() == QLatin1String("tutorial")) {
        item = std::make_unique<ExampleItem>();
        item->type = Tutorial;
        auto attributes = reader->attributes();
        item->name = attributes.value(QLatin1String("name")).toString();
        item->projectPath = attributes.value(QLatin1String("projectPath")).toString();
        item->hasSourceCode = !item->projectPath.isEmpty();
        item->projectPath.prepend(slash);
        item->projectPath.prepend(projectsOffset);
        item->image_url = StyleHelper::dpiSpecificImageFile(attributes.value(QLatin1String("imageUrl")).toString());
        QPixmapCache::remove(item->image_url);
        item->docUrl = attributes.value(QLatin1String("docUrl")).toString();
        item->isVideo = attributes.value(QLatin1String("isVideo")).toString() == QLatin1String("true");
        item->videoUrl = attributes.value(QLatin1String("videoUrl")).toString();
        item->videoLength = attributes.value(QLatin1String("videoLength")).toString();
      } else if (reader->name() == QLatin1String("fileToOpen")) {
        item->filesToOpen.append(projectsOffset + slash + reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement));
      } else if (reader->name() == QLatin1String("description")) {
        item->description = fixStringForTags(reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement));
      } else if (reader->name() == QLatin1String("dependency")) {
        item->dependencies.append(projectsOffset + slash + reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement));
      } else if (reader->name() == QLatin1String("tags")) {
        item->tags = reader->readElementText(QXmlStreamReader::ErrorOnUnexpectedElement).split(QLatin1Char(','));
      }
      break;
    case QXmlStreamReader::EndElement:
      if (reader->name() == QLatin1String("tutorial"))
        m_items.push_back(item.release());
      else if (reader->name() == QLatin1String("tutorials"))
        return;
      break;
    default: // nothing
      break;
    }
  }
}

static auto resourcePath() -> QString
{
  // normalize paths so QML doesn't freak out if it's wrongly capitalized on Windows
  return Core::ICore::resourcePath().normalizedPathName().toString();
}

auto ExamplesListModel::updateExamples() -> void
{
  QString examplesInstallPath;
  QString demosInstallPath;

  auto sources = m_exampleSetModel.exampleSources(&examplesInstallPath, &demosInstallPath);

  beginResetModel();
  qDeleteAll(m_items);
  m_items.clear();

  foreach(const QString &exampleSource, sources) {
    QFile exampleFile(exampleSource);
    if (!exampleFile.open(QIODevice::ReadOnly)) {
      if (debugExamples())
        qWarning() << "ERROR: Could not open file" << exampleSource;
      continue;
    }

    QFileInfo fi(exampleSource);
    auto offsetPath = fi.path();
    QDir examplesDir(offsetPath);
    QDir demosDir(offsetPath);

    if (debugExamples())
      qWarning() << QString::fromLatin1("Reading file \"%1\"...").arg(fi.absoluteFilePath());
    QXmlStreamReader reader(&exampleFile);
    while (!reader.atEnd())
      switch (reader.readNext()) {
      case QXmlStreamReader::StartElement:
        if (reader.name() == QLatin1String("examples"))
          parseExamples(&reader, examplesDir.path(), examplesInstallPath);
        else if (reader.name() == QLatin1String("demos"))
          parseDemos(&reader, demosDir.path(), demosInstallPath);
        else if (reader.name() == QLatin1String("tutorials"))
          parseTutorials(&reader, examplesDir.path());
        break;
      default: // nothing
        break;
      }

    if (reader.hasError() && debugExamples()) {
      qWarning().noquote().nospace() << "ERROR: Could not parse file as XML document (" << exampleSource << "):" << reader.lineNumber() << ':' << reader.columnNumber() << ": " << reader.errorString();
    }
  }
  endResetModel();
}

auto ExamplesListModel::fetchPixmapAndUpdatePixmapCache(const QString &url) const -> QPixmap
{
  QPixmap pixmap;
  pixmap.load(url);
  if (pixmap.isNull())
    pixmap.load(resourcePath() + "/welcomescreen/widgets/" + url);
  if (pixmap.isNull()) {
    auto fetchedData = Core::HelpManager::fileData(url);
    if (!fetchedData.isEmpty()) {
      QBuffer imgBuffer(&fetchedData);
      imgBuffer.open(QIODevice::ReadOnly);
      QImageReader reader(&imgBuffer, QFileInfo(url).suffix().toLatin1());
      auto img = reader.read();
      img = ScreenshotCropper::croppedImage(img, url, ListModel::default_image_size);
      pixmap = QPixmap::fromImage(img);
    }
  }
  QPixmapCache::insert(url, pixmap);
  return pixmap;
}

auto ExampleSetModel::updateQtVersionList() -> void
{
  auto versions = QtVersionManager::sortVersions(QtVersionManager::versions([](const QtVersion *v) { return v->hasExamples() || v->hasDemos(); }));

  // prioritize default qt version
  const auto defaultKit = ProjectExplorer::KitManager::defaultKit();
  const auto defaultVersion = QtKitAspect::qtVersion(defaultKit);
  if (defaultVersion && versions.contains(defaultVersion))
    versions.move(versions.indexOf(defaultVersion), 0);

  recreateModel(versions);

  auto currentIndex = m_selectedExampleSetIndex;
  if (currentIndex < 0) // reset from settings
    currentIndex = readCurrentIndexFromSettings();

  const auto currentType = getType(currentIndex);

  if (currentType == InvalidExampleSet) {
    // select examples corresponding to 'highest' Qt version
    const auto highestQt = findHighestQtVersion(versions);
    currentIndex = indexForQtVersion(highestQt);
  } else if (currentType == QtExampleSet) {
    // try to select the previously selected Qt version, or
    // select examples corresponding to 'highest' Qt version
    const auto currentQtId = getQtId(currentIndex);
    auto newQtVersion = QtVersionManager::version(currentQtId);
    if (!newQtVersion)
      newQtVersion = findHighestQtVersion(versions);
    currentIndex = indexForQtVersion(newQtVersion);
  } // nothing to do for extra example sets
  // Make sure to select something even if the above failed
  if (currentIndex < 0 && rowCount() > 0)
    currentIndex = 0; // simply select first
  selectExampleSet(currentIndex);
  emit selectedExampleSetChanged(currentIndex);
}

auto ExampleSetModel::findHighestQtVersion(const QtVersions &versions) const -> QtVersion*
{
  QtVersion *newVersion = nullptr;
  for (const auto version : versions) {
    if (!newVersion) {
      newVersion = version;
    } else {
      if (version->qtVersion() > newVersion->qtVersion()) {
        newVersion = version;
      } else if (version->qtVersion() == newVersion->qtVersion() && version->uniqueId() < newVersion->uniqueId()) {
        newVersion = version;
      }
    }
  }

  if (!newVersion && !versions.isEmpty())
    newVersion = versions.first();

  return newVersion;
}

auto ExampleSetModel::exampleSources(QString *examplesInstallPath, QString *demosInstallPath) -> QStringList
{
  QStringList sources;

  // Qt Creator shipped tutorials
  sources << ":/qtsupport/qtcreator_tutorials.xml";

  QString examplesPath;
  QString demosPath;
  QString manifestScanPath;

  auto currentType = getType(m_selectedExampleSetIndex);
  if (currentType == ExtraExampleSetType) {
    auto index = getExtraExampleSetIndex(m_selectedExampleSetIndex);
    auto exampleSet = m_extraExampleSets.at(index);
    manifestScanPath = exampleSet.manifestPath;
    examplesPath = exampleSet.examplesPath;
    demosPath = exampleSet.examplesPath;
  } else if (currentType == QtExampleSet) {
    auto qtId = getQtId(m_selectedExampleSetIndex);
    foreach(QtVersion *version, QtVersionManager::versions()) {
      if (version->uniqueId() == qtId) {
        manifestScanPath = version->docsPath().toString();
        examplesPath = version->examplesPath().toString();
        demosPath = version->demosPath().toString();
        break;
      }
    }
  }
  if (!manifestScanPath.isEmpty()) {
    // search for examples-manifest.xml, demos-manifest.xml in <path>/*/
    auto dir = QDir(manifestScanPath);
    const QStringList examplesPattern(QLatin1String("examples-manifest.xml"));
    const QStringList demosPattern(QLatin1String("demos-manifest.xml"));
    QFileInfoList fis;
    foreach(QFileInfo subDir, dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      fis << QDir(subDir.absoluteFilePath()).entryInfoList(examplesPattern);
      fis << QDir(subDir.absoluteFilePath()).entryInfoList(demosPattern);
    }
    foreach(const QFileInfo &fi, fis)
      sources.append(fi.filePath());
  }
  if (examplesInstallPath)
    *examplesInstallPath = examplesPath;
  if (demosInstallPath)
    *demosInstallPath = demosPath;

  return sources;
}

auto prefixForItem(const ExampleItem *item) -> QString
{
  QTC_ASSERT(item, return {});
  if (item->isHighlighted)
    return QLatin1String("0000 ");
  return QString();
}

auto ExamplesListModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (!index.isValid() || index.row() >= m_items.count())
    return QVariant();

  const auto item = static_cast<ExampleItem*>(m_items.at(index.row()));
  switch (role) {
  case Qt::DisplayRole: // for search only
    return QString(prefixForItem(item) + item->name + ' ' + item->tags.join(' '));
  default:
    return ListModel::data(index, role);
  }
}

auto ExampleSetModel::selectExampleSet(int index) -> void
{
  if (index != m_selectedExampleSetIndex) {
    m_selectedExampleSetIndex = index;
    writeCurrentIdToSettings(m_selectedExampleSetIndex);
    if (getType(m_selectedExampleSetIndex) == QtExampleSet) {
      const auto selectedQtVersion = QtVersionManager::version(getQtId(m_selectedExampleSetIndex));
      m_selectedQtTypes = selectedQtVersion->targetDeviceTypes();
    }
    emit selectedExampleSetChanged(m_selectedExampleSetIndex);
  }
}

auto ExampleSetModel::qtVersionManagerLoaded() -> void
{
  m_qtVersionManagerInitialized = true;
  tryToInitialize();
}

auto ExampleSetModel::helpManagerInitialized() -> void
{
  m_helpManagerInitialized = true;
  tryToInitialize();
}

auto ExampleSetModel::tryToInitialize() -> void
{
  if (m_initalized)
    return;
  if (!m_qtVersionManagerInitialized)
    return;
  if (!m_helpManagerInitialized)
    return;

  m_initalized = true;

  connect(QtVersionManager::instance(), &QtVersionManager::qtVersionsChanged, this, &ExampleSetModel::updateQtVersionList);
  connect(ProjectExplorer::KitManager::instance(), &ProjectExplorer::KitManager::defaultkitChanged, this, &ExampleSetModel::updateQtVersionList);

  updateQtVersionList();
}

ExamplesListModelFilter::ExamplesListModelFilter(ExamplesListModel *sourceModel, bool showTutorialsOnly, QObject *parent) : ListModelFilter(sourceModel, parent), m_showTutorialsOnly(showTutorialsOnly), m_examplesListModel(sourceModel) {}

auto ExamplesListModelFilter::leaveFilterAcceptsRowBeforeFiltering(const Core::ListItem *item, bool *earlyExitResult) const ->  bool
{
  QTC_ASSERT(earlyExitResult, return false);

  const auto isTutorial = static_cast<const ExampleItem*>(item)->type == Tutorial;

  if (m_showTutorialsOnly) {
    *earlyExitResult = isTutorial;
    return !isTutorial;
  }

  if (isTutorial) {
    *earlyExitResult = false;
    return true;
  }

  if (m_examplesListModel->exampleSetModel()->selectedQtSupports(Android::Constants::ANDROID_DEVICE_TYPE) && !item->tags.contains("android")) {
    *earlyExitResult = false;
    return true;
  }

  if (m_examplesListModel->exampleSetModel()->selectedQtSupports(Ios::Constants::IOS_DEVICE_TYPE) && !item->tags.contains("ios")) {
    *earlyExitResult = false;
    return true;
  }

  return false;
}

} // namespace Internal
} // namespace QtSupport
