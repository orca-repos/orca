// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.hpp>

namespace ProjectExplorer { 
class Project; 
}

namespace QmakeProjectManager {

class QmakeProFileNode;

namespace Internal {

class QmakeProjectManagerPlugin final : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.orca-repos.orca.plugin" FILE "QmakeProjectManager.json")

public:
  ~QmakeProjectManagerPlugin() final;

  #ifdef WITH_TESTS
private slots:
    void testQmakeOutputParsers_data();
    void testQmakeOutputParsers();
    void testMakefileParser_data();
    void testMakefileParser();
  #endif

private:
  auto initialize(const QStringList &arguments, QString *errorMessage) -> bool final;

  class QmakeProjectManagerPluginPrivate *d = nullptr;
};

} // namespace Internal
} // namespace QmakeProjectManager
