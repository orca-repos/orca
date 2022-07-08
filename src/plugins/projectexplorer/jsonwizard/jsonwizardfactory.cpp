// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonwizardfactory.hpp"

#include "jsonwizard.hpp"
#include "jsonwizardgeneratorfactory.hpp"
#include "jsonwizardpagefactory.hpp"

#include "../projectexplorerconstants.hpp"

#include <core/coreconstants.hpp>
#include <core/icontext.hpp>
#include <core/icore.hpp>
#include <core/jsexpander.hpp>
#include <core/messagemanager.hpp>

#include <extensionsystem/pluginmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>
#include <utils/wizard.hpp>
#include <utils/wizardpage.hpp>

#include <QDebug>
#include <QDir>
#include <QJSEngine>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QUuid>

using namespace Utils;

namespace ProjectExplorer {

constexpr char WIZARD_PATH[] = "templates/wizards";
constexpr char WIZARD_FILE[] = "wizard.json";
constexpr char VERSION_KEY[] = "version";
constexpr char ENABLED_EXPRESSION_KEY[] = "enabled";
constexpr char KIND_KEY[] = "kind";
constexpr char SUPPORTED_PROJECTS[] = "supportedProjectTypes";
constexpr char ID_KEY[] = "id";
constexpr char CATEGORY_KEY[] = "category";
constexpr char CATEGORY_NAME_KEY[] = "trDisplayCategory";
constexpr char DISPLAY_NAME_KEY[] = "trDisplayName";
constexpr char ICON_KEY[] = "icon";
constexpr char ICON_TEXT_KEY[] = "iconText";
constexpr char FONT_ICON_NAME_KEY[] = "fontIconName";
constexpr char IMAGE_KEY[] = "image";
constexpr char ICON_KIND_KEY[] = "iconKind";
constexpr char DESCRIPTION_KEY[] = "trDescription";
constexpr char REQUIRED_FEATURES_KEY[] = "featuresRequired";
constexpr char SUGGESTED_FEATURES_KEY[] = "featuresSuggested";
constexpr char GENERATOR_KEY[] = "generators";
constexpr char PAGES_KEY[] = "pages";
constexpr char TYPE_ID_KEY[] = "typeId";
constexpr char DATA_KEY[] = "data";
constexpr char PAGE_SUB_TITLE_KEY[] = "trSubTitle";
constexpr char PAGE_SHORT_TITLE_KEY[] = "trShortTitle";
constexpr char PAGE_INDEX_KEY[] = "index";
constexpr char OPTIONS_KEY[] = "options";
constexpr char PLATFORM_INDEPENDENT_KEY[] = "platformIndependent";
constexpr char DEFAULT_VALUES[] = "defaultValues";

static QList<JsonWizardPageFactory *> s_pageFactories;
static QList<JsonWizardGeneratorFactory *> s_generatorFactories;

int JsonWizardFactory::m_verbose = 0;

// Return locale language attribute "de_UTF8" -> "de", empty string for "C"
static auto languageSetting() -> QString
{
  auto name = Core::ICore::userInterfaceLanguage();
  const int underScorePos = name.indexOf(QLatin1Char('_'));
  if (underScorePos != -1)
    name.truncate(underScorePos);
  if (name.compare(QLatin1String("C"), Qt::CaseInsensitive) == 0)
    name.clear();
  return name;
}

template <class T>
static auto supportedTypeIds(const QList<T*> &factories) -> QString
{
  QStringList tmp;
  for (const T *f : factories) {
    foreach(Id i, f->supportedIds())
      tmp.append(i.toString());
  }
  return tmp.join(QLatin1String("', '"));
}

static auto parseGenerator(const QVariant &value, QString *errorMessage) -> JsonWizardFactory::Generator
{
  JsonWizardFactory::Generator gen;

  if (value.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizardFactory", "Generator is not a object.");
    return gen;
  }

  const auto data = value.toMap();
  const auto strVal = data.value(QLatin1String(TYPE_ID_KEY)).toString();
  if (strVal.isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizardFactory", "Generator has no typeId set.");
    return gen;
  }
  auto typeId = Id::fromString(QLatin1String(Constants::GENERATOR_ID_PREFIX) + strVal);
  const auto factory = findOr(s_generatorFactories, nullptr, [typeId](JsonWizardGeneratorFactory *f) { return f->canCreate(typeId); });
  if (!factory) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizardFactory", "TypeId \"%1\" of generator is unknown. Supported typeIds are: \"%2\".").arg(strVal).arg(supportedTypeIds(s_generatorFactories).replace(QLatin1String(Constants::GENERATOR_ID_PREFIX), QLatin1String("")));
    return gen;
  }

  const auto varVal = data.value(QLatin1String(DATA_KEY));
  if (!factory->validateData(typeId, varVal, errorMessage))
    return gen;

  gen.typeId = typeId;
  gen.data = varVal;

  return gen;
}

