/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "qmakenodes.hpp"

#include "qmakeproject.hpp"

#include <projectexplorer/buildconfiguration.hpp>
#include <projectexplorer/runconfiguration.hpp>
#include <projectexplorer/target.hpp>

#include <qtsupport/baseqtversion.hpp>
#include <qtsupport/qtkitinformation.hpp>

#include <resourceeditor/resourcenode.hpp>

#include <utils/qtcassert.hpp>
#include <utils/stringutils.hpp>

#include <android/androidconstants.h>
#include <ios/iosconstants.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

using namespace ProjectExplorer;
using namespace Utils;

using namespace QmakeProjectManager::Internal;

namespace QmakeProjectManager {

/*!
  \class QmakePriFileNode
  Implements abstract ProjectNode class
  */

QmakePriFileNode::QmakePriFileNode(QmakeBuildSystem *buildSystem, QmakeProFileNode *qmakeProFileNode, const FilePath &filePath, QmakePriFile *pf) : ProjectNode(filePath), m_buildSystem(buildSystem), m_qmakeProFileNode(qmakeProFileNode), m_qmakePriFile(pf) { }

auto QmakePriFileNode::priFile() const -> QmakePriFile*
{
  if (!m_buildSystem)
    return nullptr;

  if (!m_buildSystem->isParsing())
    return m_qmakePriFile;

  // During a parsing run the qmakePriFile tree will change, so search for the PriFile and
  // do not depend on the cached value.
  // NOTE: This would go away if the node tree would be per-buildsystem
  return m_buildSystem->rootProFile()->findPriFile(filePath());
}

auto QmakePriFileNode::deploysFolder(const QString &folder) const -> bool
{
  const QmakePriFile *pri = priFile();
  return pri ? pri->deploysFolder(folder) : false;
}

auto QmakePriFileNode::proFileNode() const -> QmakeProFileNode*
{
  return m_qmakeProFileNode;
}

auto QmakeBuildSystem::supportsAction(Node *context, ProjectAction action, const Node *node) const -> bool
{
  if (auto n = dynamic_cast<QmakePriFileNode*>(context)) {
    // Covers QmakeProfile, too.
    if (action == Rename) {
      auto fileNode = node->asFileNode();
      return (fileNode && fileNode->fileType() != FileType::Project) || dynamic_cast<const ResourceEditor::ResourceTopLevelNode*>(node);
    }

    auto t = ProjectType::Invalid;
    const QmakeProFile *pro = nullptr;
    if (hasParsingData()) {
      const FolderNode *folderNode = n;
      const QmakeProFileNode *proFileNode;
      while (!(proFileNode = dynamic_cast<const QmakeProFileNode*>(folderNode))) {
        folderNode = folderNode->parentFolderNode();
        QTC_ASSERT(folderNode, return false);
      }
      QTC_ASSERT(proFileNode, return false);
      pro = proFileNode->proFile();
      QTC_ASSERT(pro, return false);
      t = pro->projectType();
    }

    switch (t) {
    case ProjectType::ApplicationTemplate:
    case ProjectType::StaticLibraryTemplate:
    case ProjectType::SharedLibraryTemplate:
    case ProjectType::AuxTemplate: {
      // TODO: Some of the file types don't make much sense for aux
      // projects (e.g. cpp). It'd be nice if the "add" action could
      // work on a subset of the file types according to project type.
      if (action == AddNewFile)
        return true;
      if (action == EraseFile)
        return pro && pro->knowsFile(node->filePath());
      if (action == RemoveFile)
        return !(pro && pro->knowsFile(node->filePath()));

      auto addExistingFiles = true;
      if (node->isVirtualFolderType()) {
        // A virtual folder, we do what the projectexplorer does
        auto folder = node->asFolderNode();
        if (folder) {
          QStringList list;
          foreach(FolderNode *f, folder->folderNodes())
            list << f->filePath().toString() + QLatin1Char('/');
          if (n->deploysFolder(Utils::commonPath(list)))
            addExistingFiles = false;
        }
      }

      addExistingFiles = addExistingFiles && !n->deploysFolder(node->filePath().toString());

      if (action == AddExistingFile || action == AddExistingDirectory)
        return addExistingFiles;

      break;
    }
    case ProjectType::SubDirsTemplate:
      if (action == AddSubProject || action == AddExistingProject)
        return true;
      break;
    default:
      break;
    }

    return false;
  }

  if (auto n = dynamic_cast<QmakeProFileNode*>(context)) {
    if (action == RemoveSubProject)
      return n->parentProjectNode() && !n->parentProjectNode()->asContainerNode();
  }

  return BuildSystem::supportsAction(context, action, node);
}

auto QmakePriFileNode::canAddSubProject(const FilePath &proFilePath) const -> bool
{
  const QmakePriFile *pri = priFile();
  return pri ? pri->canAddSubProject(proFilePath) : false;
}

auto QmakePriFileNode::addSubProject(const FilePath &proFilePath) -> bool
{
  auto pri = priFile();
  return pri ? pri->addSubProject(proFilePath) : false;
}

auto QmakePriFileNode::removeSubProject(const FilePath &proFilePath) -> bool
{
  auto pri = priFile();
  return pri ? pri->removeSubProjects(proFilePath) : false;
}

auto QmakePriFileNode::subProjectFileNamePatterns() const -> QStringList
{
  return QStringList("*.pro");
}

auto QmakeBuildSystem::addFiles(Node *context, const FilePaths &filePaths, FilePaths *notAdded) -> bool
{
  if (auto n = dynamic_cast<QmakePriFileNode*>(context)) {
    auto pri = n->priFile();
    if (!pri)
      return false;
    auto matchingNodes = n->findNodes([filePaths](const Node *nn) {
      return nn->asFileNode() && filePaths.contains(nn->filePath());
    });
    matchingNodes = filtered(matchingNodes, [](const Node *n) {
      for (const Node *parent = n->parentFolderNode(); parent; parent = parent->parentFolderNode()) {
        if (dynamic_cast<const ResourceEditor::ResourceTopLevelNode*>(parent))
          return false;
      }
      return true;
    });
    auto alreadyPresentFiles = transform(matchingNodes, [](const Node *n) { return n->filePath(); });
    FilePath::removeDuplicates(alreadyPresentFiles);

    auto actualFilePaths = filePaths;
    for (const auto &e : alreadyPresentFiles)
      actualFilePaths.removeOne(e);
    if (notAdded)
      *notAdded = alreadyPresentFiles;
    qCDebug(qmakeNodesLog) << Q_FUNC_INFO << "file paths:" << filePaths << "already present:" << alreadyPresentFiles << "actual file paths:" << actualFilePaths;
    return pri->addFiles(actualFilePaths, notAdded);
  }

  return BuildSystem::addFiles(context, filePaths, notAdded);
}

auto QmakeBuildSystem::removeFiles(Node *context, const FilePaths &filePaths, FilePaths *notRemoved) -> RemovedFilesFromProject
{
  if (auto n = dynamic_cast<QmakePriFileNode*>(context)) {
    const auto pri = n->priFile();
    if (!pri)
      return RemovedFilesFromProject::Error;
    FilePaths wildcardFiles;
    FilePaths nonWildcardFiles;
    for (const auto &file : filePaths) {
      if (pri->proFile()->isFileFromWildcard(file.toString()))
        wildcardFiles << file;
      else
        nonWildcardFiles << file;
    }
    const auto success = pri->removeFiles(nonWildcardFiles, notRemoved);
    if (notRemoved)
      *notRemoved += wildcardFiles;
    if (!success)
      return RemovedFilesFromProject::Error;
    if (!wildcardFiles.isEmpty())
      return RemovedFilesFromProject::Wildcard;
    return RemovedFilesFromProject::Ok;
  }

  return BuildSystem::removeFiles(context, filePaths, notRemoved);
}

auto QmakeBuildSystem::deleteFiles(Node *context, const FilePaths &filePaths) -> bool
{
  if (auto n = dynamic_cast<QmakePriFileNode*>(context)) {
    auto pri = n->priFile();
    return pri ? pri->deleteFiles(filePaths) : false;
  }

  return BuildSystem::deleteFiles(context, filePaths);
}

auto QmakeBuildSystem::canRenameFile(Node *context, const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  if (auto n = dynamic_cast<QmakePriFileNode*>(context)) {
    auto pri = n->priFile();
    return pri ? pri->canRenameFile(oldFilePath, newFilePath) : false;
  }

  return BuildSystem::canRenameFile(context, oldFilePath, newFilePath);
}

auto QmakeBuildSystem::renameFile(Node *context, const FilePath &oldFilePath, const FilePath &newFilePath) -> bool
{
  if (auto n = dynamic_cast<QmakePriFileNode*>(context)) {
    auto pri = n->priFile();
    return pri ? pri->renameFile(oldFilePath, newFilePath) : false;
  }

  return BuildSystem::renameFile(context, oldFilePath, newFilePath);
}

auto QmakeBuildSystem::addDependencies(Node *context, const QStringList &dependencies) -> bool
{
  if (auto n = dynamic_cast<QmakePriFileNode*>(context)) {
    if (const auto pri = n->priFile())
      return pri->addDependencies(dependencies);
    return false;
  }

  return BuildSystem::addDependencies(context, dependencies);
}

auto QmakePriFileNode::addNewInformation(const FilePaths &files, Node *context) const -> FolderNode::AddNewInformation
{
  Q_UNUSED(files)
  return FolderNode::AddNewInformation(filePath().fileName(), context && context->parentProjectNode() == this ? 120 : 90);
}

/*!
  \class QmakeProFileNode
  Implements abstract ProjectNode class
  */
QmakeProFileNode::QmakeProFileNode(QmakeBuildSystem *buildSystem, const FilePath &filePath, QmakeProFile *pf) : QmakePriFileNode(buildSystem, this, filePath, pf)
{
  if (projectType() == ProjectType::ApplicationTemplate) {
    setProductType(ProductType::App);
  } else if (projectType() == ProjectType::SharedLibraryTemplate || projectType() == ProjectType::StaticLibraryTemplate) {
    setProductType(ProductType::Lib);
  } else if (projectType() != ProjectType::SubDirsTemplate) {
    setProductType(ProductType::Other);
  }
}

auto QmakeProFileNode::showInSimpleTree() const -> bool
{
  return showInSimpleTree(projectType()) || m_buildSystem->project()->rootProjectNode() == this;
}

auto QmakeProFileNode::buildKey() const -> QString
{
  return filePath().toString();
}

auto QmakeProFileNode::parseInProgress() const -> bool
{
  auto pro = proFile();
  return !pro || pro->parseInProgress();
}

auto QmakeProFileNode::validParse() const -> bool
{
  auto pro = proFile();
  return pro && pro->validParse();
}

auto QmakeProFileNode::build() -> void
{
  m_buildSystem->buildHelper(QmakeBuildSystem::BUILD, false, this, nullptr);
}

auto QmakeProFileNode::targetApplications() const -> QStringList
{
  QStringList apps;
  if (includedInExactParse() && projectType() == ProjectType::ApplicationTemplate) {
    const auto target = targetInformation().target;
    if (target.startsWith("lib") && target.endsWith(".so"))
      apps << target.mid(3, target.lastIndexOf('.') - 3);
    else
      apps << target;
  }
  return apps;
}

auto QmakeProFileNode::data(Utils::Id role) const -> QVariant
{
  if (role == Android::Constants::AndroidAbis)
    return variableValue(Variable::AndroidAbis);
  if (role == Android::Constants::AndroidAbi)
    return singleVariableValue(Variable::AndroidAbi);
  if (role == Android::Constants::AndroidExtraLibs)
    return variableValue(Variable::AndroidExtraLibs);
  if (role == Android::Constants::AndroidPackageSourceDir)
    return singleVariableValue(Variable::AndroidPackageSourceDir);
  if (role == Android::Constants::AndroidDeploySettingsFile)
    return singleVariableValue(Variable::AndroidDeploySettingsFile);
  if (role == Android::Constants::AndroidSoLibPath) {
    auto info = targetInformation();
    QStringList res = {info.buildDir.toString()};
    auto destDir = info.destDir;
    if (!destDir.isEmpty()) {
      destDir = info.buildDir.resolvePath(destDir.path());
      res.append(destDir.toString());
    }
    res.removeDuplicates();
    return res;
  }

  if (role == Android::Constants::AndroidTargets)
    return {};
  if (role == Android::Constants::AndroidApk)
    return {};

  // We can not use AppMan headers even at build time.
  if (role == "AppmanPackageDir")
    return singleVariableValue(Variable::AppmanPackageDir);
  if (role == "AppmanManifest")
    return singleVariableValue(Variable::AppmanManifest);

  if (role == Ios::Constants::IosTarget) {
    const auto info = targetInformation();
    if (info.valid)
      return info.target;
  }

  if (role == Ios::Constants::IosBuildDir) {
    const auto info = targetInformation();
    if (info.valid)
      return info.buildDir.toString();
  }

  if (role == Ios::Constants::IosCmakeGenerator) {
    // qmake is not CMake, so return empty value
    return {};
  }

  if (role == ProjectExplorer::Constants::QT_KEYWORDS_ENABLED)
    return !proFile()->variableValue(Variable::Config).contains("no_keywords");

  QTC_CHECK(false);
  return {};
}

auto QmakeProFileNode::setData(Utils::Id role, const QVariant &value) const -> bool
{
  auto pro = proFile();
  if (!pro)
    return false;
  QString scope;
  int flags = QmakeProjectManager::Internal::ProWriter::ReplaceValues;
  if (auto target = m_buildSystem->target()) {
    auto version = QtSupport::QtKitAspect::qtVersion(target->kit());
    if (version && !version->supportsMultipleQtAbis()) {
      const auto arch = pro->singleVariableValue(Variable::AndroidAbi);
      scope = QString("contains(%1,%2)").arg(Android::Constants::ANDROID_TARGET_ARCH).arg(arch);
      flags |= QmakeProjectManager::Internal::ProWriter::MultiLine;
    }
  }

  if (role == Android::Constants::AndroidExtraLibs)
    return pro->setProVariable(QLatin1String(Android::Constants::ANDROID_EXTRA_LIBS), value.toStringList(), scope, flags);
  if (role == Android::Constants::AndroidPackageSourceDir)
    return pro->setProVariable(QLatin1String(Android::Constants::ANDROID_PACKAGE_SOURCE_DIR), {value.toString()}, scope, flags);
  if (role == Android::Constants::AndroidApplicationArgs)
    return pro->setProVariable(QLatin1String(Android::Constants::ANDROID_APPLICATION_ARGUMENTS), {value.toString()}, scope, flags);

  return false;
}

auto QmakeProFileNode::proFile() const -> QmakeProFile*
{
  return dynamic_cast<QmakeProFile*>(QmakePriFileNode::priFile());
}

auto QmakeProFileNode::makefile() const -> QString
{
  return singleVariableValue(Variable::Makefile);
}

auto QmakeProFileNode::objectsDirectory() const -> QString
{
  return singleVariableValue(Variable::ObjectsDir);
}

auto QmakeProFileNode::isDebugAndRelease() const -> bool
{
  const auto configValues = variableValue(Variable::Config);
  return configValues.contains(QLatin1String("debug_and_release"));
}

auto QmakeProFileNode::isObjectParallelToSource() const -> bool
{
  return variableValue(Variable::Config).contains("object_parallel_to_source");
}

auto QmakeProFileNode::isQtcRunnable() const -> bool
{
  const auto configValues = variableValue(Variable::Config);
  return configValues.contains(QLatin1String("qtc_runnable"));
}

auto QmakeProFileNode::includedInExactParse() const -> bool
{
  const QmakeProFile *pro = proFile();
  return pro && pro->includedInExactParse();
}

auto QmakeProFileNode::addNewInformation(const FilePaths &files, Node *context) const -> FolderNode::AddNewInformation
{
  Q_UNUSED(files)
  return AddNewInformation(filePath().fileName(), context && context->parentProjectNode() == this ? 120 : 100);
}

auto QmakeProFileNode::showInSimpleTree(ProjectType projectType) const -> bool
{
  return projectType == ProjectType::ApplicationTemplate || projectType == ProjectType::SharedLibraryTemplate || projectType == ProjectType::StaticLibraryTemplate;
}

auto QmakeProFileNode::projectType() const -> ProjectType
{
  const QmakeProFile *pro = proFile();
  return pro ? pro->projectType() : ProjectType::Invalid;
}

auto QmakeProFileNode::variableValue(const Variable var) const -> QStringList
{
  auto pro = proFile();
  return pro ? pro->variableValue(var) : QStringList();
}

auto QmakeProFileNode::singleVariableValue(const Variable var) const -> QString
{
  const auto &values = variableValue(var);
  return values.isEmpty() ? QString() : values.first();
}

auto QmakeProFileNode::objectExtension() const -> QString
{
  auto exts = variableValue(Variable::ObjectExt);
  if (exts.isEmpty())
    return HostOsInfo::isWindowsHost() ? QLatin1String(".obj") : QLatin1String(".o");
  return exts.first();
}

auto QmakeProFileNode::targetInformation() const -> TargetInformation
{
  return proFile() ? proFile()->targetInformation() : TargetInformation();
}

} // namespace QmakeProjectManager
