// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppquickfixsettings.hpp"
#include <projectexplorer/project.hpp>
#include <utils/fileutils.hpp>

namespace CppEditor {
namespace Internal {

class CppQuickFixProjectsSettings : public QObject {
  Q_OBJECT

public:
  using CppQuickFixProjectsSettingsPtr = QSharedPointer<CppQuickFixProjectsSettings>;

  CppQuickFixProjectsSettings(ProjectExplorer::Project *project);

  auto getSettings() -> CppQuickFixSettings*;
  auto isUsingGlobalSettings() const -> bool;
  auto filePathOfSettingsFile() const -> const Utils::FilePath&;
  static auto getSettings(ProjectExplorer::Project *project) -> CppQuickFixProjectsSettingsPtr;
  static auto getQuickFixSettings(ProjectExplorer::Project *project) -> CppQuickFixSettings*;
  auto searchForCppQuickFixSettingsFile() -> Utils::FilePath;
  auto useGlobalSettings() -> void;
  [[nodiscard]] auto useCustomSettings() -> bool;
  auto resetOwnSettingsToGlobal() -> void;
  auto saveOwnSettings() -> bool;

private:
  auto loadOwnSettingsFromFile() -> void;

  ProjectExplorer::Project *m_project;
  Utils::FilePath m_settingsFile;
  CppQuickFixSettings m_ownSettings;
  bool m_useGlobalSettings;
};

} // namespace Internal
} // namespace CppEditor

Q_DECLARE_METATYPE(QSharedPointer<CppEditor::Internal::CppQuickFixProjectsSettings>)
