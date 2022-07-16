// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakeprojectmanager_global.hpp"
#include "qmakeparsernodes.hpp"

#include <projectexplorer/buildsystem.hpp>
#include <projectexplorer/projectnodes.hpp>

namespace Utils {
class FilePath;
}

namespace QmakeProjectManager {

class QmakeProFileNode;
class QmakeProject;

// Implements ProjectNode for qmake .pri files
class QMAKEPROJECTMANAGER_EXPORT QmakePriFileNode : public ProjectExplorer::ProjectNode {
public:
  QmakePriFileNode(QmakeBuildSystem *buildSystem, QmakeProFileNode *qmakeProFileNode, const Utils::FilePath &filePath, QmakePriFile *pf);

  auto priFile() const -> QmakePriFile*;
  auto showInSimpleTree() const -> bool override { return false; }
  auto canAddSubProject(const Utils::FilePath &proFilePath) const -> bool override;
  auto addSubProject(const Utils::FilePath &proFilePath) -> bool override;
  auto removeSubProject(const Utils::FilePath &proFilePath) -> bool override;
  auto subProjectFileNamePatterns() const -> QStringList override;
  auto addNewInformation(const Utils::FilePaths &files, Node *context) const -> AddNewInformation override;
  auto deploysFolder(const QString &folder) const -> bool override;
  auto proFileNode() const -> QmakeProFileNode*;

protected:
  QPointer<QmakeBuildSystem> m_buildSystem;

private:
  QmakeProFileNode *m_qmakeProFileNode = nullptr;
  QmakePriFile *m_qmakePriFile = nullptr;
};

// Implements ProjectNode for qmake .pro files
class QMAKEPROJECTMANAGER_EXPORT QmakeProFileNode : public QmakePriFileNode {
public:
  QmakeProFileNode(QmakeBuildSystem *buildSystem, const Utils::FilePath &filePath, QmakeProFile *pf);

  auto proFile() const -> QmakeProFile*;
  auto makefile() const -> QString;
  auto objectsDirectory() const -> QString;
  auto objectExtension() const -> QString;
  auto isDebugAndRelease() const -> bool;
  auto isObjectParallelToSource() const -> bool;
  auto isQtcRunnable() const -> bool;
  auto includedInExactParse() const -> bool;
  auto showInSimpleTree() const -> bool override;
  auto buildKey() const -> QString override;
  auto parseInProgress() const -> bool override;
  auto validParse() const -> bool override;
  auto build() -> void override;
  auto targetApplications() const -> QStringList override;
  auto addNewInformation(const Utils::FilePaths &files, Node *context) const -> AddNewInformation override;
  auto data(Utils::Id role) const -> QVariant override;
  auto setData(Utils::Id role, const QVariant &value) const -> bool override;
  auto projectType() const -> QmakeProjectManager::ProjectType;
  auto variableValue(const Variable var) const -> QStringList;
  auto singleVariableValue(const Variable var) const -> QString;
  auto targetInformation() const -> TargetInformation;
  auto showInSimpleTree(ProjectType projectType) const -> bool;
};

} // namespace QmakeProjectManager