//FIXME: createWizardFactories() has an almost identical loop. Make the loop return the results instead of
//internal processing and create a separate function for it. Then process the results in
//loadDefaultValues() and createWizardFactories()
auto JsonWizardFactory::loadDefaultValues(const QString &fileName) -> QVariantMap
{
  QString verboseLog;

  if (fileName.isEmpty()) {
    return {};
  }

  QList<IWizardFactory*> result;
  foreach(const Utils::FilePath &path, searchPaths()) {
    if (path.isEmpty())
      continue;

    auto dir = FilePath::fromString(path.toString());
    if (!dir.exists()) {
      if (verbose())
        verboseLog.append(tr("Path \"%1\" does not exist when checking Json wizard search paths.\n").arg(path.toUserOutput()));
      continue;
    }

    const auto filters = QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot;
    auto dirs = dir.dirEntries(filters);

    while (!dirs.isEmpty()) {
      const auto current = dirs.takeFirst();
      if (verbose())
        verboseLog.append(tr("Checking \"%1\" for %2.\n").arg(QDir::toNativeSeparators(current.absolutePath().toString())).arg(fileName));
      if (current.pathAppended(fileName).exists()) {
        QFile configFile(current.pathAppended(fileName).toString());
        configFile.open(QIODevice::ReadOnly);
        QJsonParseError error;
        const auto fileData = configFile.readAll();
        const auto json = QJsonDocument::fromJson(fileData, &error);
        configFile.close();

        if (error.error != QJsonParseError::NoError) {
          auto line = 1;
          auto column = 1;
          for (auto i = 0; i < error.offset; ++i) {
            if (fileData.at(i) == '\n') {
              ++line;
              column = 1;
            } else {
              ++column;
            }
          }
          verboseLog.append(tr("* Failed to parse \"%1\":%2:%3: %4\n").arg(configFile.fileName()).arg(line).arg(column).arg(error.errorString()));
          continue;
        }

        if (!json.isObject()) {
          verboseLog.append(tr("* Did not find a JSON object in \"%1\".\n").arg(configFile.fileName()));
          continue;
        }

        if (verbose())
          verboseLog.append(tr("* Configuration found and parsed.\n"));

        return json.object().toVariantMap();
      }
      auto subDirs = current.dirEntries(filters);
      if (!subDirs.isEmpty()) {
        // There is no QList::prepend(QList)...
        dirs.swap(subDirs);
        dirs.append(subDirs);
      } else if (verbose()) {
        verboseLog.append(tr("JsonWizard: \"%1\" not found\n").arg(fileName));
      }
    }
  }

  if (verbose()) {
    // Print to output pane for Windows.
    qWarning("%s", qPrintable(verboseLog));
    Core::MessageManager::writeDisrupting(verboseLog);
  }

  return {};
}

auto JsonWizardFactory::mergeDataValueMaps(const QVariant &valueMap, const QVariant &defaultValueMap) -> QVariant
{
  QVariantMap retVal;

  #if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    const QVariantMap &map = defaultValueMap.toMap();
    for (auto it = map.begin(), end = map.end(); it != end; ++it)
        retVal.insert(it.key(), it.value());

    const QVariantMap &map2 = valueMap.toMap();
    for (auto it = map2.begin(), end = map2.end(); it != end; ++it)
        retVal.insert(it.key(), it.value());
  #else
  retVal.insert(defaultValueMap.toMap());
  retVal.insert(valueMap.toMap());
  #endif
  return retVal;
}

