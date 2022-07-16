// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildpropertiessettings.hpp"

#include "projectexplorerconstants.hpp"

#include <utils/layoutbuilder.hpp>

using namespace Utils;

namespace ProjectExplorer {

// Default directory:
constexpr char DEFAULT_BUILD_DIRECTORY_TEMPLATE[] = "../%{JS: Util.asciify(\"build-%{Project:Name}-%{Kit:FileSystemName}-%{BuildConfig:Name}\")}";

BuildPropertiesSettings::BuildTriStateAspect::BuildTriStateAspect() : TriStateAspect{BuildPropertiesSettings::tr("Enable"), BuildPropertiesSettings::tr("Disable"), BuildPropertiesSettings::tr("Use Project Default")} {}

BuildPropertiesSettings::BuildPropertiesSettings()
{
  setAutoApply(false);

  registerAspect(&buildDirectoryTemplate);
  buildDirectoryTemplate.setDisplayStyle(StringAspect::LineEditDisplay);
  buildDirectoryTemplate.setSettingsKey("Directories/BuildDirectory.TemplateV2");
  buildDirectoryTemplate.setDefaultValue(DEFAULT_BUILD_DIRECTORY_TEMPLATE);
  buildDirectoryTemplate.setLabelText(tr("Default build directory:"));
  buildDirectoryTemplate.setUseGlobalMacroExpander();
  buildDirectoryTemplate.setUseResetButton();

  registerAspect(&buildDirectoryTemplateOld); // TODO: Remove in ~4.16
  buildDirectoryTemplateOld.setSettingsKey("Directories/BuildDirectory.Template");
  buildDirectoryTemplateOld.setDefaultValue(DEFAULT_BUILD_DIRECTORY_TEMPLATE);

  registerAspect(&separateDebugInfo);
  separateDebugInfo.setSettingsKey("ProjectExplorer/Settings/SeparateDebugInfo");
  separateDebugInfo.setLabelText(tr("Separate debug info:"));

  registerAspect(&qmlDebugging);
  qmlDebugging.setSettingsKey("ProjectExplorer/Settings/QmlDebugging");
  qmlDebugging.setLabelText(tr("QML debugging:"));

  registerAspect(&qtQuickCompiler);
  qtQuickCompiler.setSettingsKey("ProjectExplorer/Settings/QtQuickCompiler");
  qtQuickCompiler.setLabelText(tr("Use qmlcachegen:"));

  connect(&showQtSettings, &BoolAspect::valueChanged, &qmlDebugging, &BaseAspect::setVisible);
  connect(&showQtSettings, &BoolAspect::valueChanged, &qtQuickCompiler, &BaseAspect::setVisible);
}

auto BuildPropertiesSettings::readSettings(QSettings *s) -> void
{
  AspectContainer::readSettings(s);

  // TODO: Remove in ~4.16
  auto v = buildDirectoryTemplate.value();
  if (v.isEmpty())
    v = buildDirectoryTemplateOld.value();
  if (v.isEmpty())
    v = DEFAULT_BUILD_DIRECTORY_TEMPLATE;
  v.replace("%{CurrentProject:Name}", "%{Project:Name}");
  v.replace("%{CurrentKit:FileSystemName}", "%{Kit:FileSystemName}");
  v.replace("%{CurrentBuild:Name}", "%{BuildConfig:Name}");
  buildDirectoryTemplate.setValue(v);
}

auto BuildPropertiesSettings::defaultBuildDirectoryTemplate() -> QString
{
  return QString(DEFAULT_BUILD_DIRECTORY_TEMPLATE);
}

namespace Internal {

BuildPropertiesSettingsPage::BuildPropertiesSettingsPage(BuildPropertiesSettings *settings)
{
  setId("AB.ProjectExplorer.BuildPropertiesSettingsPage");
  setDisplayName(BuildPropertiesSettings::tr("Default Build Properties"));
  setCategory(Constants::BUILD_AND_RUN_SETTINGS_CATEGORY);
  setSettings(settings);

  setLayouter([settings](QWidget *widget) {
    auto &s = *settings;
    using namespace Layouting;

    Column{Form{s.buildDirectoryTemplate, s.separateDebugInfo, s.qmlDebugging, s.qtQuickCompiler}, Stretch()}.attachTo(widget);
  });
}

} // Internal
} // ProjectExplorer
