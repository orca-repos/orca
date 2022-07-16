// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qmakesettings.hpp"

#include <core/core-interface.hpp>

#include <projectexplorer/projectexplorerconstants.hpp>

#include <utils/layoutbuilder.hpp>

using namespace Utils;

namespace QmakeProjectManager {
namespace Internal {

QmakeSettings::QmakeSettings()
{
  setAutoApply(false);

  registerAspect(&m_warnAgainstUnalignedBuildDir);
  m_warnAgainstUnalignedBuildDir.setSettingsKey("QmakeProjectManager/WarnAgainstUnalignedBuildDir");
  m_warnAgainstUnalignedBuildDir.setDefaultValue(HostOsInfo::isWindowsHost());
  m_warnAgainstUnalignedBuildDir.setLabelText(tr("Warn if a project's source and " "build directories are not at the same level"));
  m_warnAgainstUnalignedBuildDir.setToolTip(tr("Qmake has subtle bugs that " "can be triggered if source and build directory are not at the same level."));

  registerAspect(&m_alwaysRunQmake);
  m_alwaysRunQmake.setSettingsKey("QmakeProjectManager/AlwaysRunQmake");
  m_alwaysRunQmake.setLabelText(tr("Run qmake on every build"));
  m_alwaysRunQmake.setToolTip(tr("This option can help to prevent failures on " "incremental builds, but might slow them down unnecessarily in the general case."));

  registerAspect(&m_ignoreSystemFunction);
  m_ignoreSystemFunction.setSettingsKey("QmakeProjectManager/RunSystemFunction");
  m_ignoreSystemFunction.setLabelText(tr("Ignore qmake's system() function when parsing a project"));
  m_ignoreSystemFunction.setToolTip(tr("Checking this option avoids unwanted side effects, " "but may result in inexact parsing results."));
  // The settings value has been stored with the opposite meaning for a while.
  // Avoid changing the stored value, but flip it on read/write:
  const auto invertBoolVariant = [](const QVariant &v) { return QVariant(!v.toBool()); };
  m_ignoreSystemFunction.setFromSettingsTransformation(invertBoolVariant);
  m_ignoreSystemFunction.setToSettingsTransformation(invertBoolVariant);

  readSettings(Orca::Plugin::Core::ICore::settings());
}

auto QmakeSettings::warnAgainstUnalignedBuildDir() -> bool
{
  return instance().m_warnAgainstUnalignedBuildDir.value();
}

auto QmakeSettings::alwaysRunQmake() -> bool
{
  return instance().m_alwaysRunQmake.value();
}

auto QmakeSettings::runSystemFunction() -> bool
{
  return !instance().m_ignoreSystemFunction.value(); // Note: negated.
}

auto QmakeSettings::instance() -> QmakeSettings&
{
  static QmakeSettings theSettings;
  return theSettings;
}

class SettingsWidget final : public Orca::Plugin::Core::IOptionsPageWidget {
  Q_DECLARE_TR_FUNCTIONS(QmakeProjectManager::Internal::QmakeSettingsPage)
public:
  SettingsWidget()
  {
    auto &s = QmakeSettings::instance();
    using namespace Layouting;
    Column{s.m_warnAgainstUnalignedBuildDir, s.m_alwaysRunQmake, s.m_ignoreSystemFunction, Stretch()}.attachTo(this);
  }

  auto apply() -> void final
  {
    auto &s = QmakeSettings::instance();
    if (s.isDirty()) {
      s.apply();
      s.writeSettings(Orca::Plugin::Core::ICore::settings());
    }
  }
};

QmakeSettingsPage::QmakeSettingsPage()
{
  setId("K.QmakeProjectManager.QmakeSettings");
  setDisplayName(SettingsWidget::tr("Qmake"));
  setCategory(ProjectExplorer::Constants::BUILD_AND_RUN_SETTINGS_CATEGORY);
  setWidgetCreator([] { return new SettingsWidget; });
}

} // namespace Internal
} // namespace QmakeProjectManager
