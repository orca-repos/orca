// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "disassembly-plugin.hpp"

#include "disassembly-factory.hpp"

#include <extensionsystem/pluginmanager.hpp>

namespace Orca::Plugin::Disassembly {
namespace {

class PluginPrivate : public QObject {
public:
  PluginPrivate();
  ~PluginPrivate() override;

private:
  FactoryServiceImpl m_factory_service;
  DisassemblyFactory m_editor_factory;
};

PluginPrivate::PluginPrivate()
{
  ExtensionSystem::PluginManager::addObject(&m_factory_service);
  ExtensionSystem::PluginManager::addObject(&m_editor_factory);
}

PluginPrivate::~PluginPrivate()
{
  ExtensionSystem::PluginManager::removeObject(&m_editor_factory);
  ExtensionSystem::PluginManager::removeObject(&m_factory_service);
}

PluginPrivate *dd = nullptr;

} // namespace

Plugin::~Plugin()
{
    delete dd;
    dd = nullptr;
}

auto Plugin::initialize(const QStringList &, QString *) -> bool
{
  dd = new PluginPrivate;
  return true;
}

auto Plugin::extensionsInitialized() -> void
{

}

} // namespace Orca::Plugin::Disassembly
