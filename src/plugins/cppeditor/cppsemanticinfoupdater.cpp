// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppsemanticinfoupdater.hpp"

#include "cpplocalsymbols.hpp"
#include "cppmodelmanager.hpp"

#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>

#include <cplusplus/Control.h>
#include <cplusplus/CppDocument.h>
#include <cplusplus/TranslationUnit.h>

#include <QLoggingCategory>

enum {
  debug = 0
};

using namespace CPlusPlus;

static Q_LOGGING_CATEGORY(log, "qtc.cppeditor.semanticinfoupdater", QtWarningMsg)

namespace CppEditor {

class SemanticInfoUpdaterPrivate {
public:
  class FuturizedTopLevelDeclarationProcessor : public TopLevelDeclarationProcessor {
  public:
    explicit FuturizedTopLevelDeclarationProcessor(QFutureInterface<void> &future): m_future(future) {}
    auto processDeclaration(DeclarationAST *) -> bool override { return !isCanceled(); }
    auto isCanceled() -> bool { return m_future.isCanceled(); }
  private:
    QFutureInterface<void> m_future;
  };
  
  explicit SemanticInfoUpdaterPrivate(SemanticInfoUpdater *q);
  ~SemanticInfoUpdaterPrivate();

  auto semanticInfo() const -> SemanticInfo;
  auto setSemanticInfo(const SemanticInfo &semanticInfo, bool emitSignal) -> void;
  auto update(const SemanticInfo::Source &source, bool emitSignalWhenFinished, FuturizedTopLevelDeclarationProcessor *processor) -> SemanticInfo;
  auto reuseCurrentSemanticInfo(const SemanticInfo::Source &source, bool emitSignalWhenFinished) -> bool;
  auto update_helper(QFutureInterface<void> &future, const SemanticInfo::Source &source) -> void;
  
  SemanticInfoUpdater *q;
  mutable QMutex m_lock;
  SemanticInfo m_semanticInfo;
  QFuture<void> m_future;
};

SemanticInfoUpdaterPrivate::SemanticInfoUpdaterPrivate(SemanticInfoUpdater *q) : q(q) {}

SemanticInfoUpdaterPrivate::~SemanticInfoUpdaterPrivate()
{
  m_future.cancel();
  m_future.waitForFinished();
}

auto SemanticInfoUpdaterPrivate::semanticInfo() const -> SemanticInfo
{
  QMutexLocker locker(&m_lock);
  return m_semanticInfo;
}

auto SemanticInfoUpdaterPrivate::setSemanticInfo(const SemanticInfo &semanticInfo, bool emitSignal) -> void
{
  {
    QMutexLocker locker(&m_lock);
    m_semanticInfo = semanticInfo;
  }
  if (emitSignal) {
    qCDebug(log) << "emiting new info";
    emit q->updated(semanticInfo);
  }
}

auto SemanticInfoUpdaterPrivate::update(const SemanticInfo::Source &source, bool emitSignalWhenFinished, FuturizedTopLevelDeclarationProcessor *processor) -> SemanticInfo
{
  SemanticInfo newSemanticInfo;
  newSemanticInfo.revision = source.revision;
  newSemanticInfo.snapshot = source.snapshot;

  Document::Ptr doc = newSemanticInfo.snapshot.preprocessedDocument(source.code, Utils::FilePath::fromString(source.fileName));
  if (processor)
    doc->control()->setTopLevelDeclarationProcessor(processor);
  doc->check();
  if (processor && processor->isCanceled())
    newSemanticInfo.complete = false;
  newSemanticInfo.doc = doc;

  qCDebug(log) << "update() for source revision:" << source.revision << "canceled:" << !newSemanticInfo.complete;

  setSemanticInfo(newSemanticInfo, emitSignalWhenFinished);
  return newSemanticInfo;
}

auto SemanticInfoUpdaterPrivate::reuseCurrentSemanticInfo(const SemanticInfo::Source &source, bool emitSignalWhenFinished) -> bool
{
  const auto currentSemanticInfo = semanticInfo();

  if (!source.force && currentSemanticInfo.complete && currentSemanticInfo.revision == source.revision && currentSemanticInfo.doc && currentSemanticInfo.doc->translationUnit()->ast() && currentSemanticInfo.doc->fileName() == source.fileName && !currentSemanticInfo.snapshot.isEmpty() && currentSemanticInfo.snapshot == source.snapshot) {
    SemanticInfo newSemanticInfo;
    newSemanticInfo.revision = source.revision;
    newSemanticInfo.snapshot = source.snapshot;
    newSemanticInfo.doc = currentSemanticInfo.doc;
    setSemanticInfo(newSemanticInfo, emitSignalWhenFinished);
    qCDebug(log) << "re-using current semantic info, source revision:" << source.revision;
    return true;
  }

  return false;
}

auto SemanticInfoUpdaterPrivate::update_helper(QFutureInterface<void> &future, const SemanticInfo::Source &source) -> void
{
  FuturizedTopLevelDeclarationProcessor processor(future);
  update(source, true, &processor);
}

SemanticInfoUpdater::SemanticInfoUpdater() : d(new SemanticInfoUpdaterPrivate(this)) {}

SemanticInfoUpdater::~SemanticInfoUpdater()
{
  d->m_future.cancel();
  d->m_future.waitForFinished();
}

auto SemanticInfoUpdater::update(const SemanticInfo::Source &source) -> SemanticInfo
{
  qCDebug(log) << "update() - synchronous";
  d->m_future.cancel();

  const auto emitSignalWhenFinished = false;
  if (d->reuseCurrentSemanticInfo(source, emitSignalWhenFinished)) {
    d->m_future = QFuture<void>();
    return semanticInfo();
  }

  return d->update(source, emitSignalWhenFinished, nullptr);
}

auto SemanticInfoUpdater::updateDetached(const SemanticInfo::Source &source) -> void
{
  qCDebug(log) << "updateDetached() - asynchronous";
  d->m_future.cancel();

  const auto emitSignalWhenFinished = true;
  if (d->reuseCurrentSemanticInfo(source, emitSignalWhenFinished)) {
    d->m_future = QFuture<void>();
    return;
  }

  d->m_future = Utils::runAsync(CppModelManager::instance()->sharedThreadPool(), &SemanticInfoUpdaterPrivate::update_helper, d.data(), source);
}

auto SemanticInfoUpdater::semanticInfo() const -> SemanticInfo
{
  return d->semanticInfo();
}

} // namespace CppEditor
