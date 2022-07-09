// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cppprojectupdaterinterface.hpp"
#include "projectinfo.hpp"

#include <projectexplorer/extracompiler.hpp>
#include <utils/futuresynchronizer.hpp>

#include <QFutureWatcher>

namespace CppEditor {

class ProjectInfo;

namespace Internal {

// registered in extensionsystem's object pool for plugins with weak dependency to CppEditor
class CppProjectUpdaterFactory : public QObject {
  Q_OBJECT

public:
  CppProjectUpdaterFactory();

  // keep the namespace, for the type name in the invokeMethod call
  auto create() -> Q_INVOKABLE CppEditor::CppProjectUpdaterInterface*;
};

} // namespace Internal

class CPPEDITOR_EXPORT CppProjectUpdater final : public QObject, public CppProjectUpdaterInterface {
  Q_OBJECT

public:
  CppProjectUpdater();
  ~CppProjectUpdater() override;

  auto update(const ProjectExplorer::ProjectUpdateInfo &projectUpdateInfo) -> void override;
  auto update(const ProjectExplorer::ProjectUpdateInfo &projectUpdateInfo, const QList<ProjectExplorer::ExtraCompiler*> &extraCompilers) -> void;
  auto cancel() -> void override;

private:
  auto onProjectInfoGenerated() -> void;
  auto checkForExtraCompilersFinished() -> void;

  ProjectExplorer::ProjectUpdateInfo m_projectUpdateInfo;
  QList<QPointer<ProjectExplorer::ExtraCompiler>> m_extraCompilers;
  QFutureWatcher<ProjectInfo::ConstPtr> m_generateFutureWatcher;
  bool m_isProjectInfoGenerated = false;
  QSet<QFutureWatcher<void>*> m_extraCompilersFutureWatchers;
  std::unique_ptr<QFutureInterface<void>> m_projectUpdateFutureInterface;
  Utils::FutureSynchronizer m_futureSynchronizer;
};

} // namespace CppEditor
