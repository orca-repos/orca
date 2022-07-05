// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "fileutils.h"
#include "hostosinfo.h"
#include "namevalueitem.h"

namespace Utils {

class ORCA_UTILS_EXPORT DictKey
{
public:
    DictKey(const QString &name, Qt::CaseSensitivity cs) : name(name), caseSensitivity(cs) {}

    friend auto operator==(const DictKey &k1, const DictKey &k2) -> bool
    {
        return k1.name.compare(k2.name, k1.caseSensitivity) == 0;
    }

    QString name;
    Qt::CaseSensitivity caseSensitivity;
};

inline auto operator<(const DictKey &k1, const DictKey &k2) -> bool
{
    return k1.name.compare(k2.name, k1.caseSensitivity) < 0;
}

inline auto operator>(const DictKey &k1, const DictKey &k2) -> bool { return k2 < k1; }

using NameValuePair = std::pair<QString, QString>;
using NameValuePairs = QVector<NameValuePair>;
using NameValueMap = QMap<DictKey, QPair<QString, bool>>;

class ORCA_UTILS_EXPORT NameValueDictionary {
public:
  using const_iterator = NameValueMap::const_iterator;

  explicit NameValueDictionary(OsType osType = HostOsInfo::hostOs()) : m_osType(osType) {}
  explicit NameValueDictionary(const QStringList &env, OsType osType = HostOsInfo::hostOs());
  explicit NameValueDictionary(const NameValuePairs &nameValues);

  auto toStringList() const -> QStringList;
  auto value(const QString &key) const -> QString;
  auto set(const QString &key, const QString &value, bool enabled = true) -> void;
  auto unset(const QString &key) -> void;
  auto modify(const NameValueItems &items) -> void;
  /// Return the KeyValueDictionary changes necessary to modify this into the other environment.
  auto diff(const NameValueDictionary &other, bool checkAppendPrepend = false) const -> NameValueItems;
  auto hasKey(const QString &key) const -> bool;
  auto osType() const -> OsType;
  auto nameCaseSensitivity() const -> Qt::CaseSensitivity;
  auto userName() const -> QString;
  auto clear() -> void;
  auto size() const -> int;
  auto key(const_iterator it) const -> QString { return it.key().name; }
  auto value(const_iterator it) const -> QString { return it.value().first; }
  auto isEnabled(const_iterator it) const -> bool { return it.value().second; }
  auto constBegin() const -> const_iterator { return m_values.constBegin(); }
  auto constEnd() const -> const_iterator { return m_values.constEnd(); }
  auto constFind(const QString &name) const -> const_iterator;

  friend auto operator!=(const NameValueDictionary &first, const NameValueDictionary &second) -> bool
  {
    return !(first == second);
  }

  friend auto operator==(const NameValueDictionary &first, const NameValueDictionary &second) -> bool
  {
    return first.m_osType == second.m_osType && first.m_values == second.m_values;
  }

protected:
  auto findKey(const QString &key) -> NameValueMap::iterator;
  auto findKey(const QString &key) const -> const_iterator;

  NameValueMap m_values;
  OsType m_osType;
};

} // namespace Utils
