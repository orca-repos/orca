// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <utils/id.hpp>

#include <QSet>

namespace Core {

class CORE_EXPORT FeatureSet;

class CORE_EXPORT IFeatureProvider {
public:
  virtual ~IFeatureProvider() = default;

  virtual auto availableFeatures(Utils::Id id) const -> QSet<Utils::Id> = 0;
  virtual auto availablePlatforms() const -> QSet<Utils::Id> = 0;
  virtual auto displayNameForPlatform(Utils::Id id) const -> QString = 0;
};

} // namespace Core
