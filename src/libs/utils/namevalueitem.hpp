// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "environmentfwd.hpp"
#include "utils_global.hpp"

#include <QStringList>
#include <QVariantList>
#include <QVector>

namespace Utils {

class ORCA_UTILS_EXPORT NameValueItem {
public:
  enum Operation : char {
    SetEnabled,
    Unset,
    Prepend,
    Append,
    SetDisabled
  };

  NameValueItem() = default;
  NameValueItem(const QString &key, const QString &value, Operation operation = SetEnabled) : name(key), value(value), operation(operation) {}

  auto apply(NameValueDictionary *dictionary) const -> void { apply(dictionary, operation); }
  static auto sort(NameValueItems *list) -> void;
  static auto fromStringList(const QStringList &list) -> NameValueItems;
  static auto toStringList(const NameValueItems &list) -> QStringList;
  static auto itemsFromVariantList(const QVariantList &list) -> NameValueItems;
  static auto toVariantList(const NameValueItems &list) -> QVariantList;
  static auto itemFromVariantList(const QVariantList &list) -> NameValueItem;
  static auto toVariantList(const NameValueItem &item) -> QVariantList;

  friend auto operator==(const NameValueItem &first, const NameValueItem &second) -> bool
  {
    return first.operation == second.operation && first.name == second.name && first.value == second.value;
  }

  friend auto operator!=(const NameValueItem &first, const NameValueItem &second) -> bool
  {
    return !(first == second);
  }

  friend ORCA_UTILS_EXPORT auto operator<<(QDebug debug, const NameValueItem &i) -> QDebug;

  QString name;
  QString value;
  Operation operation = Unset;

private:
  auto apply(NameValueDictionary *dictionary, Operation op) const -> void;
};

} // namespace Utils
