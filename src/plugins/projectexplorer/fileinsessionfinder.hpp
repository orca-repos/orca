// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/fileutils.hpp>

namespace ProjectExplorer {

// Possibly used by "QtCreatorTerminalPlugin"
PROJECTEXPLORER_EXPORT auto findFileInSession(const Utils::FilePath &filePath) -> Utils::FilePaths;

} // namespace ProjectExplorer
