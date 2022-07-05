// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "displayname.h"

namespace Utils {

auto DisplayName::setValue(const QString &name) -> bool
{
  if (value() == name)
    return false;
  if (name == m_defaultValue)
    m_value.clear();
  else
    m_value = name;
  return true;
}

auto DisplayName::setDefaultValue(const QString &name) -> bool
{
  if (m_defaultValue == name)
    return false;
  const QString originalName = value();
  m_defaultValue = name;
  return originalName != value();
}

auto DisplayName::value() const -> QString
{
  return m_value.isEmpty() ? m_defaultValue : m_value;
}

auto DisplayName::usesDefaultValue() const -> bool
{
  return m_value.isEmpty();
}

auto DisplayName::toMap(QVariantMap &map, const QString &key) const -> void
{
  if (!usesDefaultValue())
    map.insert(key, m_value);
}

auto DisplayName::fromMap(const QVariantMap &map, const QString &key) -> void
{
  m_value = map.value(key).toString();
}

auto operator==(const DisplayName &dn1, const DisplayName &dn2) -> bool
{
  return dn1.value() == dn2.value() && dn1.defaultValue() == dn2.defaultValue();
}

} // namespace Utils
