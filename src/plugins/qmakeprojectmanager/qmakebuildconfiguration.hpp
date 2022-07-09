// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qmakeprojectmanager_global.hpp"

#include <projectexplorer/buildconfiguration.hpp>
#include <qtsupport/baseqtversion.hpp>

#include <utils/aspects.hpp>

namespace ProjectExplorer {
class FileNode;
class MakeStep;
} // ProjectExplorer

namespace QmakeProjectManager {

class QMakeStep;
class QmakeBuildSystem;
class QmakeProFileNode;

class QMAKEPROJECTMANAGER_EXPORT QmakeBuildConfiguration : public ProjectExplorer::BuildConfiguration {
  Q_OBJECT

public:
  QmakeBuildConfiguration(ProjectExplorer::Target *target, Utils::Id id);
  ~QmakeBuildConfiguration() override;

  auto buildSystem() const -> ProjectExplorer::BuildSystem* final;
  auto setSubNodeBuild(QmakeProFileNode *node) -> void;
  auto subNodeBuild() const -> QmakeProFileNode*;
  auto fileNodeBuild() const -> ProjectExplorer::FileNode*;
  auto setFileNodeBuild(ProjectExplorer::FileNode *node) -> void;
  auto qmakeBuildConfiguration() const -> QtSupport::QtVersion::QmakeBuildConfigs;
  auto setQMakeBuildConfiguration(QtSupport::QtVersion::QmakeBuildConfigs config) -> void;

  /// suffix should be unique
  static auto shadowBuildDirectory(const Utils::FilePath &profilePath, const ProjectExplorer::Kit *k, const QString &suffix, BuildConfiguration::BuildType type) -> Utils::FilePath;
  auto configCommandLineArguments() const -> QStringList;

  // This function is used in a few places.
  // The drawback is that we shouldn't actually depend on them being always there
  // That is generally the stuff that is asked should normally be transferred to
  // QmakeProject *
  // So that we can later enable people to build qmake the way they would like
  auto qmakeStep() const -> QMakeStep*;
  auto qmakeBuildSystem() const -> QmakeBuildSystem*;
  auto makefile() const -> Utils::FilePath;

  enum MakefileState {
    MakefileMatches,
    MakefileForWrongProject,
    MakefileIncompatible,
    MakefileMissing
  };

  auto compareToImportFrom(const Utils::FilePath &makefile, QString *errorString = nullptr) -> MakefileState;
  static auto extractSpecFromArguments(QString *arguments, const Utils::FilePath &directory, const QtSupport::QtVersion *version, QStringList *outArgs = nullptr) -> QString;
  auto toMap() const -> QVariantMap override;
  auto buildType() const -> BuildType override;
  auto addToEnvironment(Utils::Environment &env) const -> void override;
  static auto unalignedBuildDirWarning() -> QString;
  static auto isBuildDirAtSafeLocation(const QString &sourceDir, const QString &buildDir) -> bool;
  auto isBuildDirAtSafeLocation() const -> bool;
  auto separateDebugInfo() const -> Utils::TriState;
  auto forceSeparateDebugInfo(bool sepDebugInfo) -> void;
  auto qmlDebugging() const -> Utils::TriState;
  auto forceQmlDebugging(bool enable) -> void;
  auto useQtQuickCompiler() const -> Utils::TriState;
  auto forceQtQuickCompiler(bool enable) -> void;
  auto runSystemFunction() const -> bool;

signals:
  /// emitted for setQMakeBuildConfig, not emitted for Qt version changes, even
  /// if those change the qmakebuildconfig
  auto qmakeBuildConfigurationChanged() -> void;
  auto separateDebugInfoChanged() -> void;
  auto qmlDebuggingChanged() -> void;
  auto useQtQuickCompilerChanged() -> void;

protected:
  auto fromMap(const QVariantMap &map) -> bool override;
  auto regenerateBuildFiles(ProjectExplorer::Node *node = nullptr) -> bool override;

private:
  auto restrictNextBuild(const ProjectExplorer::RunConfiguration *rc) -> void override;
  auto kitChanged() -> void;
  auto toolChainUpdated(ProjectExplorer::ToolChain *tc) -> void;
  auto qtVersionsChanged(const QList<int> &, const QList<int> &, const QList<int> &changed) -> void;
  auto updateProblemLabel() -> void;
  auto makeStep() const -> ProjectExplorer::MakeStep*;

  class LastKitState {
  public:
    LastKitState();
    explicit LastKitState(ProjectExplorer::Kit *k);
    auto operator ==(const LastKitState &other) const -> bool;
    auto operator !=(const LastKitState &other) const -> bool;
  private:
    int m_qtVersion = -1;
    QByteArray m_toolchain;
    QString m_sysroot;
    QString m_mkspec;
  };

  LastKitState m_lastKitState;
  QtSupport::QtVersion::QmakeBuildConfigs m_qmakeBuildConfiguration;
  QmakeProFileNode *m_subNodeBuild = nullptr;
  ProjectExplorer::FileNode *m_fileNodeBuild = nullptr;
  QmakeBuildSystem *m_buildSystem = nullptr;
};

class QMAKEPROJECTMANAGER_EXPORT QmakeBuildConfigurationFactory : public ProjectExplorer::BuildConfigurationFactory {
public:
  QmakeBuildConfigurationFactory();
};

} // namespace QmakeProjectManager
