// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/id.hpp>
#include <utils/settingsaccessor.hpp>

namespace CMakeProjectManager {

class CMakeTool;

namespace Internal {

class CMakeToolSettingsAccessor : public Utils::UpgradingSettingsAccessor {
public:
  CMakeToolSettingsAccessor();

  struct CMakeTools {
    Utils::Id defaultToolId;
    std::vector<std::unique_ptr<CMakeTool>> cmakeTools;
  };

  auto restoreCMakeTools(QWidget *parent) const -> CMakeTools;
  auto saveCMakeTools(const QList<CMakeTool*> &cmakeTools, const Utils::Id &defaultId, QWidget *parent) -> void;

private:
  auto cmakeTools(const QVariantMap &data, bool fromSdk) const -> CMakeTools;
};

} // namespace Internal
} // namespace CMakeProjectManager
