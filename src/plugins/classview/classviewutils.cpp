// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "classviewutils.hpp"
#include "classviewconstants.hpp"
#include "classviewsymbolinformation.hpp"

#include <QStandardItem>
#include <QDebug>

namespace ClassView {
namespace Internal {

/*!
    Converts QVariant location container to internal.
    \a locationsVar contains a list of variant locations from the data of an
    item.
    Returns a set of symbol locations.
 */

auto roleToLocations(const QList<QVariant> &locationsVar) -> QSet<SymbolLocation>
{
  QSet<SymbolLocation> locations;
  foreach(const QVariant &loc, locationsVar) {
    if (loc.canConvert<SymbolLocation>())
      locations.insert(loc.value<SymbolLocation>());
  }

  return locations;
}

/*!
    Returns symbol information for \a item.
*/

auto symbolInformationFromItem(const QStandardItem *item) -> SymbolInformation
{
  Q_ASSERT(item);

  if (!item)
    return SymbolInformation();

  const auto &name = item->data(Constants::SymbolNameRole).toString();
  const auto &type = item->data(Constants::SymbolTypeRole).toString();
  auto iconType = 0;

  const auto var = item->data(Constants::IconTypeRole);
  auto ok = false;
  int value;
  if (var.isValid())
    value = var.toInt(&ok);
  if (ok)
    iconType = value;

  return SymbolInformation(name, type, iconType);
}

} // namespace Internal
} // namespace ClassView
