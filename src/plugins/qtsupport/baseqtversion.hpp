// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <utils/fileutils.hpp>
#include <utils/macroexpander.hpp>

#include <projectexplorer/abi.hpp>
#include <projectexplorer/task.hpp>

#include <QSet>
#include <QStringList>
#include <QVariantMap>

QT_BEGIN_NAMESPACE
class ProFileEvaluator;
class QMakeGlobals;
QT_END_NAMESPACE

namespace Utils {
class Environment;
class FileInProjectFinder;
} // Utils

namespace ProjectExplorer {
class Kit;
class ToolChain;
class Target;
} // ProjectExplorer

namespace QtSupport {

class QtConfigWidget;
class QtVersion;

class QTSUPPORT_EXPORT QtVersionNumber {
public:
  QtVersionNumber(int ma = -1, int mi = -1, int p = -1);
  QtVersionNumber(const QString &versionString);

  auto features() const -> QSet<Utils::Id>;
  auto matches(int major = -1, int minor = -1, int patch = -1) const -> bool;
  auto operator <(const QtVersionNumber &b) const -> bool;
  auto operator <=(const QtVersionNumber &b) const -> bool;
  auto operator >(const QtVersionNumber &b) const -> bool;
  auto operator >=(const QtVersionNumber &b) const -> bool;
  auto operator !=(const QtVersionNumber &b) const -> bool;
  auto operator ==(const QtVersionNumber &b) const -> bool;

  int majorVersion;
  int minorVersion;
  int patchVersion;
};

namespace Internal {
class QtOptionsPageWidget;
class QtVersionPrivate;
}

class QTSUPPORT_EXPORT QtVersion {
  Q_DECLARE_TR_FUNCTIONS(QtSupport::QtVersion)

public:
  using Predicate = std::function<bool(const QtVersion *)>;

  virtual ~QtVersion();

  virtual auto fromMap(const QVariantMap &map) -> void;
  virtual auto equals(QtVersion *other) -> bool;
  auto isAutodetected() const -> bool;
  auto detectionSource() const -> QString;
  auto displayName() const -> QString;
  auto unexpandedDisplayName() const -> QString;
  auto setUnexpandedDisplayName(const QString &name) -> void;
  // All valid Ids are >= 0
  auto uniqueId() const -> int;
  auto type() const -> QString;
  virtual auto toMap() const -> QVariantMap;
  virtual auto isValid() const -> bool;
  static auto isValidPredicate(const Predicate &predicate = {}) -> Predicate;
  virtual auto invalidReason() const -> QString;
  virtual auto warningReason() const -> QStringList;
  virtual auto description() const -> QString = 0;
  virtual auto toHtml(bool verbose) const -> QString;
  auto qtAbis() const -> ProjectExplorer::Abis;
  auto hasAbi(ProjectExplorer::Abi::OS, ProjectExplorer::Abi::OSFlavor flavor = ProjectExplorer::Abi::UnknownFlavor) const -> bool;
  auto applyProperties(QMakeGlobals *qmakeGlobals) const -> void;
  virtual auto addToEnvironment(const ProjectExplorer::Kit *k, Utils::Environment &env) const -> void;
  auto qmakeRunEnvironment() const -> Utils::Environment;
  // source path defined by qmake property QT_INSTALL_PREFIX/src or by qmake.stash QT_SOURCE_TREE
  auto sourcePath() const -> Utils::FilePath;
  // returns source path for installed qt packages and empty string for self build qt
  auto qtPackageSourcePath() const -> Utils::FilePath;
  auto isInQtSourceDirectory(const Utils::FilePath &filePath) const -> bool;
  auto isQtSubProject(const Utils::FilePath &filePath) const -> bool;
  auto rccFilePath() const -> Utils::FilePath;
  // used by UiCodeModelSupport
  auto uicFilePath() const -> Utils::FilePath;
  auto designerFilePath() const -> Utils::FilePath;
  auto linguistFilePath() const -> Utils::FilePath;
  auto qscxmlcFilePath() const -> Utils::FilePath;
  auto qmlRuntimeFilePath() const -> Utils::FilePath;
  auto qmlplugindumpFilePath() const -> Utils::FilePath;
  auto qtVersionString() const -> QString;
  auto qtVersion() const -> QtVersionNumber;
  auto qtSoPaths() const -> QStringList;
  auto hasExamples() const -> bool;
  auto hasDocs() const -> bool;
  auto hasDemos() const -> bool;
  // former local functions
  auto qmakeFilePath() const -> Utils::FilePath;
  /// @returns the name of the mkspec
  auto mkspec() const -> QString;
  auto mkspecFor(ProjectExplorer::ToolChain *tc) const -> QString;
  /// @returns the full path to the default directory
  /// specifally not the directory the symlink/ORIGINAL_QMAKESPEC points to
  auto mkspecPath() const -> Utils::FilePath;
  auto hasMkspec(const QString &spec) const -> bool;

