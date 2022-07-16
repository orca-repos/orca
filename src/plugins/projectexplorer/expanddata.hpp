// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/porting.hpp>

#include <QString>
#include <QHash>
#include <QDebug>

namespace ProjectExplorer {
namespace Internal {

class ExpandData {
public:
  ExpandData() = default;
  ExpandData(const QString &path_, const QString &displayName_);

  auto operator==(const ExpandData &other) const -> bool;
  static auto fromSettings(const QVariant &v) -> ExpandData;
  auto toSettings() const -> QVariant;

  QString path;
  QString displayName;
};

auto qHash(const ExpandData &data) -> Utils::QHashValueType;

} // namespace Internal
} // namespace ProjectExplorer
