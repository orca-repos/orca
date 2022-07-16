// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclient_global.hpp"

#include <core/core-options-page-interface.hpp>

#include <QAbstractItemModel>
#include <QCoreApplication>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QUuid>
#include <QWidget>

QT_BEGIN_NAMESPACE
class QComboBox;
class QLineEdit;
QT_END_NAMESPACE

namespace Utils {
class CommandLine;
class FilePath;
class PathChooser;
class FancyLineEdit;
} // namespace Utils

namespace Orca::Plugin::Core {
class IDocument;
}

namespace ProjectExplorer {
class Project;
}

namespace LanguageClient {

class Client;
class BaseClientInterface;

struct LANGUAGECLIENT_EXPORT LanguageFilter {
  QStringList mimeTypes;
  QStringList filePattern;

  auto isSupported(const Utils::FilePath &filePath, const QString &mimeType) const -> bool;
  auto isSupported(const Orca::Plugin::Core::IDocument *document) const -> bool;
  auto operator==(const LanguageFilter &other) const -> bool;
  auto operator!=(const LanguageFilter &other) const -> bool;
};

class LANGUAGECLIENT_EXPORT BaseSettings {
public:
  BaseSettings() = default;
  virtual ~BaseSettings() = default;

  enum StartBehavior {
    AlwaysOn = 0,
    RequiresFile,
    RequiresProject,
    LastSentinel
  };

  QString m_name = QString("New Language Server");
  QString m_id = QUuid::createUuid().toString();
  Utils::Id m_settingsTypeId;
  bool m_enabled = true;
  StartBehavior m_startBehavior = RequiresFile;
  LanguageFilter m_languageFilter;
  QString m_initializationOptions;

  auto initializationOptions() const -> QJsonObject;
  virtual auto applyFromSettingsWidget(QWidget *widget) -> bool;
  virtual auto createSettingsWidget(QWidget *parent = nullptr) const -> QWidget*;
  virtual auto copy() const -> BaseSettings* { return new BaseSettings(*this); }
  virtual auto isValid() const -> bool;
  auto createClient() -> Client*;
  auto createClient(ProjectExplorer::Project *project) -> Client*;
  virtual auto toMap() const -> QVariantMap;
  virtual auto fromMap(const QVariantMap &map) -> void;

protected:
  // TODO: remove in Qt Creator 6 and rename createInterfaceWithProject back to it
  virtual auto createInterface() const -> BaseClientInterface* { return nullptr; }
  virtual auto createClient(BaseClientInterface *interface) const -> Client*;

  virtual auto createInterfaceWithProject(ProjectExplorer::Project *) const -> BaseClientInterface*
  {
    return createInterface();
  }

  BaseSettings(const BaseSettings &other) = default;
  BaseSettings(BaseSettings &&other) = default;
  auto operator=(const BaseSettings &other) -> BaseSettings& = default;
  auto operator=(BaseSettings &&other) -> BaseSettings& = default;

private:
  auto canStart(QList<const Orca::Plugin::Core::IDocument*> documents) const -> bool;
};

class LANGUAGECLIENT_EXPORT StdIOSettings : public BaseSettings {
public:
  StdIOSettings() = default;
  ~StdIOSettings() override = default;

  Utils::FilePath m_executable;
  QString m_arguments;

  auto applyFromSettingsWidget(QWidget *widget) -> bool override;
  auto createSettingsWidget(QWidget *parent = nullptr) const -> QWidget* override;
  auto copy() const -> BaseSettings* override { return new StdIOSettings(*this); }
  auto isValid() const -> bool override;
  auto toMap() const -> QVariantMap override;
  auto fromMap(const QVariantMap &map) -> void override;
  auto arguments() const -> QString;
  auto command() const -> Utils::CommandLine;

protected:
  auto createInterfaceWithProject(ProjectExplorer::Project *project) const -> BaseClientInterface* override;

  StdIOSettings(const StdIOSettings &other) = default;
  StdIOSettings(StdIOSettings &&other) = default;
  auto operator=(const StdIOSettings &other) -> StdIOSettings& = default;
  auto operator=(StdIOSettings &&other) -> StdIOSettings& = default;
};

struct ClientType {
  Utils::Id id;
  QString name;
  using SettingsGenerator = std::function<BaseSettings*()>;
  SettingsGenerator generator = nullptr;
};

class LANGUAGECLIENT_EXPORT LanguageClientSettings {
  Q_DECLARE_TR_FUNCTIONS(LanguageClientSettings)

public:
  static auto init() -> void;
  static auto fromSettings(QSettings *settings) -> QList<BaseSettings*>;
  static auto pageSettings() -> QList<BaseSettings*>;
  static auto changedSettings() -> QList<BaseSettings*>;

  /**
   * must be called before the delayed initialize phase
   * otherwise the settings are not loaded correctly
   */
  static auto registerClientType(const ClientType &type) -> void;
  static auto addSettings(BaseSettings *settings) -> void;
  static auto enableSettings(const QString &id) -> void;
  static auto toSettings(QSettings *settings, const QList<BaseSettings*> &languageClientSettings) -> void;

  static auto outlineComboBoxIsSorted() -> bool;
  static auto setOutlineComboBoxSorted(bool sorted) -> void;
};

class LANGUAGECLIENT_EXPORT BaseSettingsWidget : public QWidget {
  Q_OBJECT

public:
  explicit BaseSettingsWidget(const BaseSettings *settings, QWidget *parent = nullptr);
  ~BaseSettingsWidget() override = default;

  auto name() const -> QString;
  auto filter() const -> LanguageFilter;
  auto startupBehavior() const -> BaseSettings::StartBehavior;
  auto alwaysOn() const -> bool;
  auto requiresProject() const -> bool;
  auto initializationOptions() const -> QString;

private:
  auto showAddMimeTypeDialog() -> void;

  QLineEdit *m_name = nullptr;
  QLabel *m_mimeTypes = nullptr;
  QLineEdit *m_filePattern = nullptr;
  QComboBox *m_startupBehavior = nullptr;
  Utils::FancyLineEdit *m_initializationOptions = nullptr;

  static constexpr char filterSeparator = ';';
};

class LANGUAGECLIENT_EXPORT StdIOSettingsWidget : public BaseSettingsWidget {
  Q_OBJECT

public:
  explicit StdIOSettingsWidget(const StdIOSettings *settings, QWidget *parent = nullptr);
  ~StdIOSettingsWidget() override = default;

  auto executable() const -> Utils::FilePath;
  auto arguments() const -> QString;

private:
  Utils::PathChooser *m_executable = nullptr;
  QLineEdit *m_arguments = nullptr;
};

} // namespace LanguageClient
