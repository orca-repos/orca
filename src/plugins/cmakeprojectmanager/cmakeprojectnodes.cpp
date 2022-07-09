// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cmakeprojectnodes.hpp"

#include "cmakebuildsystem.hpp"
#include "cmakeprojectconstants.hpp"

#include <constants/android/androidconstants.hpp>
#include <core/fileiconprovider.hpp>
#include <constants/ios/iosconstants.hpp>
#include <projectexplorer/projectexplorerconstants.hpp>
#include <projectexplorer/target.hpp>

#include <utils/qtcassert.hpp>

using namespace ProjectExplorer;

namespace CMakeProjectManager {
namespace Internal {

CMakeInputsNode::CMakeInputsNode(const Utils::FilePath &cmakeLists) : ProjectExplorer::ProjectNode(cmakeLists)
{
  setPriority(Node::DefaultPriority - 10); // Bottom most!
  setDisplayName(QCoreApplication::translate("CMakeFilesProjectNode", "CMake Modules"));
  setIcon(DirectoryIcon(ProjectExplorer::Constants::FILEOVERLAY_MODULES));
  setListInProject(false);
}

CMakeListsNode::CMakeListsNode(const Utils::FilePath &cmakeListPath) : ProjectExplorer::ProjectNode(cmakeListPath)
{
  setIcon(DirectoryIcon(Constants::FILE_OVERLAY_CMAKE));
  setListInProject(false);
}

auto CMakeListsNode::showInSimpleTree() const -> bool
{
  return false;
}

auto CMakeListsNode::visibleAfterAddFileAction() const -> Utils::optional<Utils::FilePath>
{
  return filePath().pathAppended("CMakeLists.txt");
}

CMakeProjectNode::CMakeProjectNode(const Utils::FilePath &directory) : ProjectExplorer::ProjectNode(directory)
{
  setPriority(Node::DefaultProjectPriority + 1000);
  setIcon(DirectoryIcon(ProjectExplorer::Constants::FILEOVERLAY_PRODUCT));
  setListInProject(false);
}

auto CMakeProjectNode::tooltip() const -> QString
{
  return QString();
}

CMakeTargetNode::CMakeTargetNode(const Utils::FilePath &directory, const QString &target) : ProjectExplorer::ProjectNode(directory)
{
  m_target = target;
  setPriority(Node::DefaultProjectPriority + 900);
  setIcon(":/projectexplorer/images/build.png"); // TODO: Use proper icon!
  setListInProject(false);
  setProductType(ProductType::Other);
}

auto CMakeTargetNode::tooltip() const -> QString
{
  return m_tooltip;
}

auto CMakeTargetNode::buildKey() const -> QString
{
  return m_target;
}

auto CMakeTargetNode::buildDirectory() const -> Utils::FilePath
{
  return m_buildDirectory;
}

auto CMakeTargetNode::setBuildDirectory(const Utils::FilePath &directory) -> void
{
  m_buildDirectory = directory;
}

auto CMakeTargetNode::data(Utils::Id role) const -> QVariant
{
  auto value = [this](const QByteArray &key) -> QVariant {
    for (const auto &configItem : m_config) {
      if (configItem.key == key)
        return configItem.value;
    }
    return {};
  };

  auto values = [this](const QByteArray &key) -> QVariant {
    for (const auto &configItem : m_config) {
      if (configItem.key == key)
        return configItem.values;
    }
    return {};
  };

  if (role == Android::Constants::AndroidAbi)
    return value(Android::Constants::ANDROID_ABI);

  if (role == Android::Constants::AndroidAbis)
    return value(Android::Constants::ANDROID_ABIS);

  // TODO: Concerns the variables below. Qt 6 uses target properties which cannot be read
  // by the current mechanism, and the variables start with "Qt_" prefix.

  if (role == Android::Constants::AndroidPackageSourceDir)
    return value(Android::Constants::ANDROID_PACKAGE_SOURCE_DIR);

  if (role == Android::Constants::AndroidExtraLibs)
    return value(Android::Constants::ANDROID_EXTRA_LIBS);

  if (role == Android::Constants::AndroidDeploySettingsFile)
    return value(Android::Constants::ANDROID_DEPLOYMENT_SETTINGS_FILE);

  if (role == Android::Constants::AndroidApplicationArgs)
    return value(Android::Constants::ANDROID_APPLICATION_ARGUMENTS);

  if (role == Android::Constants::ANDROID_ABIS)
    return value(Android::Constants::ANDROID_ABIS);

  if (role == Android::Constants::AndroidSoLibPath)
    return values(Android::Constants::ANDROID_SO_LIBS_PATHS);

  if (role == Android::Constants::AndroidTargets)
    return values("TARGETS_BUILD_PATH");

  if (role == Android::Constants::AndroidApk)
    return {};

  if (role == Ios::Constants::IosTarget) {
    // For some reason the artifact is e.g. "Debug/untitled.app/untitled" which is wrong.
    // It actually is e.g. "Debug-iphonesimulator/untitled.app/untitled".
    // Anyway, the iOS plugin is only interested in the app bundle name without .app.
    return m_artifact.fileName();
  }

  if (role == Ios::Constants::IosBuildDir) {
    // This is a path relative to root build directory.
    // When generating Xcode project, CMake may put here a "${EFFECTIVE_PLATFORM_NAME}" macro,
    // which is expanded by Xcode at build time.
    // To get an actual executable path, iOS plugin replaces this macro with either "-iphoneos"
    // or "-iphonesimulator" depending on the device type (which is unavailable here).

    // dir/target.app/target -> dir
    return m_artifact.parentDir().parentDir().toString();
  }

  if (role == Ios::Constants::IosCmakeGenerator)
    return value("CMAKE_GENERATOR");

  QTC_ASSERT(false, qDebug() << "Unknown role" << role.toString());
  // Better guess than "not present".
  return value(role.toString().toUtf8());
}

auto CMakeTargetNode::setConfig(const CMakeConfig &config) -> void
{
  m_config = config;
}

auto CMakeTargetNode::visibleAfterAddFileAction() const -> Utils::optional<Utils::FilePath>
{
  return filePath().pathAppended("CMakeLists.txt");
}

auto CMakeTargetNode::build() -> void
{
  auto p = getProject();
  auto t = p ? p->activeTarget() : nullptr;
  if (t)
    static_cast<CMakeBuildSystem*>(t->buildSystem())->buildCMakeTarget(displayName());
}

auto CMakeTargetNode::setTargetInformation(const QList<Utils::FilePath> &artifacts, const QString &type) -> void
{
  m_tooltip = QCoreApplication::translate("CMakeTargetNode", "Target type: ") + type + "<br>";
  if (artifacts.isEmpty()) {
    m_tooltip += QCoreApplication::translate("CMakeTargetNode", "No build artifacts");
  } else {
    const auto tmp = Utils::transform(artifacts, &Utils::FilePath::toUserOutput);
    m_tooltip += QCoreApplication::translate("CMakeTargetNode", "Build artifacts:") + "<br>" + tmp.join("<br>");
    m_artifact = artifacts.first();
  }
}

} // Internal
} // CMakeBuildSystem
