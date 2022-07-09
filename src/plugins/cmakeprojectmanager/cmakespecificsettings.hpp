// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/dialogs/ioptionspage.hpp>

#include <utils/aspects.hpp>

namespace CMakeProjectManager {
namespace Internal {

enum AfterAddFileAction : int {
  AskUser,
  CopyFilePath,
  NeverCopyFilePath
};

class CMakeSpecificSettings final : public Utils::AspectContainer {
  Q_DECLARE_TR_FUNCTIONS(CMakeProjectManager::Internal::CMakeSpecificSettings)

public:
  CMakeSpecificSettings();

  Utils::SelectionAspect afterAddFileSetting;
  Utils::StringAspect ninjaPath;
  Utils::BoolAspect packageManagerAutoSetup;
  Utils::BoolAspect askBeforeReConfigureInitialParams;
};

class CMakeSpecificSettingsPage final : public Core::IOptionsPage {
public:
  explicit CMakeSpecificSettingsPage(CMakeSpecificSettings *settings);
};

} // Internal
} // CMakeProjectManager
