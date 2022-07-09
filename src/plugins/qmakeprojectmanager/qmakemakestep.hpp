// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakeprojectmanager_global.hpp"

#include <projectexplorer/makestep.hpp>

namespace QmakeProjectManager {
namespace Internal {

class QmakeMakeStepFactory : public ProjectExplorer::BuildStepFactory {
public:
  QmakeMakeStepFactory();
};

} // Internal
} // QmakeProjectManager