auto JsonWizardFactory::getDataValue(const QLatin1String &key, const QVariantMap &valueSet, const QVariantMap &defaultValueSet, const QVariant &notExistValue) -> QVariant
{
  QVariant retVal = {};

  if ((valueSet.contains(key) && valueSet.value(key).type() == QVariant::Map) || (defaultValueSet.contains(key) && defaultValueSet.value(key).type() == QVariant::Map)) {
    retVal = mergeDataValueMaps(valueSet.value(key), defaultValueSet.value(key));
  } else {
    const auto defaultValue = defaultValueSet.value(key, notExistValue);
    retVal = valueSet.value(key, defaultValue);
  }

  return retVal;
}

auto JsonWizardFactory::screenSizeInfoFromPage(const QString &pageType) const -> std::pair<int, QStringList>
{
  /* Retrieving the ScreenFactor "trKey" values from pages[i]/data[j]/data["items"], where
   * pages[i] is the page of type `pageType` and data[j] is the data item with name ScreenFactor
  */

  const auto id = Id::fromString(Constants::PAGE_ID_PREFIX + pageType);

  const auto it = std::find_if(std::cbegin(m_pages), std::cend(m_pages), [&id](const Page &page) {
    return page.typeId == id;
  });

  if (it == std::cend(m_pages))
    return {};

  const auto data = it->data;
  if (data.type() != QVariant::List)
    return {};

  const auto screenFactorField = findOrDefault(data.toList(), [](const QVariant &field) {
    const auto m = field.toMap();
    return "ScreenFactor" == m["name"];
  });

  if (screenFactorField.type() != QVariant::Map)
    return {};

  const auto screenFactorData = screenFactorField.toMap()["data"];
  if (screenFactorData.type() != QVariant::Map)
    return {};

  const auto screenFactorDataMap = screenFactorData.toMap();
  if (!screenFactorDataMap.contains("items"))
    return {};

  auto ok = false;
  const auto index = screenFactorDataMap["index"].toInt(&ok);
  const auto items = screenFactorDataMap["items"].toList();
  if (items.isEmpty())
    return {};

  auto values = transform(items, [](const QVariant &item) {
    const auto m = item.toMap();
    return m["trKey"].toString();
  });

  if (values.isEmpty())
    return {};

  return std::make_pair(index, values);
}

auto JsonWizardFactory::parsePage(const QVariant &value, QString *errorMessage) -> Page
{
  Page p;

  if (value.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizardFactory", "Page is not an object.");
    return p;
  }

  const auto data = value.toMap();
  auto defaultValueFile = data.value(QLatin1String(DEFAULT_VALUES)).toString();
  if (!defaultValueFile.isEmpty())
    defaultValueFile.append(QLatin1String(".json"));
  const auto defaultData = loadDefaultValues(defaultValueFile);

  const auto strVal = getDataValue(QLatin1String(TYPE_ID_KEY), data, defaultData).toString();
  if (strVal.isEmpty()) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizardFactory", "Page has no typeId set.");
    return p;
  }
  auto typeId = Id::fromString(QLatin1String(Constants::PAGE_ID_PREFIX) + strVal);

  auto factory = findOr(s_pageFactories, nullptr, [typeId](JsonWizardPageFactory *f) { return f->canCreate(typeId); });
  if (!factory) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizardFactory", "TypeId \"%1\" of page is unknown. Supported typeIds are: \"%2\".").arg(strVal).arg(supportedTypeIds(s_pageFactories).replace(QLatin1String(Constants::PAGE_ID_PREFIX), QLatin1String("")));
    return p;
  }

  const auto title = localizedString(getDataValue(QLatin1String(DISPLAY_NAME_KEY), data, defaultData));
  const auto subTitle = localizedString(getDataValue(QLatin1String(PAGE_SUB_TITLE_KEY), data, defaultData));
  const auto shortTitle = localizedString(getDataValue(QLatin1String(PAGE_SHORT_TITLE_KEY), data, defaultData));

  bool ok;
  auto index = getDataValue(QLatin1String(PAGE_INDEX_KEY), data, defaultData, -1).toInt(&ok);
  if (!ok) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizardFactory", "Page with typeId \"%1\" has invalid \"index\".").arg(typeId.toString());
    return p;
  }

  auto enabled = getDataValue(QLatin1String(ENABLED_EXPRESSION_KEY), data, defaultData, true);

  auto specifiedSubData = data.value(QLatin1String(DATA_KEY));
  auto defaultSubData = defaultData.value(QLatin1String(DATA_KEY));
  QVariant subData;

  if (specifiedSubData.isNull())
    subData = defaultSubData;
  else if (specifiedSubData.type() == QVariant::Map)
    subData = mergeDataValueMaps(specifiedSubData.toMap(), defaultSubData.toMap());
  else if (specifiedSubData.type() == QVariant::List)
    subData = specifiedSubData;

  if (!factory->validateData(typeId, subData, errorMessage))
    return p;

  p.typeId = typeId;
  p.title = title;
  p.subTitle = subTitle;
  p.shortTitle = shortTitle;
  p.index = index;
  p.data = subData;
  p.enabled = enabled;

  return p;
}

