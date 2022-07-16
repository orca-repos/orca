// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <qmakeprojectmanager/qmakestep.hpp>
#include <qtsupport/baseqtversion.hpp>
#include <utils/filepath.hpp>

namespace QmakeProjectManager {
namespace Internal {

struct QMakeAssignment {
  QString variable;
  QString op;
  QString value;
};

class MakeFileParse {
public:
  enum class Mode {
    FilterKnownConfigValues,
    DoNotFilterKnownConfigValues
  };

  MakeFileParse(const Utils::FilePath &makefile, Mode mode);

  enum MakefileState {
    MakefileMissing,
    CouldNotParse,
    Okay
  };

  auto makeFileState() const -> MakefileState;
  auto qmakePath() const -> Utils::FilePath;
  auto srcProFile() const -> Utils::FilePath;
  auto config() const -> QMakeStepConfig;
  auto unparsedArguments() const -> QString;
  auto effectiveBuildConfig(QtSupport::QtVersion::QmakeBuildConfigs defaultBuildConfig) const -> QtSupport::QtVersion::QmakeBuildConfigs;
  static auto logging() -> const QLoggingCategory&;
  auto parseCommandLine(const QString &command, const QString &project) -> void;

private:
  auto parseArgs(const QString &args, const QString &project, QList<QMakeAssignment> *assignments, QList<QMakeAssignment> *afterAssignments) -> void;
  auto parseAssignments(const QList<QMakeAssignment> &assignments) -> QList<QMakeAssignment>;

  class QmakeBuildConfig {
  public:
    bool explicitDebug = false;
    bool explicitRelease = false;
    bool explicitBuildAll = false;
    bool explicitNoBuildAll = false;
  };

  const Mode m_mode;
  MakefileState m_state;
  Utils::FilePath m_qmakePath;
  Utils::FilePath m_srcProFile;

  QmakeBuildConfig m_qmakeBuildConfig;
  QMakeStepConfig m_config;
  QString m_unparsedArguments;
};

} // namespace Internal
} // namespace QmakeProjectManager
