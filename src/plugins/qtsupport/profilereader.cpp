// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "profilereader.hpp"

#include <core/icore.hpp>
#include <projectexplorer/taskhub.hpp>

#include <QCoreApplication>
#include <QDebug>

using namespace ProjectExplorer;
using namespace QtSupport;

static auto format(const QString &fileName, int lineNo, const QString &msg) -> QString
{
  if (lineNo > 0)
    return QString::fromLatin1("%1(%2): %3").arg(fileName, QString::number(lineNo), msg);
  else if (!fileName.isEmpty())
    return QString::fromLatin1("%1: %2").arg(fileName, msg);
  else
    return msg;
}

ProMessageHandler::ProMessageHandler(bool verbose, bool exact) : m_verbose(verbose), m_exact(exact), m_prefix(QCoreApplication::translate("ProMessageHandler", "[Inexact] ")) {}

ProMessageHandler::~ProMessageHandler()
{
  if (!m_messages.isEmpty())
    Core::MessageManager::writeFlashing(m_messages);
}

static auto addTask(Task::TaskType type, const QString &description, const Utils::FilePath &file = {}, int line = -1) -> void
{
  QMetaObject::invokeMethod(TaskHub::instance(), [=]() {
    TaskHub::addTask(BuildSystemTask(type, description, file, line));
  });
}

auto ProMessageHandler::message(int type, const QString &msg, const QString &fileName, int lineNo) -> void
{
  if ((type & CategoryMask) == ErrorMessage && ((type & SourceMask) == SourceParser || m_verbose)) {
    // parse error in qmake files
    if (m_exact) {
      addTask(Task::Error, msg, Utils::FilePath::fromString(fileName), lineNo);
    } else {
      appendMessage(format(fileName, lineNo, msg));
    }
  }
}

auto ProMessageHandler::fileMessage(int type, const QString &msg) -> void
{
  // message(), warning() or error() calls in qmake files
  if (!m_verbose)
    return;
  if (m_exact && type == QMakeHandler::ErrorMessage)
    addTask(Task::Error, msg);
  else if (m_exact && type == QMakeHandler::WarningMessage)
    addTask(Task::Warning, msg);
  else
    appendMessage(msg);
}

auto ProMessageHandler::appendMessage(const QString &msg) -> void
{
  m_messages << (m_exact ? msg : m_prefix + msg);
}

ProFileReader::ProFileReader(QMakeGlobals *option, QMakeVfs *vfs) : QMakeParser(ProFileCacheManager::instance()->cache(), vfs, this), ProFileEvaluator(option, this, vfs, this), m_ignoreLevel(0)
{
  setExtraConfigs(QStringList("qtc_run"));
}

ProFileReader::~ProFileReader()
{
  foreach(ProFile *pf, m_proFiles)
    pf->deref();
}

auto ProFileReader::setCumulative(bool on) -> void
{
  setVerbose(!on);
  setExact(!on);
  ProFileEvaluator::setCumulative(on);
}

auto ProFileReader::aboutToEval(ProFile *parent, ProFile *pro, EvalFileType type) -> void
{
  if (m_ignoreLevel || (type != EvalProjectFile && type != EvalIncludeFile)) {
    m_ignoreLevel++;
  } else if (parent) {
    // Skip the actual .pro file, as nobody needs that.
    QVector<ProFile*> &children = m_includeFiles[parent];
    if (!children.contains(pro)) {
      children.append(pro);
      m_proFiles.append(pro);
      pro->ref();
    }
  }
}

auto ProFileReader::doneWithEval(ProFile *) -> void
{
  if (m_ignoreLevel)
    m_ignoreLevel--;
}

auto ProFileReader::includeFiles() const -> QHash<ProFile*, QVector<ProFile*>>
{
  return m_includeFiles;
}

ProFileCacheManager *ProFileCacheManager::s_instance = nullptr;

ProFileCacheManager::ProFileCacheManager(QObject *parent) : QObject(parent)
{
  s_instance = this;
  m_timer.setInterval(5000);
  m_timer.setSingleShot(true);
  connect(&m_timer, &QTimer::timeout, this, &ProFileCacheManager::clear);
}

auto ProFileCacheManager::incRefCount() -> void
{
  ++m_refCount;
  m_timer.stop();
}

auto ProFileCacheManager::decRefCount() -> void
{
  --m_refCount;
  if (!m_refCount)
    m_timer.start();
}

ProFileCacheManager::~ProFileCacheManager()
{
  s_instance = nullptr;
  clear();
}

auto ProFileCacheManager::cache() -> ProFileCache*
{
  if (!m_cache)
    m_cache = new ProFileCache;
  return m_cache;
}

auto ProFileCacheManager::clear() -> void
{
  Q_ASSERT(m_refCount == 0);
  // Just deleting the cache will be safe as long as the sequence of
  // obtaining a cache pointer and using it is atomic as far as the main
  // loop is concerned. Use a shared pointer once this is not true anymore.
  delete m_cache;
  m_cache = nullptr;
}

auto ProFileCacheManager::discardFiles(const QString &prefix, QMakeVfs *vfs) -> void
{
  if (m_cache)
    m_cache->discardFiles(prefix, vfs);
}

auto ProFileCacheManager::discardFile(const QString &fileName, QMakeVfs *vfs) -> void
{
  if (m_cache)
    m_cache->discardFile(fileName, vfs);
}