//FIXME: loadDefaultValues() has an almost identical loop. Make the loop return the results instead of
//internal processing and create a separate function for it. Then process the results in
//loadDefaultValues() and loadDefaultValues()
auto JsonWizardFactory::createWizardFactories() -> QList<IWizardFactory*>
{
  QString errorMessage;
  QString verboseLog;
  const QString wizardFileName = QLatin1String(WIZARD_FILE);

  QList<IWizardFactory*> result;
  foreach(const FilePath &path, searchPaths()) {
    if (path.isEmpty())
      continue;

    if (!path.exists()) {
      if (verbose())
        verboseLog.append(tr("Path \"%1\" does not exist when checking Json wizard search paths.\n").arg(path.toUserOutput()));
      continue;
    }

    const FileFilter filter{{}, QDir::Dirs | QDir::Readable | QDir::NoDotAndDotDot, QDirIterator::NoIteratorFlags};
    const auto sortflags = QDir::Name | QDir::IgnoreCase;
    auto dirs = path.dirEntries(filter, sortflags);

    while (!dirs.isEmpty()) {
      const auto currentDir = dirs.takeFirst();
      if (verbose())
        verboseLog.append(tr("Checking \"%1\" for %2.\n").arg(currentDir.toUserOutput()).arg(wizardFileName));
      const auto currentFile = currentDir / wizardFileName;
      if (currentFile.exists()) {
        QJsonParseError error;
        const auto fileData = currentFile.fileContents();
        const auto json = QJsonDocument::fromJson(fileData, &error);

        if (error.error != QJsonParseError::NoError) {
          auto line = 1;
          auto column = 1;
          for (auto i = 0; i < error.offset; ++i) {
            if (fileData.at(i) == '\n') {
              ++line;
              column = 1;
            } else {
              ++column;
            }
          }
          verboseLog.append(tr("* Failed to parse \"%1\":%2:%3: %4\n").arg(currentFile.fileName()).arg(line).arg(column).arg(error.errorString()));
          continue;
        }

        if (!json.isObject()) {
          verboseLog.append(tr("* Did not find a JSON object in \"%1\".\n").arg(currentFile.fileName()));
          continue;
        }

        if (verbose())
          verboseLog.append(tr("* Configuration found and parsed.\n"));

        auto data = json.object().toVariantMap();

        auto version = data.value(QLatin1String(VERSION_KEY), 0).toInt();
        if (version < 1 || version > 1) {
          verboseLog.append(tr("* Version %1 not supported.\n").arg(version));
          continue;
        }

        auto factory = createWizardFactory(data, currentDir, &errorMessage);
        if (!factory) {
          verboseLog.append(tr("* Failed to create: %1\n").arg(errorMessage));
          continue;
        }

        result << factory;
      } else {
        auto subDirs = currentDir.dirEntries(filter, sortflags);
        if (!subDirs.isEmpty()) {
          // There is no QList::prepend(QList)...
          dirs.swap(subDirs);
          dirs.append(subDirs);
        } else if (verbose()) {
          verboseLog.append(tr("JsonWizard: \"%1\" not found\n").arg(wizardFileName));
        }
      }
    }
  }

  if (verbose()) {
    // Print to output pane for Windows.
    qWarning("%s", qPrintable(verboseLog));
    Core::MessageManager::writeDisrupting(verboseLog);
  }

  return result;
}

