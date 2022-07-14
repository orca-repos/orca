// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-help-manager.hpp"

namespace Orca::Plugin::Core {

class CORE_EXPORT Implementation {
protected:
  Implementation();
  virtual ~Implementation();

public:
  virtual auto registerDocumentation(const QStringList &file_names) -> void = 0;
  virtual auto unregisterDocumentation(const QStringList &file_names) -> void = 0;
  virtual auto linksForIdentifier(const QString &id) -> QMultiMap<QString, QUrl> = 0;
  virtual auto linksForKeyword(const QString &keyword) -> QMultiMap<QString, QUrl> = 0;
  virtual auto fileData(const QUrl &url) -> QByteArray = 0;
  virtual auto showHelpUrl(const QUrl &url, HelpViewerLocation location = HelpModeAlways) -> void = 0;
};

} // namespace Orca::Plugin::Core
