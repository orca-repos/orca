// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/featureprovider.hpp>

namespace ProjectExplorer {
namespace Internal {

class KitFeatureProvider : public Core::IFeatureProvider {
public:
  auto availableFeatures(Utils::Id id) const -> QSet<Utils::Id> override;
  auto availablePlatforms() const -> QSet<Utils::Id> override;
  auto displayNameForPlatform(Utils::Id id) const -> QString override;
};

} // namespace Internal
} // namespace ProjectExplorer
