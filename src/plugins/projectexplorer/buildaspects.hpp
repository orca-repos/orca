// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"

#include <utils/aspects.hpp>

namespace Utils {
class FilePath;
}

namespace ProjectExplorer {
class BuildConfiguration;

class PROJECTEXPLORER_EXPORT BuildDirectoryAspect : public Utils::StringAspect {
  Q_OBJECT

public:
  explicit BuildDirectoryAspect(const BuildConfiguration *bc);
  ~BuildDirectoryAspect() override;

  auto allowInSourceBuilds(const Utils::FilePath &sourceDir) -> void;
  auto isShadowBuild() const -> bool;
  auto setProblem(const QString &description) -> void;
  auto addToLayout(Utils::LayoutBuilder &builder) -> void override;
  static auto fixupDir(const Utils::FilePath &dir) -> Utils::FilePath;

private:
  auto toMap(QVariantMap &map) const -> void override;
  auto fromMap(const QVariantMap &map) -> void override;
  auto updateProblemLabel() -> void;

  class Private;
  Private *const d;
};

class PROJECTEXPLORER_EXPORT SeparateDebugInfoAspect : public Utils::TriStateAspect {
  Q_OBJECT

public:
  SeparateDebugInfoAspect();
};

} // namespace ProjectExplorer
