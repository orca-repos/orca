// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <projectexplorer/rawprojectpart.hpp>

namespace QtSupport {

class QtVersion;

class QTSUPPORT_EXPORT CppKitInfo : public ProjectExplorer::KitInfo {
public:
  CppKitInfo(ProjectExplorer::Kit *kit);

  QtVersion *qtVersion = nullptr;
};

} // namespace QtSupport
