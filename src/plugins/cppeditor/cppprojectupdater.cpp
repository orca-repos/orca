// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppprojectupdater.hpp"

#include "cppmodelmanager.hpp"
#include "cppprojectinfogenerator.hpp"
#include "generatedcodemodelsupport.hpp"

#include <core/core-progress-manager.hpp>

#include <projectexplorer/toolchainmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>

#include <QFutureInterface>

using namespace ProjectExplorer;

namespace CppEditor {

CppProjectUpdater::CppProjectUpdater()
{
  connect(&m_generateFutureWatcher, &QFutureWatcher<ProjectInfo>::finished, this, &CppProjectUpdater::onProjectInfoGenerated);
  m_futureSynchronizer.setCancelOnWait(true);
}

CppProjectUpdater::~CppProjectUpdater()
{
  cancel();
}

auto CppProjectUpdater::update(const ProjectUpdateInfo &projectUpdateInfo) -> void
{
  update(projectUpdateInfo, {});
}

auto CppProjectUpdater::update(const ProjectUpdateInfo &projectUpdateInfo, const QList<ProjectExplorer::ExtraCompiler*> &extraCompilers) -> void
{
  // Stop previous update.
  cancel();

  m_extraCompilers = Utils::transform(extraCompilers, [](ExtraCompiler *compiler) {
    return QPointer<ExtraCompiler>(compiler);
  });
  m_projectUpdateInfo = projectUpdateInfo;

  using namespace ProjectExplorer;

  // Run the project info generator in a worker thread and continue if that one is finished.
  auto generateFuture = Utils::runAsync([=](QFutureInterface<ProjectInfo::ConstPtr> &futureInterface) {
    auto fullProjectUpdateInfo = projectUpdateInfo;
    if (fullProjectUpdateInfo.rppGenerator)
      fullProjectUpdateInfo.rawProjectParts = fullProjectUpdateInfo.rppGenerator();
    Internal::ProjectInfoGenerator generator(futureInterface, fullProjectUpdateInfo);
    futureInterface.reportResult(generator.generate());
  });
  m_generateFutureWatcher.setFuture(generateFuture);
  m_futureSynchronizer.addFuture(generateFuture);

  // extra compilers
  for (auto compiler : qAsConst(m_extraCompilers)) {
    if (compiler->isDirty()) {
      QPointer<QFutureWatcher<void>> watcher = new QFutureWatcher<void>;
      // queued connection to delay after the extra compiler updated its result contents,
      // which is also done in the main thread when compiler->run() finished
      connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher] {
        // In very unlikely case the CppProjectUpdater::cancel() could have been
        // invoked after posting the finished() signal and before this handler
        // gets called. In this case the watcher is already deleted.
        if (!watcher)
          return;
        m_projectUpdateFutureInterface->setProgressValue(m_projectUpdateFutureInterface->progressValue() + 1);
        m_extraCompilersFutureWatchers.remove(watcher);
        watcher->deleteLater();
        if (!watcher->isCanceled())
          checkForExtraCompilersFinished();
      }, Qt::QueuedConnection);
      m_extraCompilersFutureWatchers += watcher;
      watcher->setFuture(QFuture<void>(compiler->run()));
      m_futureSynchronizer.addFuture(watcher->future());
    }
  }

  m_projectUpdateFutureInterface.reset(new QFutureInterface<void>);
  m_projectUpdateFutureInterface->setProgressRange(0, m_extraCompilersFutureWatchers.size() + 1 /*generateFuture*/);
  m_projectUpdateFutureInterface->setProgressValue(0);
  m_projectUpdateFutureInterface->reportStarted();
  Orca::Plugin::Core::ProgressManager::addTask(m_projectUpdateFutureInterface->future(), tr("Preparing C++ Code Model"), "CppProjectUpdater");
}

auto CppProjectUpdater::cancel() -> void
{
  if (m_projectUpdateFutureInterface && m_projectUpdateFutureInterface->isRunning())
    m_projectUpdateFutureInterface->reportFinished();
  m_generateFutureWatcher.setFuture({});
  m_isProjectInfoGenerated = false;
  qDeleteAll(m_extraCompilersFutureWatchers);
  m_extraCompilersFutureWatchers.clear();
  m_extraCompilers.clear();
  m_futureSynchronizer.cancelAllFutures();
}

auto CppProjectUpdater::onProjectInfoGenerated() -> void
{
  if (m_generateFutureWatcher.isCanceled() || m_generateFutureWatcher.future().resultCount() < 1)
    return;

  m_projectUpdateFutureInterface->setProgressValue(m_projectUpdateFutureInterface->progressValue() + 1);
  m_isProjectInfoGenerated = true;
  checkForExtraCompilersFinished();
}

auto CppProjectUpdater::checkForExtraCompilersFinished() -> void
{
  if (!m_extraCompilersFutureWatchers.isEmpty() || !m_isProjectInfoGenerated)
    return; // still need to wait

  m_projectUpdateFutureInterface->reportFinished();
  m_projectUpdateFutureInterface.reset();

  QList<ExtraCompiler*> extraCompilers;
  QSet<QString> compilerFiles;
  for (const auto &compiler : qAsConst(m_extraCompilers)) {
    if (compiler) {
      extraCompilers += compiler.data();
      compilerFiles += Utils::transform<QSet>(compiler->targets(), &Utils::FilePath::toString);
    }
  }
  GeneratedCodeModelSupport::update(extraCompilers);
  m_extraCompilers.clear();

  auto updateFuture = CppModelManager::instance()->updateProjectInfo(m_generateFutureWatcher.result(), compilerFiles);
  m_futureSynchronizer.addFuture(updateFuture);
}

namespace Internal {
CppProjectUpdaterFactory::CppProjectUpdaterFactory()
{
  setObjectName("CppProjectUpdaterFactory");
}

auto CppProjectUpdaterFactory::create() -> CppProjectUpdaterInterface*
{
  return new CppProjectUpdater;
}

} // namespace Internal
} // namespace CppEditor