auto JsonWizardFactory::createWizardFactory(const QVariantMap &data, const FilePath &baseDir, QString *errorMessage) -> JsonWizardFactory*
{
  auto *factory = new JsonWizardFactory;
  if (!factory->initialize(data, baseDir, errorMessage)) {
    delete factory;
    factory = nullptr;
  }
  return factory;
}

static auto environmentTemplatesPaths() -> QStringList
{
  QStringList paths;

  const auto envTempPath = QString::fromLocal8Bit(qgetenv("QTCREATOR_TEMPLATES_PATH"));

  if (!envTempPath.isEmpty()) {
    for (const auto &path : envTempPath.split(HostOsInfo::pathListSeparator(), Qt::SkipEmptyParts)) {
      auto canonicalPath = QDir(path).canonicalPath();
      if (!canonicalPath.isEmpty() && !paths.contains(canonicalPath))
        paths.append(canonicalPath);
    }
  }

  return paths;
}

auto JsonWizardFactory::searchPaths() -> FilePaths&
{
  static FilePaths m_searchPaths = {Core::ICore::userResourcePath(WIZARD_PATH), Core::ICore::resourcePath(WIZARD_PATH)};
  for (const auto &environmentTemplateDirName : environmentTemplatesPaths())
    m_searchPaths << FilePath::fromString(environmentTemplateDirName);

  return m_searchPaths;
}

auto JsonWizardFactory::addWizardPath(const FilePath &path) -> void
{
  searchPaths().append(path);
}

auto JsonWizardFactory::clearWizardPaths() -> void
{
  searchPaths().clear();
}

auto JsonWizardFactory::setVerbose(int level) -> void
{
  m_verbose = level;
}

auto JsonWizardFactory::verbose() -> int
{
  return m_verbose;
}

auto JsonWizardFactory::registerPageFactory(JsonWizardPageFactory *factory) -> void
{
  QTC_ASSERT(!s_pageFactories.contains(factory), return);
  s_pageFactories.append(factory);
}

auto JsonWizardFactory::registerGeneratorFactory(JsonWizardGeneratorFactory *factory) -> void
{
  QTC_ASSERT(!s_generatorFactories.contains(factory), return);
  s_generatorFactories.append(factory);
}

static auto qmlProjectName(const FilePath &folder) -> QString
{
  auto currentFolder = folder;
  while (!currentFolder.isEmpty()) {
    const auto fileList = currentFolder.dirEntries({{"*.qmlproject"}});
    if (!fileList.isEmpty())
      return fileList.first().baseName();
    currentFolder = currentFolder.parentDir();
  }

  return {};
}

