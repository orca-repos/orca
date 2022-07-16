// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/porting.hpp>

#include <QMetaType>
#include <QString>

#include <limits.h>

namespace ClassView {
namespace Internal {

class SymbolInformation {
public:
  SymbolInformation();
  SymbolInformation(const QString &name, const QString &type, int iconType = INT_MIN);

  auto operator<(const SymbolInformation &other) const -> bool;
  auto name() const -> const QString& { return m_name; }
  auto type() const -> const QString& { return m_type; }
  auto iconType() const -> int { return m_iconType; }
  auto hash() const { return m_hash; }

  auto operator==(const SymbolInformation &other) const -> bool
  {
    return hash() == other.hash() && iconType() == other.iconType() && name() == other.name() && type() == other.type();
  }

  auto iconTypeSortOrder() const -> int;

  friend auto qHash(const SymbolInformation &information) { return information.hash(); }

private:
  const int m_iconType;
  const Utils::QHashValueType m_hash; // precalculated hash value - to speed up qHash
  const QString m_name;               // symbol name (e.g. SymbolInformation)
  const QString m_type;               // symbol type (e.g. (int char))
};

} // namespace Internal
} // namespace ClassView

Q_DECLARE_METATYPE(ClassView::Internal::SymbolInformation)
