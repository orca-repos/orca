// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.hpp"

#include <QObject>
#include <QMap>

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

namespace Core {
namespace HelpManager {

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

auto documentationPath() -> CORE_EXPORT QString;
auto registerDocumentation(const QStringList &file_names) -> CORE_EXPORT void;
auto unregisterDocumentation(const QStringList &file_names) -> CORE_EXPORT void;
auto linksForIdentifier(const QString &id) -> CORE_EXPORT QMultiMap<QString, QUrl>;
auto linksForKeyword(const QString &keyword) -> CORE_EXPORT QMultiMap<QString, QUrl>;
auto fileData(const QUrl &url) -> CORE_EXPORT QByteArray;
auto showHelpUrl(const QUrl &url, HelpViewerLocation location = HelpModeAlways) -> CORE_EXPORT void;
auto showHelpUrl(const QString &url, HelpViewerLocation location = HelpModeAlways) -> CORE_EXPORT void;

} // HelpManager
} // Core
