// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/kitmanager.hpp>

namespace QmakeProjectManager {
namespace Internal {

class QmakeKitAspect : public ProjectExplorer::KitAspect {
  Q_OBJECT

public:
  QmakeKitAspect();

  auto validate(const ProjectExplorer::Kit *k) const -> ProjectExplorer::Tasks override;
  auto createConfigWidget(ProjectExplorer::Kit *k) const -> ProjectExplorer::KitAspectWidget* override;
  auto toUserOutput(const ProjectExplorer::Kit *k) const -> ItemList override;
  auto addToMacroExpander(ProjectExplorer::Kit *kit, Utils::MacroExpander *expander) const -> void override;
  static auto id() -> Utils::Id;

  enum class MkspecSource {
    User,
    Code
  };

  static auto setMkspec(ProjectExplorer::Kit *k, const QString &mkspec, MkspecSource source) -> void;
  static auto mkspec(const ProjectExplorer::Kit *k) -> QString;
  static auto effectiveMkspec(const ProjectExplorer::Kit *k) -> QString;
  static auto defaultMkspec(const ProjectExplorer::Kit *k) -> QString;
};

} // namespace Internal
} // namespace QmakeProjectManager
