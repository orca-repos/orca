// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <QObject>
#include <QMap>

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class CORE_EXPORT Signals : public QObject {
  Q_OBJECT

public:
  static auto instance() -> Signals*;

signals:
  auto setupFinished() -> void;
  auto documentationChanged() -> void;
};

enum HelpViewerLocation {
  SideBySideIfPossible = 0,
  SideBySideAlways = 1,
  HelpModeAlways = 2,
  ExternalHelpAlways = 3
};

CORE_EXPORT auto documentationPath() -> QString;
CORE_EXPORT auto registerDocumentation(const QStringList &file_names) -> void;
CORE_EXPORT auto unregisterDocumentation(const QStringList &file_names) -> void;
CORE_EXPORT auto linksForIdentifier(const QString &id) -> QMultiMap<QString, QUrl>;
CORE_EXPORT auto linksForKeyword(const QString &keyword) -> QMultiMap<QString, QUrl>;
CORE_EXPORT auto fileData(const QUrl &url) -> QByteArray;
CORE_EXPORT auto showHelpUrl(const QUrl &url, HelpViewerLocation location = HelpModeAlways) -> void;
CORE_EXPORT auto showHelpUrl(const QString &url, HelpViewerLocation location = HelpModeAlways) -> void;

} // namespace Orca::Plugin::Core
