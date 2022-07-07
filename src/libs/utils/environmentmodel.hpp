// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "namevaluemodel.hpp"

namespace Utils {

class ORCA_UTILS_EXPORT EnvironmentModel : public NameValueModel {
  Q_OBJECT

public:
  auto baseEnvironment() const -> const Environment&;
  auto setBaseEnvironment(const Environment &env) -> void;
};

} // namespace Utils
