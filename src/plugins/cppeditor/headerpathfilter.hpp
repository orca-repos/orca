// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "compileroptionsbuilder.hpp"
#include "projectpart.hpp"

#include <utils/filepath.hpp>

namespace CppEditor::Internal {

class HeaderPathFilter {
public:
  HeaderPathFilter(const ProjectPart &projectPart, UseTweakedHeaderPaths useTweakedHeaderPaths = UseTweakedHeaderPaths::Yes, const QString &clangVersion = {}, const Utils::FilePath &clangIncludeDirectory = {}, const QString &projectDirectory = {}, const QString &buildDirectory = {}) : projectPart{projectPart}, clangVersion{clangVersion}, clangFallbackIncludeDirectory{clangIncludeDirectory}, projectDirectory(ensurePathWithSlashEnding(projectDirectory)), buildDirectory(ensurePathWithSlashEnding(buildDirectory)), useTweakedHeaderPaths{useTweakedHeaderPaths} {}

  auto process() -> void;

private:
  auto filterHeaderPath(const ProjectExplorer::HeaderPath &headerPath) -> void;
  auto tweakHeaderPaths() -> void;
  auto addPreIncludesPath() -> void;
  auto isProjectHeaderPath(const QString &path) const -> bool;
  auto removeGccInternalIncludePaths() -> void;
  static auto ensurePathWithSlashEnding(const QString &path) -> QString;

public:
  ProjectExplorer::HeaderPaths builtInHeaderPaths;
  ProjectExplorer::HeaderPaths systemHeaderPaths;
  ProjectExplorer::HeaderPaths userHeaderPaths;
  const ProjectPart &projectPart;
  const QString clangVersion;
  const Utils::FilePath clangFallbackIncludeDirectory;
  const QString projectDirectory;
  const QString buildDirectory;
  const UseTweakedHeaderPaths useTweakedHeaderPaths;
};

} // namespace CppEditor::Internal
