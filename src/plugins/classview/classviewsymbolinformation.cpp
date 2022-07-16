// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classviewsymbolinformation.hpp"

#include "classviewconstants.hpp"
#include "classviewutils.hpp"

#include <utils/utilsicons.hpp>

#include <QPair>
#include <QHash>

namespace ClassView {
namespace Internal {

/*!
    \class SymbolInformation
    \brief The SymbolInformation class provides the name, type, and icon for a
    single item in the Class View tree.
*/

SymbolInformation::SymbolInformation() : m_iconType(INT_MIN), m_hash(0) {}
SymbolInformation::SymbolInformation(const QString &valueName, const QString &valueType, int valueIconType) : m_iconType(valueIconType), m_hash(qHashMulti(0, valueIconType, valueName, valueType)), m_name(valueName), m_type(valueType) {}

/*!
    Returns an icon type sort order number. It is not pre-calculated, as it is
    needed for converting to standard item only.
*/

auto SymbolInformation::iconTypeSortOrder() const -> int
{
  namespace Icons = Utils::CodeModelIcon;
  constexpr int IconSortOrder[] = {Icons::Namespace, Icons::Enum, Icons::Class, Icons::FuncPublic, Icons::FuncProtected, Icons::FuncPrivate, Icons::FuncPublicStatic, Icons::FuncProtectedStatic, Icons::FuncPrivateStatic, Icons::Signal, Icons::SlotPublic, Icons::SlotProtected, Icons::SlotPrivate, Icons::VarPublic, Icons::VarProtected, Icons::VarPrivate, Icons::VarPublicStatic, Icons::VarProtectedStatic, Icons::VarPrivateStatic, Icons::Enumerator, Icons::Keyword, Icons::Macro, Icons::Unknown};

  static QHash<int, int> sortOrder;

  // TODO: Check if this static initialization is OK when SymbolInformation object are
  // instantiated in different threads.
  if (sortOrder.isEmpty()) {
    for (auto i : IconSortOrder)
      sortOrder.insert(i, sortOrder.count());
  }

  // if it is missing - return the same value
  if (!sortOrder.contains(m_iconType))
    return m_iconType;

  return sortOrder[m_iconType];
}

auto SymbolInformation::operator<(const SymbolInformation &other) const -> bool
{
  // comparsion is not a critical for speed
  if (iconType() != other.iconType()) {
    const auto l = iconTypeSortOrder();
    const auto r = other.iconTypeSortOrder();
    if (l < r)
      return true;
    if (l > r)
      return false;
  }

  // The desired behavior here is to facilitate case insensitive
  // sorting without generating false case sensitive equalities.
  // Performance should be appropriate since in C++ there aren't
  // many symbols that differ by case only.

  auto cmp = name().compare(other.name(), Qt::CaseInsensitive);
  if (cmp == 0)
    cmp = name().compare(other.name());
  if (cmp < 0)
    return true;
  if (cmp > 0)
    return false;
  return type().compare(other.type()) < 0;
}

} // namespace Internal
} // namespace ClassView
