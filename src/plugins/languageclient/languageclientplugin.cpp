// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "languageclientplugin.hpp"

#include "client.hpp"
#include "languageclientmanager.hpp"

#include <core/core-action-container.hpp>
#include <core/core-action-manager.hpp>

#include <QAction>
#include <QMenu>

namespace LanguageClient {

static LanguageClientPlugin *m_instance = nullptr;

LanguageClientPlugin::LanguageClientPlugin()
{
  m_instance = this;
}

LanguageClientPlugin::~LanguageClientPlugin()
{
  m_instance = nullptr;
}

auto LanguageClientPlugin::instance() -> LanguageClientPlugin*
{
  return m_instance;
}

auto LanguageClientPlugin::initialize(const QStringList & /*arguments*/, QString * /*errorString*/) -> bool
{
  using namespace Orca::Plugin::Core;

  LanguageClientManager::init();
  LanguageClientSettings::registerClientType({Constants::LANGUAGECLIENT_STDIO_SETTINGS_ID, tr("Generic StdIO Language Server"), []() { return new StdIOSettings; }});

  //register actions
  const auto toolsDebugContainer = ActionManager::actionContainer(Orca::Plugin::Core::M_TOOLS_DEBUG);

  const auto inspectAction = new QAction(tr("Inspect Language Clients..."), this);
  connect(inspectAction, &QAction::triggered, this, &LanguageClientManager::showInspector);
  toolsDebugContainer->addAction(ActionManager::registerAction(inspectAction, "LanguageClient.InspectLanguageClients"));

  return true;
}

auto LanguageClientPlugin::extensionsInitialized() -> void
{
  LanguageClientSettings::init();
}

auto LanguageClientPlugin::aboutToShutdown() -> ExtensionSystem::IPlugin::ShutdownFlag
{
  LanguageClientManager::shutdown();
  if (LanguageClientManager::clients().isEmpty())
    return ExtensionSystem::IPlugin::SynchronousShutdown;
  QTC_ASSERT(LanguageClientManager::instance(), return ExtensionSystem::IPlugin::SynchronousShutdown);
  connect(LanguageClientManager::instance(), &LanguageClientManager::shutdownFinished, this, &ExtensionSystem::IPlugin::asynchronousShutdownFinished);
  return ExtensionSystem::IPlugin::AsynchronousShutdown;
}

} // namespace LanguageClient
