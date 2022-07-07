// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include "fileutils.hpp"
#include "optional.hpp"
#include "persistentsettings.hpp"

#include <QHash>
#include <QMessageBox>
#include <QVariantMap>

#include <memory>

namespace Utils {

// -----------------------------------------------------------------------------
// Helper:
// -----------------------------------------------------------------------------

ORCA_UTILS_EXPORT auto versionFromMap(const QVariantMap &data) -> int;
ORCA_UTILS_EXPORT auto originalVersionFromMap(const QVariantMap &data) -> int;
ORCA_UTILS_EXPORT auto settingsIdFromMap(const QVariantMap &data) -> QByteArray;
ORCA_UTILS_EXPORT auto setVersionInMap(QVariantMap &data, int version) -> void;
ORCA_UTILS_EXPORT auto setOriginalVersionInMap(QVariantMap &data, int version) -> void;
ORCA_UTILS_EXPORT auto setSettingsIdInMap(QVariantMap &data, const QByteArray &id) -> void;

// --------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------

ORCA_UTILS_EXPORT auto versionFromMap(const QVariantMap &data) -> int;
ORCA_UTILS_EXPORT auto originalVersionFromMap(const QVariantMap &data) -> int;
ORCA_UTILS_EXPORT auto settingsIdFromMap(const QVariantMap &data) -> QByteArray;
ORCA_UTILS_EXPORT auto setVersionInMap(QVariantMap &data, int version) -> void;
ORCA_UTILS_EXPORT auto setOriginalVersionInMap(QVariantMap &data, int version) -> void;
ORCA_UTILS_EXPORT auto setSettingsIdInMap(QVariantMap &data, const QByteArray &id) -> void;

using SettingsMergeResult = optional<QPair<QString, QVariant>>;

// --------------------------------------------------------------------
// SettingsAccessor:
// --------------------------------------------------------------------

// Read/write files incl. error handling suitable for the UI:
class ORCA_UTILS_EXPORT SettingsAccessor {
public:
  SettingsAccessor(const QString &docType, const QString &displayName, const QString &applicationDisplayName);
  virtual ~SettingsAccessor() = default;

  enum ProceedInfo {
    Continue,
    DiscardAndContinue
  };

  using ButtonMap = QHash<QMessageBox::StandardButton, ProceedInfo>;

  class Issue {
  public:
    enum class Type {
      ERROR,
      WARNING
    };

    Issue(const QString &title, const QString &message, const Type type) : title{title}, message{message}, type{type} { }

    auto allButtons() const -> QMessageBox::StandardButtons;

    QString title;
    QString message;
    Type type;
    QMessageBox::StandardButton defaultButton = QMessageBox::NoButton;
    QMessageBox::StandardButton escapeButton = QMessageBox::Ok;
    QHash<QMessageBox::StandardButton, ProceedInfo> buttons = {{QMessageBox::Ok, ProceedInfo::Continue}};
  };

  class RestoreData {
  public:
    RestoreData() = default;
    RestoreData(const FilePath &path, const QVariantMap &data) : path{path}, data{data} { }
    RestoreData(const QString &title, const QString &message, const Issue::Type type) : RestoreData(Issue(title, message, type)) { }
    RestoreData(const Issue &issue) : issue{issue} { }

    auto hasIssue() const -> bool { return bool(issue); }
    auto hasError() const -> bool { return hasIssue() && issue.value().type == Issue::Type::ERROR; }
    auto hasWarning() const -> bool { return hasIssue() && issue.value().type == Issue::Type::WARNING; }

    FilePath path;
    QVariantMap data;
    optional<Issue> issue;
  };

  auto restoreSettings(QWidget *parent) const -> QVariantMap;
  auto saveSettings(const QVariantMap &data, QWidget *parent) const -> bool;

  const QString docType;
  const QString displayName;
  const QString applicationDisplayName;

  auto setBaseFilePath(const FilePath &baseFilePath) -> void { m_baseFilePath = baseFilePath; }
  auto setReadOnly() -> void { m_readOnly = true; }
  auto baseFilePath() const -> FilePath { return m_baseFilePath; }

  virtual auto readData(const FilePath &path, QWidget *parent) const -> RestoreData;
  virtual auto writeData(const FilePath &path, const QVariantMap &data, QWidget *parent) const -> optional<Issue>;

protected:
  // Report errors:
  auto restoreSettings(const FilePath &settingsPath, QWidget *parent) const -> QVariantMap;
  static auto reportIssues(const Issue &issue, const FilePath &path, QWidget *parent) -> ProceedInfo;
  virtual auto preprocessReadSettings(const QVariantMap &data) const -> QVariantMap;
  virtual auto prepareToWriteSettings(const QVariantMap &data) const -> QVariantMap;
  virtual auto readFile(const FilePath &path) const -> RestoreData;
  virtual auto writeFile(const FilePath &path, const QVariantMap &data) const -> optional<Issue>;

private:
  FilePath m_baseFilePath;
  mutable std::unique_ptr<PersistentSettingsWriter> m_writer;
  bool m_readOnly = false;
};

// --------------------------------------------------------------------
// BackingUpSettingsAccessor:
// --------------------------------------------------------------------

class ORCA_UTILS_EXPORT BackUpStrategy {
public:
  virtual ~BackUpStrategy() = default;

  virtual auto readFileCandidates(const FilePath &baseFileName) const -> FilePaths;
  // Return -1 if data1 is better that data2, 0 if both are equally worthwhile
  // and 1 if data2 is better than data1
  virtual auto compare(const SettingsAccessor::RestoreData &data1, const SettingsAccessor::RestoreData &data2) const -> int;

