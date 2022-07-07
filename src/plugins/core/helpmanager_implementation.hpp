// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "helpmanager.hpp"

namespace Core {
namespace HelpManager {

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

} // HelpManager
} // Core
