// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ioutputparser.hpp"

namespace ProjectExplorer {
namespace Internal {

class LldParser : public OutputTaskParser {
  auto handleLine(const QString &line, Utils::OutputFormat type) -> Result override;
};

} // namespace Internal
} // namespace ProjectExplorer
