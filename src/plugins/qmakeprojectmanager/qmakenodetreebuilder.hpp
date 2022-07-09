// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakeprojectmanager_global.hpp"
#include "qmakeparsernodes.hpp"
#include "qmakenodes.hpp"
#include "qmakeproject.hpp"

namespace QmakeProjectManager {

class QmakeNodeTreeBuilder
{
public:
    static auto buildTree(QmakeBuildSystem *buildSystem) -> std::unique_ptr<QmakeProFileNode>;
};

} // namespace QmakeProjectManager
