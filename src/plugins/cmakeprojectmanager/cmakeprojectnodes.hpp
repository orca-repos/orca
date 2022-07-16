// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmakeconfigitem.hpp"

#include <projectexplorer/projectnodes.hpp>

namespace CMakeProjectManager {
namespace Internal {

class CMakeInputsNode : public ProjectExplorer::ProjectNode {
public:
  CMakeInputsNode(const Utils::FilePath &cmakeLists);
};

class CMakeListsNode : public ProjectExplorer::ProjectNode {
public:
  CMakeListsNode(const Utils::FilePath &cmakeListPath);

  auto showInSimpleTree() const -> bool final;
  auto visibleAfterAddFileAction() const -> Utils::optional<Utils::FilePath> override;
};

class CMakeProjectNode : public ProjectExplorer::ProjectNode {
public:
  CMakeProjectNode(const Utils::FilePath &directory);

  auto tooltip() const -> QString final;
};

class CMakeTargetNode : public ProjectExplorer::ProjectNode {
public:
  CMakeTargetNode(const Utils::FilePath &directory, const QString &target);

  auto setTargetInformation(const QList<Utils::FilePath> &artifacts, const QString &type) -> void;
  auto tooltip() const -> QString final;
  auto buildKey() const -> QString final;
  auto buildDirectory() const -> Utils::FilePath;
  auto setBuildDirectory(const Utils::FilePath &directory) -> void;
  auto visibleAfterAddFileAction() const -> Utils::optional<Utils::FilePath> override;
  auto build() -> void override;
  auto data(Utils::Id role) const -> QVariant override;
  auto setConfig(const CMakeConfig &config) -> void;

private:
  QString m_tooltip;
  Utils::FilePath m_buildDirectory;
  Utils::FilePath m_artifact;
  CMakeConfig m_config;
};

} // namespace Internal
} // namespace CMakeProjectManager
