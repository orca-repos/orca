// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "environmentfwd.hpp"
#include "utils_global.hpp"

#include <QAbstractTableModel>

#include <memory>

namespace Utils {

namespace Internal {
class NameValueModelPrivate;
}

class ORCA_UTILS_EXPORT NameValueModel : public QAbstractTableModel {
  Q_OBJECT

public:
  explicit NameValueModel(QObject *parent = nullptr);
  ~NameValueModel() override;

  auto rowCount(const QModelIndex &parent) const -> int override;
  auto columnCount(const QModelIndex &parent) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;
  auto headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const -> QVariant override;
  auto addVariable() -> QModelIndex;
  auto addVariable(const NameValueItem &item) -> QModelIndex;
  auto resetVariable(const QString &name) -> void;
  auto unsetVariable(const QString &name) -> void;
  auto toggleVariable(const QModelIndex &index) -> void;
  auto isUnset(const QString &name) -> bool;
  auto isEnabled(const QString &name) const -> bool;
  auto canReset(const QString &name) -> bool;
  auto indexToVariable(const QModelIndex &index) const -> QString;
  auto variableToIndex(const QString &name) const -> QModelIndex;
  auto changes(const QString &key) const -> bool;
  auto baseNameValueDictionary() const -> const NameValueDictionary&;
  auto setBaseNameValueDictionary(const NameValueDictionary &dictionary) -> void;
  auto userChanges() const -> NameValueItems;
  auto setUserChanges(const NameValueItems &items) -> void;
  auto currentEntryIsPathList(const QModelIndex &current) const -> bool;

signals:
  auto userChangesChanged() -> void;
  /// Hint to the view where it should make sense to focus on next
  // This is a hack since there is no way for a model to suggest
  // the next interesting place to focus on to the view.
  auto focusIndex(const QModelIndex &index) -> void;

private:
  std::unique_ptr<Internal::NameValueModelPrivate> d;
};

} // namespace Utils
