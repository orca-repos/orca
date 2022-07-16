// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmakeconfigitem.hpp"

#include <utils/treemodel.hpp>

#include <QString>

namespace CMakeProjectManager {
namespace Internal {

class ConfigModelTreeItem;

class ConfigModel : public Utils::TreeModel<> {
  Q_OBJECT

public:
  enum Roles {
    ItemIsAdvancedRole = Qt::UserRole,
    ItemIsInitialRole
  };

  struct DataItem {
    auto operator==(const DataItem &other) const -> bool
    {
      return key == other.key && isInitial == other.isInitial;
    }

    DataItem() {}
    DataItem(const CMakeConfigItem &cmi);

    auto setType(CMakeConfigItem::Type cmt) -> void;
    auto typeDisplay() const -> QString;
    auto toCMakeConfigItem() const -> CMakeConfigItem;

    enum Type {
      BOOLEAN,
      FILE,
      DIRECTORY,
      STRING,
      UNKNOWN
    };

    QString key;
    Type type = STRING;
    bool isHidden = false;
    bool isAdvanced = false;
    bool isInitial = false;
    bool inCMakeCache = false;
    bool isUnset = false;
    QString value;
    QString description;
    QStringList values;
  };

  explicit ConfigModel(QObject *parent = nullptr);
  ~ConfigModel() override;

  using KitConfiguration = QHash<QString, CMakeConfigItem>;

  auto data(const QModelIndex &idx, int role) const -> QVariant final;
  auto setData(const QModelIndex &idx, const QVariant &data, int role) -> bool final;
  auto appendConfiguration(const QString &key, const QString &value = QString(), const DataItem::Type type = DataItem::UNKNOWN, bool isInitial = false, const QString &description = QString(), const QStringList &values = QStringList()) -> void;
  auto setConfiguration(const CMakeConfig &config) -> void;
  auto setBatchEditConfiguration(const CMakeConfig &config) -> void;
  auto setInitialParametersConfiguration(const CMakeConfig &config) -> void;
  auto setConfiguration(const QList<DataItem> &config) -> void;
  auto setConfigurationFromKit(const KitConfiguration &kitConfig) -> void;
  auto flush() -> void;
  auto resetAllChanges(bool initialParameters = false) -> void;
  auto hasChanges(bool initialParameters = false) const -> bool;
  auto canForceTo(const QModelIndex &idx, const DataItem::Type type) const -> bool;
  auto forceTo(const QModelIndex &idx, const DataItem::Type type) -> void;
  auto toggleUnsetFlag(const QModelIndex &idx) -> void;
  auto applyKitValue(const QModelIndex &idx) -> void;
  auto applyInitialValue(const QModelIndex &idx) -> void;
  static auto dataItemFromIndex(const QModelIndex &idx) -> DataItem;
  auto configurationForCMake() const -> QList<DataItem>;
  auto macroExpander() const -> Utils::MacroExpander*;
  auto setMacroExpander(Utils::MacroExpander *newExpander) -> void;

private:
  enum class KitOrInitial {
    Kit,
    Initial
  };

  auto applyKitOrInitialValue(const QModelIndex &idx, KitOrInitial ki) -> void;

  class InternalDataItem : public DataItem {
  public:
    InternalDataItem(const DataItem &item);
    InternalDataItem(const InternalDataItem &item) = default;

    auto currentValue() const -> QString;

    bool isUserChanged = false;
    bool isUserNew = false;
    QString newValue;
    QString kitValue;
    QString initialValue;
  };

  auto generateTree() -> void;

  auto setConfiguration(const QList<InternalDataItem> &config) -> void;
  QList<InternalDataItem> m_configuration;
  KitConfiguration m_kitConfiguration;
  Utils::MacroExpander *m_macroExpander = nullptr;

  friend class Internal::ConfigModelTreeItem;
};

class ConfigModelTreeItem : public Utils::TreeItem {
public:
  ConfigModelTreeItem(ConfigModel::InternalDataItem *di = nullptr) : dataItem(di) {}
  ~ConfigModelTreeItem() override;

  auto data(int column, int role) const -> QVariant final;
  auto setData(int column, const QVariant &data, int role) -> bool final;
  auto flags(int column) const -> Qt::ItemFlags final;
  auto toolTip() const -> QString;
  auto currentValue() const -> QString;

  ConfigModel::InternalDataItem *dataItem;
};

} // namespace Internal
} // namespace CMakeProjectManager