auto JsonWizardFactory::runWizardImpl(const FilePath &path, QWidget *parent, Id platform, const QVariantMap &variables, bool showWizard) -> Wizard*
{
  const auto wizard = new JsonWizard(parent);
  wizard->setWindowIcon(icon());
  wizard->setWindowTitle(displayName());

  wizard->setValue(QStringLiteral("WizardDir"), m_wizardDir.toVariant());
  auto tmp = requiredFeatures();
  tmp.subtract(pluginFeatures());
  wizard->setValue(QStringLiteral("RequiredFeatures"), Id::toStringList(tmp));
  tmp = m_preferredFeatures;
  tmp.subtract(pluginFeatures());
  wizard->setValue(QStringLiteral("PreferredFeatures"), Id::toStringList(tmp));

  wizard->setValue(QStringLiteral("Features"), Id::toStringList(availableFeatures(platform)));
  wizard->setValue(QStringLiteral("Plugins"), Id::toStringList(pluginFeatures()));

  // Add data to wizard:
  for (auto i = variables.constBegin(); i != variables.constEnd(); ++i)
    wizard->setValue(i.key(), i.value());

  wizard->setValue(QStringLiteral("InitialPath"), path.toString());
  wizard->setValue(QStringLiteral("QmlProjectName"), qmlProjectName(path));
  wizard->setValue(QStringLiteral("Platform"), platform.toString());

  QString kindStr = QLatin1String(Core::Constants::WIZARD_KIND_UNKNOWN);
  if (kind() == FileWizard)
    kindStr = QLatin1String(Core::Constants::WIZARD_KIND_FILE);
  else if (kind() == ProjectWizard)
    kindStr = QLatin1String(Core::Constants::WIZARD_KIND_PROJECT);
  wizard->setValue(QStringLiteral("kind"), kindStr);

  wizard->setValue(QStringLiteral("trDescription"), description());
  wizard->setValue(QStringLiteral("trDisplayName"), displayName());
  wizard->setValue(QStringLiteral("trDisplayCategory"), displayCategory());
  wizard->setValue(QStringLiteral("category"), category());
  wizard->setValue(QStringLiteral("id"), id().toString());

  const auto expander = wizard->expander();
  for (const auto &od : qAsConst(m_options)) {
    if (od.condition(*expander))
      wizard->setValue(od.key(), od.value(*expander));
  }

  auto havePage = false;
  for (const auto &data : qAsConst(m_pages)) {
    QTC_ASSERT(data.isValid(), continue);

    if (!JsonWizard::boolFromVariant(data.enabled, wizard->expander()))
      continue;

    havePage = true;
    const auto factory = findOr(s_pageFactories, nullptr, [&data](JsonWizardPageFactory *f) {
      return f->canCreate(data.typeId);
    });
    QTC_ASSERT(factory, continue);
    const auto page = factory->create(wizard, data.typeId, data.data);
    QTC_ASSERT(page, continue);

    page->setTitle(data.title);
    page->setSubTitle(data.subTitle);
    page->setProperty(SHORT_TITLE_PROPERTY, data.shortTitle);

    if (data.index >= 0) {
      wizard->setPage(data.index, page);
      if (wizard->page(data.index) != page) // Failed to set page!
        delete page;
    } else {
      wizard->addPage(page);
    }
  }

  for (const auto &data : qAsConst(m_generators)) {
    QTC_ASSERT(data.isValid(), continue);
    const auto factory = findOr(s_generatorFactories, nullptr, [&data](JsonWizardGeneratorFactory *f) {
      return f->canCreate(data.typeId);
    });
    QTC_ASSERT(factory, continue);
    const auto gen = factory->create(data.typeId, data.data, path.toString(), platform, variables);
    QTC_ASSERT(gen, continue);

    wizard->addGenerator(gen);
  }

  if (!havePage) {
    wizard->accept();
    wizard->deleteLater();
    return nullptr;
  }

  if (showWizard)
    wizard->show();
  return wizard;
}

auto JsonWizardFactory::objectOrList(const QVariant &data, QString *errorMessage) -> QList<QVariant>
{
  QList<QVariant> result;
  if (data.isNull())
    *errorMessage = tr("key not found.");
  else if (data.type() == QVariant::Map)
    result.append(data);
  else if (data.type() == QVariant::List)
    result = data.toList();
  else
    *errorMessage = tr("Expected an object or a list.");
  return result;
}

auto JsonWizardFactory::localizedString(const QVariant &value) -> QString
{
  if (value.isNull())
    return QString();
  if (value.type() == QVariant::Map) {
    const auto tmp = value.toMap();
    const auto locale = languageSetting().toLower();
    QStringList locales;
    locales << locale << QLatin1String("en") << QLatin1String("C") << tmp.keys();
    for (const auto &locale : qAsConst(locales)) {
      auto result = tmp.value(locale, QString()).toString();
      if (!result.isEmpty())
        return result;
    }
    return QString();
  }
  return QCoreApplication::translate("ProjectExplorer::JsonWizard", value.toByteArray());
}

