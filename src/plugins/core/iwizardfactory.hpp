// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>
#include <core/featureprovider.hpp>

#include <QIcon>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

QT_FORWARD_DECLARE_CLASS(QAction)

namespace Utils {
class FilePath;
class Wizard;
} // Utils

namespace Core {

namespace Internal {
class CorePlugin;
}

class CORE_EXPORT IWizardFactory : public QObject {
  Q_OBJECT

public:
  enum WizardKind {
    FileWizard = 0x01,
    ProjectWizard = 0x02
  };

  enum WizardFlag {
    PlatformIndependent = 0x01,
    ForceCapitalLetterForFileName = 0x02
  };

  Q_DECLARE_FLAGS(WizardFlags, WizardFlag)

  using FactoryCreator = std::function<QList<IWizardFactory*>()>;

  auto id() const -> Utils::Id { return m_id; }
  auto kind() const -> WizardKind { return m_supported_project_types.isEmpty() ? FileWizard : ProjectWizard; }
  auto icon() const -> QIcon { return m_icon; }
  auto fontIconName() const -> QString { return m_font_icon_name; }
  auto description() const -> QString { return m_description; }
  auto displayName() const -> QString { return m_display_name; }
  auto category() const -> QString { return m_category; }
  auto displayCategory() const -> QString { return m_display_category; }
  auto descriptionImage() const -> QString { return m_description_image; }
  auto requiredFeatures() const -> QSet<Utils::Id> { return m_required_features; }
  auto flags() const -> WizardFlags { return m_flags; }
  auto detailsPageQmlPath() const -> QUrl { return m_details_page_qml_path; }
  auto supportedProjectTypes() const -> QSet<Utils::Id> { return m_supported_project_types; }
  auto setId(const Utils::Id id) -> void { m_id = id; }
  auto setSupportedProjectTypes(const QSet<Utils::Id> &project_types) -> void { m_supported_project_types = project_types; }
  auto setIcon(const QIcon &icon, const QString &icon_text = {}) -> void;
  auto setFontIconName(const QString &code) -> void { m_font_icon_name = code; }
  auto setDescription(const QString &description) -> void { m_description = description; }
  auto setDisplayName(const QString &display_name) -> void { m_display_name = display_name; }
  auto setCategory(const QString &category) -> void { m_category = category; }
  auto setDisplayCategory(const QString &display_category) -> void { m_display_category = display_category; }
  auto setDescriptionImage(const QString &description_image) -> void { m_description_image = description_image; }
  auto setRequiredFeatures(const QSet<Utils::Id> &feature_set) -> void { m_required_features = feature_set; }
  auto addRequiredFeature(const Utils::Id &feature) -> void { m_required_features |= feature; }
  auto setFlags(const WizardFlags flags) -> void { m_flags = flags; }
  auto setDetailsPageQmlPath(const QString &file_path) -> void;
  auto runPath(const Utils::FilePath &default_path) const -> Utils::FilePath;

  // Does bookkeeping and the calls runWizardImpl. Please implement that.
  auto runWizard(const Utils::FilePath &path, QWidget *parent, Utils::Id platform, const QVariantMap &variables, bool show_wizard = true) -> Utils::Wizard*;
  virtual auto isAvailable(Utils::Id platform_id) const -> bool;
  auto supportedPlatforms() const -> QSet<Utils::Id>;
  static auto registerFactoryCreator(const FactoryCreator &creator) -> void;

  // Utility to find all registered wizards
  static auto allWizardFactories() -> QList<IWizardFactory*>;
  static auto allAvailablePlatforms() -> QSet<Utils::Id>;
  static auto displayNameForPlatform(Utils::Id i) -> QString;
  static auto registerFeatureProvider(IFeatureProvider *provider) -> void;
  static auto isWizardRunning() -> bool;
  static auto currentWizard() -> QWidget*;
  static auto requestNewItemDialog(const QString &title, const QList<IWizardFactory*> &factories, const Utils::FilePath &default_location, const QVariantMap &extra_variables) -> void;
  static auto themedIcon(const Utils::FilePath &icon_mask_path) -> QIcon;

protected:
  static auto pluginFeatures() -> QSet<Utils::Id>;
  static auto availableFeatures(Utils::Id platform_id) -> QSet<Utils::Id>;
  virtual auto runWizardImpl(const Utils::FilePath &path, QWidget *parent, Utils::Id platform, const QVariantMap &variables, bool show_wizard = true) -> Utils::Wizard* = 0;

private:
  static auto initialize() -> void;
  static auto destroyFeatureProvider() -> void;
  static auto clearWizardFactories() -> void;

  QAction *m_action = nullptr;
  QIcon m_icon;
  QString m_font_icon_name;
  QString m_description;
  QString m_display_name;
  QString m_category;
  QString m_display_category;
  QString m_description_image;
  QUrl m_details_page_qml_path;
  QSet<Utils::Id> m_required_features;
  QSet<Utils::Id> m_supported_project_types;
  WizardFlags m_flags;
  Utils::Id m_id;

  friend class Internal::CorePlugin;
};

} // namespace Core

Q_DECLARE_OPERATORS_FOR_FLAGS(Core::IWizardFactory::WizardFlags)
