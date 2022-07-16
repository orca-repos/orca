// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cpptoolsreuse.hpp"
#include "cppworkingcopy.hpp"
#include "projectpart.hpp"

#include <projectexplorer/project.hpp>

#include <QFutureInterface>
#include <QObject>
#include <QMutex>

namespace ProjectExplorer {
class Project;
}

namespace CppEditor {

class CPPEDITOR_EXPORT BaseEditorDocumentParser : public QObject {
  Q_OBJECT

public:
  using Ptr = QSharedPointer<BaseEditorDocumentParser>;
  static auto get(const QString &filePath) -> Ptr;

  struct Configuration {
    bool usePrecompiledHeaders = false;
    QByteArray editorDefines;
    QString preferredProjectPartId;

    auto operator==(const Configuration &other) -> bool
    {
      return usePrecompiledHeaders == other.usePrecompiledHeaders && editorDefines == other.editorDefines && preferredProjectPartId == other.preferredProjectPartId;
    }
  };

  struct UpdateParams {
    UpdateParams(const WorkingCopy &workingCopy, const ProjectExplorer::Project *activeProject, Utils::Language languagePreference, bool projectsUpdated) : workingCopy(workingCopy), activeProject(activeProject ? activeProject->projectFilePath() : Utils::FilePath()), languagePreference(languagePreference), projectsUpdated(projectsUpdated) { }

    WorkingCopy workingCopy;
    const Utils::FilePath activeProject;
    Utils::Language languagePreference = Utils::Language::Cxx;
    bool projectsUpdated = false;
  };

  BaseEditorDocumentParser(const QString &filePath);
  ~BaseEditorDocumentParser() override;

  auto filePath() const -> QString;
  auto configuration() const -> Configuration;
  auto setConfiguration(const Configuration &configuration) -> void;
  auto update(const UpdateParams &updateParams) -> void;
  auto update(const QFutureInterface<void> &future, const UpdateParams &updateParams) -> void;
  auto projectPartInfo() const -> ProjectPartInfo;

signals:
  auto projectPartInfoUpdated(const ProjectPartInfo &projectPartInfo) -> void;

protected:
  struct State {
    QByteArray editorDefines;
    ProjectPartInfo projectPartInfo;
  };

  auto state() const -> State;
  auto setState(const State &state) -> void;

  static auto determineProjectPart(const QString &filePath, const QString &preferredProjectPartId, const ProjectPartInfo &currentProjectPartInfo, const Utils::FilePath &activeProject, Utils::Language languagePreference, bool projectsUpdated) -> ProjectPartInfo;

  mutable QMutex m_stateAndConfigurationMutex;

private:
  virtual auto updateImpl(const QFutureInterface<void> &future, const UpdateParams &updateParams) -> void = 0;

  const QString m_filePath;
  Configuration m_configuration;
  State m_state;
  mutable QMutex m_updateIsRunning;
};

} // namespace CppEditor
