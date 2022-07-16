// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "runconfigurationaspects.hpp"
#include "runcontrol.hpp"

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT CustomExecutableRunConfiguration : public RunConfiguration {
  Q_OBJECT

public:
  CustomExecutableRunConfiguration(Target *target, Utils::Id id);
  explicit CustomExecutableRunConfiguration(Target *target);

  auto defaultDisplayName() const -> QString;

private:
  auto runnable() const -> Runnable override;
  auto isEnabled() const -> bool override;
  auto checkForIssues() const -> Tasks override;
  auto configurationDialogFinished() -> void;
  auto executable() const -> Utils::FilePath;
};

class CustomExecutableRunConfigurationFactory : public FixedRunConfigurationFactory {
public:
  CustomExecutableRunConfigurationFactory();
};

class CustomExecutableRunWorkerFactory : public RunWorkerFactory {
public:
  CustomExecutableRunWorkerFactory();
};

} // namespace ProjectExplorer
