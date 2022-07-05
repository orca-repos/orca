// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtcassert.h"
#include "singleton.h"

#include <QCoreApplication>
#include <QList>
#include <QThread>

#include <unordered_map>

using namespace Utils;

namespace Utils {

// The order of elements reflects dependencies, i.e.
// if B requires A then B will follow A on this list
static QList<Singleton *> s_singletonList;
static QMutex s_mutex;
static std::unordered_map<std::type_index, SingletonStaticData> s_staticDataList;

Singleton::~Singleton()
{
  QMutexLocker locker(&s_mutex);
  s_singletonList.removeAll(this);
}

auto Singleton::addSingleton(Singleton *singleton) -> void
{
  QMutexLocker locker(&s_mutex);
  s_singletonList.append(singleton);
}

auto Singleton::staticData(std::type_index index) -> SingletonStaticData&
{
  QMutexLocker locker(&s_mutex);
  return s_staticDataList[index];
}

// Note: it's caller responsibility to ensure that this function is being called when all other
// threads don't use any singleton. As a good practice: finish all other threads that were using
// singletons before this function is called.
// Some singletons (currently e.g. SshConnectionManager) can work only in main thread,
// so this method should be called from main thread only.
auto Singleton::deleteAll() -> void
{
  QTC_ASSERT(QThread::currentThread() == qApp->thread(), return);
  QList<Singleton*> oldList;
  {
    QMutexLocker locker(&s_mutex);
    oldList = s_singletonList;
    s_singletonList = {};
  }
  // Keep the reverse order when deleting
  while (!oldList.isEmpty())
    delete oldList.takeLast();
}

} // namespace Utils
