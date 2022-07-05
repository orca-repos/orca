// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

//
//  W A R N I N G
//  -------------
//
// This file is mostly copied from Qt code and should not be touched
// unless really needed.
//


//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/qhash.h>
#include <QtCore/qmutex.h>
QT_BEGIN_NAMESPACE
class QFileInfo;
class QIODevice;
class QUrl;
QT_END_NAMESPACE

#include "mimetype.h"
#include "mimetype_p.h"
#include "mimeglobpattern_p.h"

namespace Utils {
namespace Internal {

class MimeProviderBase;

class MimeDatabasePrivate {
public:
  Q_DISABLE_COPY(MimeDatabasePrivate)

  MimeDatabasePrivate();
  ~MimeDatabasePrivate();

  static auto instance() -> MimeDatabasePrivate*;
  auto provider() -> MimeProviderBase*;
  auto setProvider(MimeProviderBase *theProvider) -> void;
  auto defaultMimeType() const -> QString { return m_defaultMimeType; }
  auto inherits(const QString &mime, const QString &parent) -> bool;
  auto allMimeTypes() -> QList<MimeType>;
  auto mimeTypeForName(const QString &nameOrAlias) -> MimeType;
  auto mimeTypeForFileNameAndData(const QString &fileName, QIODevice *device, int *priorityPtr) -> MimeType;
  auto findByData(const QByteArray &data, int *priorityPtr) -> MimeType;
  auto mimeTypeForFileName(const QString &fileName, QString *foundSuffix = nullptr) -> QStringList;

  mutable MimeProviderBase *m_provider;
  const QString m_defaultMimeType;
  QMutex mutex;
  int m_startupPhase = 0;
};

class MimeDatabase {
  Q_DISABLE_COPY(MimeDatabase)

public:
  MimeDatabase();
  ~MimeDatabase();

  auto mimeTypeForName(const QString &nameOrAlias) const -> MimeType;

  enum MatchMode {
    MatchDefault = 0x0,
    MatchExtension = 0x1,
    MatchContent = 0x2
  };

  auto mimeTypeForFile(const QString &fileName, MatchMode mode = MatchDefault) const -> MimeType;
  auto mimeTypeForFile(const QFileInfo &fileInfo, MatchMode mode = MatchDefault) const -> MimeType;
  auto mimeTypesForFileName(const QString &fileName) const -> QList<MimeType>;
  auto mimeTypeForData(const QByteArray &data) const -> MimeType;
  auto mimeTypeForData(QIODevice *device) const -> MimeType;
  auto mimeTypeForUrl(const QUrl &url) const -> MimeType;
  auto mimeTypeForFileNameAndData(const QString &fileName, QIODevice *device) const -> MimeType;
  auto mimeTypeForFileNameAndData(const QString &fileName, const QByteArray &data) const -> MimeType;
  auto suffixForFileName(const QString &fileName) const -> QString;
  auto allMimeTypes() const -> QList<MimeType>;

  // For debugging purposes.
  enum StartupPhase {
    BeforeInitialize,
    PluginsLoading,
    PluginsInitializing,
    // Register up to here.
    PluginsDelayedInitializing,
    // Use from here on.
    UpAndRunning
  };

  static auto setStartupPhase(StartupPhase) -> void;

private:
  Internal::MimeDatabasePrivate *d;
};

} // Internal
} // Utils
