// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakespecificsettings.hpp"

#include <core/core-interface.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>

#include <utils/layoutbuilder.hpp>

using namespace Utils;

namespace CMakeProjectManager {
namespace Internal {

CMakeSpecificSettings::CMakeSpecificSettings()
{
  // TODO: fixup of QTCREATORBUG-26289 , remove in Qt Creator 7 or so
  Orca::Plugin::Core::ICore::settings()->remove("CMakeSpecificSettings/NinjaPath");

  setSettingsGroup("CMakeSpecificSettings");
  setAutoApply(false);

  registerAspect(&afterAddFileSetting);
  afterAddFileSetting.setSettingsKey("ProjectPopupSetting");
  afterAddFileSetting.setDefaultValue(AfterAddFileAction::AskUser);
  afterAddFileSetting.addOption(tr("Ask about copying file paths"));
  afterAddFileSetting.addOption(tr("Do not copy file paths"));
  afterAddFileSetting.addOption(tr("Copy file paths"));
  afterAddFileSetting.setToolTip(tr("Determines whether file paths are copied " "to the clipboard for pasting to the CMakeLists.txt file when you " "add new files to CMake projects."));

  registerAspect(&ninjaPath);
  ninjaPath.setSettingsKey("NinjaPath");
  // never save this to the settings:
  ninjaPath.setToSettingsTransformation([](const QVariant &) { return QVariant::fromValue(QString()); });

  registerAspect(&packageManagerAutoSetup);
  packageManagerAutoSetup.setSettingsKey("PackageManagerAutoSetup");
  packageManagerAutoSetup.setDefaultValue(true);
  packageManagerAutoSetup.setLabelText(tr("Package manager auto setup"));
  packageManagerAutoSetup.setToolTip(tr("Add the CMAKE_PROJECT_INCLUDE_BEFORE variable " "pointing to a CMake script that will install dependencies from the conanfile.txt, " "conanfile.py, or vcpkg.json file from the project source directory."));

  registerAspect(&askBeforeReConfigureInitialParams);
  askBeforeReConfigureInitialParams.setSettingsKey("AskReConfigureInitialParams");
  askBeforeReConfigureInitialParams.setDefaultValue(true);
  askBeforeReConfigureInitialParams.setLabelText(tr("Ask before re-configuring with " "initial parameters"));
}

// CMakeSpecificSettingsPage

CMakeSpecificSettingsPage::CMakeSpecificSettingsPage(CMakeSpecificSettings *settings)
{
  setId("CMakeSpecificSettings");
  setDisplayName(::CMakeProjectManager::Internal::CMakeSpecificSettings::tr("CMake"));
  setCategory(ProjectExplorer::Constants::BUILD_AND_RUN_SETTINGS_CATEGORY);
  setSettings(settings);

  setLayouter([settings](QWidget *widget) {
    auto &s = *settings;
    using namespace Layouting;
    Column{Group{Title(::CMakeProjectManager::Internal::CMakeSpecificSettings::tr("Adding Files")), s.afterAddFileSetting}, s.packageManagerAutoSetup, s.askBeforeReConfigureInitialParams, Stretch(),}.attachTo(widget);
  });
}

} // Internal
} // CMakeProjectManager
