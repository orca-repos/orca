// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "qtsupport_global.hpp"

#include <core/core-message-manager.hpp>
#include <proparser/profileevaluator.h>

#include <QObject>
#include <QMap>
#include <QVector>
#include <QTimer>

namespace QtSupport {
namespace Internal {
class QtSupportPlugin;
}

class QTSUPPORT_EXPORT ProMessageHandler : public QMakeHandler {
public:
  ProMessageHandler(bool verbose = true, bool exact = true);
  virtual ~ProMessageHandler();

  auto aboutToEval(ProFile *, ProFile *, EvalFileType) -> void override {}
  auto doneWithEval(ProFile *) -> void override {}
  auto message(int type, const QString &msg, const QString &fileName, int lineNo) -> void override;
  auto fileMessage(int type, const QString &msg) -> void override;
  auto setVerbose(bool on) -> void { m_verbose = on; }
  auto setExact(bool on) -> void { m_exact = on; }

private:
  auto appendMessage(const QString &msg) -> void;

  bool m_verbose;
  bool m_exact;
  QString m_prefix;
  QStringList m_messages;
};

class QTSUPPORT_EXPORT ProFileReader : public ProMessageHandler, public QMakeParser, public ProFileEvaluator {
public:
  ProFileReader(QMakeGlobals *option, QMakeVfs *vfs);
  ~ProFileReader() override;

  auto setCumulative(bool on) -> void;
  auto includeFiles() const -> QHash<ProFile*, QVector<ProFile*>>;
  auto aboutToEval(ProFile *parent, ProFile *proFile, EvalFileType type) -> void override;
  auto doneWithEval(ProFile *parent) -> void override;

private:
  // Tree of ProFiles, mapping from parent to children
  QHash<ProFile*, QVector<ProFile*>> m_includeFiles;
  // One entry per ProFile::ref() call, might contain duplicates
  QList<ProFile*> m_proFiles;
  int m_ignoreLevel;
};

class QTSUPPORT_EXPORT ProFileCacheManager : public QObject {
  Q_OBJECT

public:
  static auto instance() -> ProFileCacheManager* { return s_instance; }
  auto cache() -> ProFileCache*;
  auto discardFiles(const QString &prefix, QMakeVfs *vfs) -> void;
  auto discardFile(const QString &fileName, QMakeVfs *vfs) -> void;
  auto incRefCount() -> void;
  auto decRefCount() -> void;

private:
  ProFileCacheManager(QObject *parent);
  ~ProFileCacheManager() override;

  auto clear() -> void;
  ProFileCache *m_cache = nullptr;
  int m_refCount = 0;
  QTimer m_timer;

  static ProFileCacheManager *s_instance;
  friend class Internal::QtSupportPlugin;
};

} // namespace QtSupport
