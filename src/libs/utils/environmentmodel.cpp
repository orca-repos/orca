// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "environmentmodel.h"

#include <utils/algorithm.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>

#include <QString>
#include <QFont>

namespace Utils {

auto EnvironmentModel::baseEnvironment() const -> const Environment&
{
  return static_cast<const Environment&>(baseNameValueDictionary());
}

auto EnvironmentModel::setBaseEnvironment(const Environment &env) -> void
{
  setBaseNameValueDictionary(env);
}

} // namespace Utils
