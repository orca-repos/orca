// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include "jsonwizard.hpp"

#include <core/iwizardfactory.hpp>

#include <utils/filepath.hpp>

#include <QVariant>

namespace ProjectExplorer {

class JsonWizardFactory;
class JsonWizardPageFactory;
class JsonWizardGeneratorFactory;
class ProjectExplorerPlugin;
class ProjectExplorerPluginPrivate;

class PROJECTEXPLORER_EXPORT JsonWizardFactory : public Core::IWizardFactory {
  Q_OBJECT

public:
  // Add search paths for wizard.json files. All subdirs are going to be checked.
  static auto addWizardPath(const Utils::FilePath &path) -> void;
  static auto clearWizardPaths() -> void;

  // actual interface of the wizard factory:
  class Generator {
  public:
    auto isValid() const -> bool { return typeId.isValid(); }

    Utils::Id typeId;
    QVariant data;
  };

  class Page {
  public:
    auto isValid() const -> bool { return typeId.isValid(); }
    QString title;
    QString subTitle;
    QString shortTitle;
    int index = -1; // page index in the wizard
    Utils::Id typeId;
    QVariant enabled;
    QVariant data;
  };

  static auto registerPageFactory(JsonWizardPageFactory *factory) -> void;
  static auto registerGeneratorFactory(JsonWizardGeneratorFactory *factory) -> void;
  static auto objectOrList(const QVariant &data, QString *errorMessage) -> QList<QVariant>;
  static auto localizedString(const QVariant &value) -> QString;
  auto isAvailable(Utils::Id platformId) const -> bool override;
  virtual auto screenSizeInfoFromPage(const QString &pageType) const -> std::pair<int, QStringList>;

private:
  auto runWizardImpl(const Utils::FilePath &path, QWidget *parent, Utils::Id platform, const QVariantMap &variables, bool showWizard = true) -> Utils::Wizard* override;

  // Create all wizards. As other plugins might register factories for derived
  // classes. Called when the new file dialog is shown for the first time.
  static auto createWizardFactories() -> QList<IWizardFactory*>;
  static auto createWizardFactory(const QVariantMap &data, const Utils::FilePath &baseDir, QString *errorMessage) -> JsonWizardFactory*;
  static auto searchPaths() -> Utils::FilePaths&;
  static auto setVerbose(int level) -> void;
  static auto verbose() -> int;
  static auto destroyAllFactories() -> void;
  auto initialize(const QVariantMap &data, const Utils::FilePath &baseDir, QString *errorMessage) -> bool;
  auto parsePage(const QVariant &value, QString *errorMessage) -> Page;
  auto loadDefaultValues(const QString &fileName) -> QVariantMap;
  auto getDataValue(const QLatin1String &key, const QVariantMap &valueSet, const QVariantMap &defaultValueSet, const QVariant &notExistValue = {}) -> QVariant;
  auto mergeDataValueMaps(const QVariant &valueMap, const QVariant &defaultValueMap) -> QVariant;

  QVariant m_enabledExpression;
  Utils::FilePath m_wizardDir;
  QList<Generator> m_generators;
  QList<Page> m_pages;
  QList<JsonWizard::OptionDefinition> m_options;
  QSet<Utils::Id> m_preferredFeatures;
  static int m_verbose;

  friend class ProjectExplorerPlugin;
  friend class ProjectExplorerPluginPrivate;
};

namespace Internal {

class JsonWizardFactoryJsExtension : public QObject {
  Q_OBJECT

public:
  JsonWizardFactoryJsExtension(Utils::Id platformId, const QSet<Utils::Id> &availableFeatures, const QSet<Utils::Id> &pluginFeatures);

  auto value(const QString &name) const -> Q_INVOKABLE QVariant;

private:
  Utils::Id m_platformId;
  QSet<Utils::Id> m_availableFeatures;
  QSet<Utils::Id> m_pluginFeatures;
};

} // namespace Internal
} // namespace ProjectExplorer