auto JsonWizardFactory::isAvailable(Id platformId) const -> bool
{
  if (!IWizardFactory::isAvailable(platformId)) // check for required features
    return false;

  MacroExpander expander;
  auto e = &expander;
  expander.registerVariable("Platform", tr("The platform selected for the wizard."), [platformId]() { return platformId.toString(); });
  expander.registerVariable("Features", tr("The features available to this wizard."), [e, platformId]() { return JsonWizard::stringListToArrayString(Id::toStringList(availableFeatures(platformId)), e); });
  expander.registerVariable("Plugins", tr("The plugins loaded."), [e]() {
    return JsonWizard::stringListToArrayString(Id::toStringList(pluginFeatures()), e);
  });
  const Core::JsExpander jsExpander;
  jsExpander.registerObject("Wizard", new Internal::JsonWizardFactoryJsExtension(platformId, availableFeatures(platformId), pluginFeatures()));
  jsExpander.engine().evaluate("var value = Wizard.value");
  jsExpander.registerForExpander(e);
  return JsonWizard::boolFromVariant(m_enabledExpression, &expander);
}

auto JsonWizardFactory::destroyAllFactories() -> void
{
  qDeleteAll(s_pageFactories);
  s_pageFactories.clear();
  qDeleteAll(s_generatorFactories);
  s_generatorFactories.clear();
}

