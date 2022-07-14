// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "analysis-plugin.hpp"

namespace Orca::Plugin::Analysis {

auto Plugin::initialize(const QStringList &, QString *) -> bool
{
  return true;
}

auto Plugin::extensionsInitialized() -> void
{

}

} // namespace Orca::Plugin::Analysis
