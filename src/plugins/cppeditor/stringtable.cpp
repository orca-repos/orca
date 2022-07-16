// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "stringtable.hpp"

#include <utils/qtcassert.hpp>
#include <utils/runextensions.hpp>

#include <QDebug>
#include <QElapsedTimer>
#include <QMutex>
#include <QSet>
#include <QThreadPool>
#include <QTimer>

#ifdef WITH_TESTS
#include <extensionsystem/pluginmanager.hpp>
#endif

namespace CppEditor::Internal {

enum {
  GCTimeOut = 10 * 1000 // 10 seconds
};

enum {
  DebugStringTable = 0
};

class StringTablePrivate : public QObject {
public:
  StringTablePrivate();
  ~StringTablePrivate() override { cancelAndWait(); }

  auto cancelAndWait() -> void;
  auto insert(const QString &string) -> QString;
  auto startGC() -> void;
  auto GC(QFutureInterface<void> &futureInterface) -> void;

  QFuture<void> m_future;
  QMutex m_lock;
  QSet<QString> m_strings;
  QTimer m_gcCountDown;
};

static StringTablePrivate *m_instance = nullptr;

StringTablePrivate::StringTablePrivate()
{
  m_strings.reserve(1000);

  m_gcCountDown.setObjectName(QLatin1String("StringTable::m_gcCountDown"));
  m_gcCountDown.setSingleShot(true);
  m_gcCountDown.setInterval(GCTimeOut);
  connect(&m_gcCountDown, &QTimer::timeout, this, &StringTablePrivate::startGC);
}

auto StringTable::insert(const QString &string) -> QString
{
  return m_instance->insert(string);
}

auto StringTablePrivate::cancelAndWait() -> void
{
  if (!m_future.isRunning())
    return;
  m_future.cancel();
  m_future.waitForFinished();
}

auto StringTablePrivate::insert(const QString &string) -> QString
{
  if (string.isEmpty())
    return string;

  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  #ifndef QT_NO_UNSHARABLE_CONTAINERS
    QTC_ASSERT(const_cast<QString&>(string).data_ptr()->ref.isSharable(), return string);
  #endif
  #endif

  QMutexLocker locker(&m_lock);
  // From this point of time any possible new call to startGC() will be held until
  // we finish this function. So we are sure that after canceling the running GC() method now,
  // no new call to GC() will be executed until we finish this function.
  cancelAndWait();
  // A possibly running GC() thread already finished, so it's safe to modify m_strings from
  // now until we unlock the mutex.
  return *m_strings.insert(string);
}

auto StringTablePrivate::startGC() -> void
{
  QMutexLocker locker(&m_lock);
  cancelAndWait();
  m_future = Utils::runAsync(&StringTablePrivate::GC, this);
}

auto StringTable::scheduleGC() -> void
{
  QMetaObject::invokeMethod(&m_instance->m_gcCountDown, QOverload<>::of(&QTimer::start), Qt::QueuedConnection);
}

StringTable::StringTable()
{
  m_instance = new StringTablePrivate;
}

StringTable::~StringTable()
{
  delete m_instance;
  m_instance = nullptr;
}

static auto isQStringInUse(const QString &string) -> bool
{
  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    auto data_ptr = const_cast<QString&>(string).data_ptr();
    return data_ptr->ref.isShared() || data_ptr->ref.isStatic() /* QStringLiteral ? */;
  #else
  auto data_ptr = const_cast<QString&>(string).data_ptr();
  return data_ptr->isShared() || !data_ptr->isMutable() /* QStringLiteral ? */;
  #endif
}

auto StringTablePrivate::GC(QFutureInterface<void> &futureInterface) -> void
{
  #ifdef WITH_TESTS
    if (ExtensionSystem::PluginManager::isScenarioRunning("TestStringTable")) {
        if (ExtensionSystem::PluginManager::finishScenario())
            QThread::currentThread()->sleep(5);
    }
  #endif

  auto initialSize = 0;
  QElapsedTimer timer;
  if (DebugStringTable) {
    initialSize = m_strings.size();
    timer.start();
  }

  // Collect all QStrings which have refcount 1. (One reference in m_strings and nowhere else.)
  for (auto i = m_strings.begin(); i != m_strings.end();) {
    if (futureInterface.isCanceled())
      return;

    if (!isQStringInUse(*i))
      i = m_strings.erase(i);
    else
      ++i;
  }

  if (DebugStringTable) {
    const int currentSize = m_strings.size();
    qDebug() << "StringTable::GC removed" << initialSize - currentSize << "strings in" << timer.elapsed() << "ms, size is now" << currentSize;
  }
}

} // CppEditor::Internal
