// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "runconfigurationaspects.hpp"
#include "runcontrol.hpp"

namespace ProjectExplorer {
namespace Internal {

class DesktopQmakeRunConfigurationFactory final : public RunConfigurationFactory {
public:
  DesktopQmakeRunConfigurationFactory();
};

class QbsRunConfigurationFactory final : public RunConfigurationFactory {
public:
  QbsRunConfigurationFactory();
};

class CMakeRunConfigurationFactory final : public RunConfigurationFactory {
public:
  CMakeRunConfigurationFactory();
};

} // namespace Internal
} // namespace ProjectExplorer
