// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QString>
#include <QVariantMap>

namespace Utils {

// Can be used for anything with a translatable, user-settable name with a fixed default value
// that gets set by a constructor or factory.
class ORCA_UTILS_EXPORT DisplayName {
public:
  // These return true if and only if the value of displayName() has changed.
  auto setValue(const QString &name) -> bool;
  auto setDefaultValue(const QString &name) -> bool;
  auto value() const -> QString;
  auto defaultValue() const -> QString { return m_defaultValue; }
  auto usesDefaultValue() const -> bool;
  auto toMap(QVariantMap &map, const QString &key) const -> void;
  auto fromMap(const QVariantMap &map, const QString &key) -> void;

private:
  QString m_value;
  QString m_defaultValue;
};

ORCA_UTILS_EXPORT auto operator==(const DisplayName &dn1, const DisplayName &dn2) -> bool;

inline auto operator!=(const DisplayName &dn1, const DisplayName &dn2) -> bool
{
  return !(dn1 == dn2);
}

} // namespace Utils

