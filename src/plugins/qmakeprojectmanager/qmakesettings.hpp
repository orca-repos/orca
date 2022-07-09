// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/dialogs/ioptionspage.hpp>

#include <utils/aspects.hpp>

namespace QmakeProjectManager {
namespace Internal {

class QmakeSettings : public Utils::AspectContainer {
  Q_OBJECT

public:
  static auto instance() -> QmakeSettings&;
  static auto warnAgainstUnalignedBuildDir() -> bool;
  static auto alwaysRunQmake() -> bool;
  static auto runSystemFunction() -> bool;

signals:
  auto settingsChanged() -> void;

private:
  QmakeSettings();
  friend class SettingsWidget;

  Utils::BoolAspect m_warnAgainstUnalignedBuildDir;
  Utils::BoolAspect m_alwaysRunQmake;
  Utils::BoolAspect m_ignoreSystemFunction;
};

class QmakeSettingsPage final : public Core::IOptionsPage {
public:
  QmakeSettingsPage();
};

} // namespace Internal
} // namespace QmakeProjectManager
