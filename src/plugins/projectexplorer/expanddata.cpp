// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "expanddata.hpp"

#include <QVariant>

using namespace ProjectExplorer;
using namespace Internal;

ExpandData::ExpandData(const QString &path_, const QString &displayName_) : path(path_), displayName(displayName_) { }

auto ExpandData::operator==(const ExpandData &other) const -> bool
{
  return path == other.path && displayName == other.displayName;
}

auto ExpandData::fromSettings(const QVariant &v) -> ExpandData
{
  const auto list = v.toStringList();
  return list.size() == 2 ? ExpandData(list.at(0), list.at(1)) : ExpandData();
}

auto ExpandData::toSettings() const -> QVariant
{
  return QVariant::fromValue(QStringList({path, displayName}));
}

auto Internal::qHash(const ExpandData &data) -> Utils::QHashValueType
{
  return qHash(data.path) ^ qHash(data.displayName);
}
