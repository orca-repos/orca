// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <core/core-options-page-interface.hpp>

#include <utils/aspects.hpp>

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT BuildPropertiesSettings : public Utils::AspectContainer {
  Q_DECLARE_TR_FUNCTIONS(ProjectExplorer::Internal::BuildPropertiesSettings)

public:
  BuildPropertiesSettings();

  class BuildTriStateAspect : public Utils::TriStateAspect {
  public:
    BuildTriStateAspect();
  };

  Utils::StringAspect buildDirectoryTemplate;
  Utils::StringAspect buildDirectoryTemplateOld; // TODO: Remove in ~4.16
  BuildTriStateAspect separateDebugInfo;
  BuildTriStateAspect qmlDebugging;
  BuildTriStateAspect qtQuickCompiler;
  Utils::BoolAspect showQtSettings;

  auto readSettings(QSettings *settings) -> void;
  auto defaultBuildDirectoryTemplate() -> QString;
};

namespace Internal {

class BuildPropertiesSettingsPage final : public Orca::Plugin::Core::IOptionsPage {
public:
  explicit BuildPropertiesSettingsPage(BuildPropertiesSettings *settings);
};

} // namespace Internal
} // namespace ProjectExplorer