  enum QmakeBuildConfig {
    NoBuild = 1,
    DebugBuild = 2,
    BuildAll = 8
  };

  Q_DECLARE_FLAGS(QmakeBuildConfigs, QmakeBuildConfig)

  virtual auto defaultBuildConfig() const -> QmakeBuildConfigs;
  /// Check a .pro-file/Qt version combination on possible issues
  /// @return a list of tasks, ordered on severity (errors first, then
  ///         warnings and finally info items.
  auto reportIssues(const QString &proFile, const QString &buildDir) const -> ProjectExplorer::Tasks;
  static auto isQmlDebuggingSupported(const ProjectExplorer::Kit *k, QString *reason = nullptr) -> bool;
  auto isQmlDebuggingSupported(QString *reason = nullptr) const -> bool;
  static auto isQtQuickCompilerSupported(const ProjectExplorer::Kit *k, QString *reason = nullptr) -> bool;
  auto isQtQuickCompilerSupported(QString *reason = nullptr) const -> bool;
  auto hasQmlDumpWithRelocatableFlag() const -> bool;
  virtual auto createConfigurationWidget() const -> QtConfigWidget*;
  auto defaultUnexpandedDisplayName() const -> QString;
  virtual auto targetDeviceTypes() const -> QSet<Utils::Id> = 0;
  virtual auto validateKit(const ProjectExplorer::Kit *k) -> ProjectExplorer::Tasks;
  auto prefix() const -> Utils::FilePath;
  auto binPath() const -> Utils::FilePath;
  auto libExecPath() const -> Utils::FilePath;
  auto configurationPath() const -> Utils::FilePath;
  auto dataPath() const -> Utils::FilePath;
  auto demosPath() const -> Utils::FilePath;
  auto docsPath() const -> Utils::FilePath;
  auto examplesPath() const -> Utils::FilePath;
  auto frameworkPath() const -> Utils::FilePath;
  auto headerPath() const -> Utils::FilePath;
  auto importsPath() const -> Utils::FilePath;
  auto libraryPath() const -> Utils::FilePath;
  auto pluginPath() const -> Utils::FilePath;
  auto qmlPath() const -> Utils::FilePath;
  auto translationsPath() const -> Utils::FilePath;
  auto hostBinPath() const -> Utils::FilePath;
  auto hostLibexecPath() const -> Utils::FilePath;
  auto hostDataPath() const -> Utils::FilePath;
  auto hostPrefixPath() const -> Utils::FilePath;
  auto mkspecsPath() const -> Utils::FilePath;
  auto librarySearchPath() const -> Utils::FilePath;
  auto directoriesToIgnoreInProjectTree() const -> Utils::FilePaths;
  auto qtNamespace() const -> QString;
  auto qtLibInfix() const -> QString;
  auto isFrameworkBuild() const -> bool;
  // Note: A Qt version can have both a debug and a release built at the same time!
  auto hasDebugBuild() const -> bool;
  auto hasReleaseBuild() const -> bool;
  auto macroExpander() const -> Utils::MacroExpander*; // owned by the Qt version
  static auto createMacroExpander(const std::function<const QtVersion *()> &qtVersion) -> std::unique_ptr<Utils::MacroExpander>;
  static auto populateQmlFileFinder(Utils::FileInProjectFinder *finder, const ProjectExplorer::Target *target) -> void;
  auto features() const -> QSet<Utils::Id>;
  virtual auto supportsMultipleQtAbis() const -> bool;

protected:
  QtVersion();
  QtVersion(const QtVersion &other) = delete;

  virtual auto availableFeatures() const -> QSet<Utils::Id>;
  virtual auto reportIssuesImpl(const QString &proFile, const QString &buildDir) const -> ProjectExplorer::Tasks;
  virtual auto detectQtAbis() const -> ProjectExplorer::Abis;
  // helper function for desktop and simulator to figure out the supported abis based on the libraries
  static auto qtAbisFromLibrary(const Utils::FilePaths &coreLibraries) -> ProjectExplorer::Abis;
  auto resetCache() const -> void;
  auto ensureMkSpecParsed() const -> void;
  virtual auto parseMkSpec(ProFileEvaluator *) const -> void;
  virtual auto setupQmakeRunEnvironment(Utils::Environment &env) const -> void;

private:
  auto updateDefaultDisplayName() -> void;

  friend class QtVersionFactory;
  friend class QtVersionManager;
  friend class Internal::QtVersionPrivate;
  friend class Internal::QtOptionsPageWidget;

  auto setId(int id) -> void;
  auto clone() const -> QtVersion*;

  Internal::QtVersionPrivate *d = nullptr;
};

using QtVersions = QList<QtVersion *>;

} // QtSupport

Q_DECLARE_OPERATORS_FOR_FLAGS(QtSupport::QtVersion::QmakeBuildConfigs)