  virtual auto backupName(const QVariantMap &oldData, const FilePath &path, const QVariantMap &data) const -> optional<FilePath>;
};

class ORCA_UTILS_EXPORT BackingUpSettingsAccessor : public SettingsAccessor {
public:
  BackingUpSettingsAccessor(const QString &docType, const QString &displayName, const QString &applicationDisplayName);
  BackingUpSettingsAccessor(std::unique_ptr<BackUpStrategy> &&strategy, const QString &docType, const QString &displayName, const QString &applicationDisplayName);

  auto readData(const FilePath &path, QWidget *parent) const -> RestoreData override;
  auto writeData(const FilePath &path, const QVariantMap &data, QWidget *parent) const -> optional<Issue> override;
  auto strategy() const -> BackUpStrategy* { return m_strategy.get(); }

private:
  auto readFileCandidates(const FilePath &path) const -> FilePaths;
  auto bestReadFileData(const FilePaths &candidates, QWidget *parent) const -> RestoreData;
  auto backupFile(const FilePath &path, const QVariantMap &data, QWidget *parent) const -> void;

  std::unique_ptr<BackUpStrategy> m_strategy;
};

// --------------------------------------------------------------------
// UpgradingSettingsAccessor:
// --------------------------------------------------------------------

class UpgradingSettingsAccessor;

class ORCA_UTILS_EXPORT VersionedBackUpStrategy : public BackUpStrategy {
public:
  VersionedBackUpStrategy(const UpgradingSettingsAccessor *accessor);

  // Return -1 if data1 is better that data2, 0 if both are equally worthwhile
  // and 1 if data2 is better than data1
  auto compare(const SettingsAccessor::RestoreData &data1, const SettingsAccessor::RestoreData &data2) const -> int override;
  auto backupName(const QVariantMap &oldData, const FilePath &path, const QVariantMap &data) const -> optional<FilePath> override;
  auto accessor() const -> const UpgradingSettingsAccessor* { return m_accessor; }

protected:
  const UpgradingSettingsAccessor *m_accessor = nullptr;
};

// Handles updating a QVariantMap from version() to version() + 1
class ORCA_UTILS_EXPORT VersionUpgrader {
public:
  VersionUpgrader(int version, const QString &extension);
  virtual ~VersionUpgrader() = default;

  auto version() const -> int;
  auto backupExtension() const -> QString;

  virtual auto upgrade(const QVariantMap &data) -> QVariantMap = 0;

protected:
  using Change = QPair<QLatin1String, QLatin1String>;
  auto renameKeys(const QList<Change> &changes, QVariantMap map) const -> QVariantMap;

private:
  const int m_version;
  const QString m_extension;
};

class MergingSettingsAccessor;

class ORCA_UTILS_EXPORT UpgradingSettingsAccessor : public BackingUpSettingsAccessor {
public:
  UpgradingSettingsAccessor(const QString &docType, const QString &displayName, const QString &applicationDisplayName);
  UpgradingSettingsAccessor(std::unique_ptr<BackUpStrategy> &&strategy, const QString &docType, const QString &displayName, const QString &appDisplayName);

  auto currentVersion() const -> int;
  auto firstSupportedVersion() const -> int;
  auto lastSupportedVersion() const -> int;
  auto settingsId() const -> QByteArray { return m_id; }
  auto isValidVersionAndId(int version, const QByteArray &id) const -> bool;
  auto upgrader(int version) const -> VersionUpgrader*;
  auto readData(const FilePath &path, QWidget *parent) const -> RestoreData override;

protected:
  auto prepareToWriteSettings(const QVariantMap &data) const -> QVariantMap override;
  auto setSettingsId(const QByteArray &id) -> void { m_id = id; }
  auto addVersionUpgrader(std::unique_ptr<VersionUpgrader> &&upgrader) -> bool;
  auto upgradeSettings(const RestoreData &data, int targetVersion) const -> RestoreData;
  auto validateVersionRange(const RestoreData &data) const -> RestoreData;

private:
  QByteArray m_id;
  std::vector<std::unique_ptr<VersionUpgrader>> m_upgraders;
};

// --------------------------------------------------------------------
// MergingSettingsAccessor:
// --------------------------------------------------------------------

class ORCA_UTILS_EXPORT MergingSettingsAccessor : public UpgradingSettingsAccessor {
public:
  struct SettingsMergeData {
    QVariantMap main;
    QVariantMap secondary;
    QString key;
  };

  MergingSettingsAccessor(std::unique_ptr<BackUpStrategy> &&strategy, const QString &docType, const QString &displayName, const QString &applicationDisplayName);

  auto readData(const FilePath &path, QWidget *parent) const -> RestoreData final;
  auto setSecondaryAccessor(std::unique_ptr<SettingsAccessor> &&secondary) -> void;

protected:
  auto mergeSettings(const RestoreData &main, const RestoreData &secondary) const -> RestoreData;
  virtual auto merge(const SettingsMergeData &global, const SettingsMergeData &local) const -> SettingsMergeResult = 0;
  static auto isHouseKeepingKey(const QString &key) -> bool;
  virtual auto postprocessMerge(const QVariantMap &main, const QVariantMap &secondary, const QVariantMap &result) const -> QVariantMap;

private:
  std::unique_ptr<SettingsAccessor> m_secondaryAccessor;
};

using SettingsMergeFunction = std::function<SettingsMergeResult(const MergingSettingsAccessor::SettingsMergeData &, const MergingSettingsAccessor::SettingsMergeData &)>;
ORCA_UTILS_EXPORT auto mergeQVariantMaps(const QVariantMap &mainTree, const QVariantMap &secondaryTree, const SettingsMergeFunction &merge) -> QVariant;

} // namespace Utils
