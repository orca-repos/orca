// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "languageclientmanager.hpp"
#include "languageclientoutline.hpp"
#include "languageclientsettings.hpp"

#include <extensionsystem/iplugin.hpp>

namespace LanguageClient {

class LanguageClientPlugin : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.orca-repos.orca.plugin" FILE "LanguageClient.json")

public:
  LanguageClientPlugin();
  ~LanguageClientPlugin() override;

  static auto instance() -> LanguageClientPlugin*;

  // IPlugin interface
private:
  auto initialize(const QStringList &arguments, QString *errorString) -> bool override;
  auto extensionsInitialized() -> void override;
  auto aboutToShutdown() -> ShutdownFlag override;
  
  LanguageClientOutlineWidgetFactory m_outlineFactory;

  #ifdef WITH_TESTS
private slots:
    void testSnippetParsing_data();
    void testSnippetParsing();
  #endif
};

} // namespace LanguageClient