auto JsonWizardFactory::initialize(const QVariantMap &data, const FilePath &baseDir, QString *errorMessage) -> bool
{
  QTC_ASSERT(errorMessage, return false);

  errorMessage->clear();

  m_wizardDir = baseDir.absoluteFilePath();

  m_enabledExpression = data.value(QLatin1String(ENABLED_EXPRESSION_KEY), true);

  auto projectTypes = Id::fromStringList(data.value(QLatin1String(SUPPORTED_PROJECTS)).toStringList());
  // FIXME: "kind" was relevant up to and including Qt Creator 3.6:
  const auto unsetKind = QUuid::createUuid().toString();
  auto strVal = data.value(QLatin1String(KIND_KEY), unsetKind).toString();
  if (strVal != unsetKind && strVal != QLatin1String("class") && strVal != QLatin1String("file") && strVal != QLatin1String("project")) {
    *errorMessage = tr("\"kind\" value \"%1\" is not \"class\" (deprecated), \"file\" or \"project\".").arg(strVal);
    return false;
  }
  if ((strVal == QLatin1String("file") || strVal == QLatin1String("class")) && !projectTypes.isEmpty()) {
    *errorMessage = tr("\"kind\" is \"file\" or \"class\" (deprecated) and \"%1\" is also set.").arg(QLatin1String(SUPPORTED_PROJECTS));
    return false;
  }
  if (strVal == QLatin1String("project") && projectTypes.isEmpty())
    projectTypes.insert("UNKNOWN_PROJECT");
  // end of legacy code
  setSupportedProjectTypes(projectTypes);

  strVal = data.value(QLatin1String(ID_KEY)).toString();
  if (strVal.isEmpty()) {
    *errorMessage = tr("No id set.");
    return false;
  }
  setId(Id::fromString(strVal));

  strVal = data.value(QLatin1String(CATEGORY_KEY)).toString();
  if (strVal.isEmpty()) {
    *errorMessage = tr("No category is set.");
    return false;
  }
  setCategory(strVal);

  strVal = data.value(QLatin1String(ICON_KEY)).toString();
  const auto iconPath = baseDir.resolvePath(strVal);
  if (!iconPath.exists()) {
    *errorMessage = tr("Icon file \"%1\" not found.").arg(iconPath.toUserOutput());
    return false;
  }
  const auto iconText = data.value(QLatin1String(ICON_TEXT_KEY)).toString();
  const auto iconIsThemed = data.value(QLatin1String(ICON_KIND_KEY)).toString().compare("Themed", Qt::CaseInsensitive) == 0;
  setIcon(iconIsThemed ? themedIcon(iconPath) : strVal.isEmpty() ? QIcon() : QIcon(iconPath.toString()), iconText);

  const auto fontIconName = data.value(QLatin1String(FONT_ICON_NAME_KEY)).toString();
  setFontIconName(fontIconName);

  strVal = data.value(QLatin1String(IMAGE_KEY)).toString();
  if (!strVal.isEmpty()) {
    const auto imagePath = baseDir.resolvePath(strVal);
    if (!imagePath.exists()) {
      *errorMessage = tr("Image file \"%1\" not found.").arg(imagePath.toUserOutput());
      return false;
    }
    setDescriptionImage(imagePath.toString());
  }

  const auto detailsPage = baseDir.resolvePath(QString("detailsPage.qml"));
  if (detailsPage.exists())
    setDetailsPageQmlPath(detailsPage.toString());

  setRequiredFeatures(Id::fromStringList(data.value(QLatin1String(REQUIRED_FEATURES_KEY)).toStringList()));
  m_preferredFeatures = Id::fromStringList(data.value(QLatin1String(SUGGESTED_FEATURES_KEY)).toStringList());
  m_preferredFeatures.unite(requiredFeatures());

  strVal = localizedString(data.value(QLatin1String(DISPLAY_NAME_KEY)));
  if (strVal.isEmpty()) {
    *errorMessage = tr("No displayName set.");
    return false;
  }
  setDisplayName(strVal);

  strVal = localizedString(data.value(QLatin1String(CATEGORY_NAME_KEY)));
  if (strVal.isEmpty()) {
    *errorMessage = tr("No displayCategory set.");
    return false;
  }
  setDisplayCategory(strVal);

  strVal = localizedString(data.value(QLatin1String(DESCRIPTION_KEY)));
  if (strVal.isEmpty()) {
    *errorMessage = tr("No description set.");
    return false;
  }
  setDescription(strVal);

  // Generator:
  auto list = objectOrList(data.value(QLatin1String(GENERATOR_KEY)), errorMessage);
  if (!errorMessage->isEmpty()) {
    *errorMessage = tr("When parsing \"generators\": %1").arg(*errorMessage);
    return false;
  }

  for (const auto &v : qAsConst(list)) {
    auto gen = parseGenerator(v, errorMessage);
    if (gen.isValid())
      m_generators.append(gen);
    else
      return false;
  }

  // Pages:
  list = objectOrList(data.value(QLatin1String(PAGES_KEY)), errorMessage);
  if (!errorMessage->isEmpty()) {
    *errorMessage = tr("When parsing \"pages\": %1").arg(*errorMessage);
    return false;
  }

  for (const auto &v : qAsConst(list)) {
    auto p = parsePage(v, errorMessage);
    if (p.isValid())
      m_pages.append(p);
    else
      return false;
  }

  WizardFlags flags;
  if (data.value(QLatin1String(PLATFORM_INDEPENDENT_KEY), false).toBool())
    flags |= PlatformIndependent;
  setFlags(flags);

  // Options:
  m_options = JsonWizard::parseOptions(data.value(QLatin1String(OPTIONS_KEY)), errorMessage);
  return errorMessage->isEmpty();
}

namespace Internal {

JsonWizardFactoryJsExtension::JsonWizardFactoryJsExtension(Id platformId, const QSet<Id> &availableFeatures, const QSet<Id> &pluginFeatures) : m_platformId(platformId), m_availableFeatures(availableFeatures), m_pluginFeatures(pluginFeatures) {}

auto JsonWizardFactoryJsExtension::value(const QString &name) const -> QVariant
{
  if (name == "Platform")
    return m_platformId.toString();
  if (name == "Features")
    return Id::toStringList(m_availableFeatures);
  if (name == "Plugins")
    return Id::toStringList(m_pluginFeatures);
  return QVariant();
}

} // namespace Internal
} // namespace ProjectExplorer
