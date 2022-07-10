// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.hpp>

namespace ProjectExplorer { class Node; }

namespace CMakeProjectManager {
namespace Internal {

class CMakeSpecificSettings;

class CMakeProjectPlugin final : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.orca-repos.orca.plugin" FILE "cmakeprojectmanager.json")

public:
  static auto projectTypeSpecificSettings() -> CMakeSpecificSettings*;

  ~CMakeProjectPlugin();

  #ifdef WITH_TESTS
private slots:
    void testCMakeParser_data();
    void testCMakeParser();

    void testCMakeSplitValue_data();
    void testCMakeSplitValue();

    void testCMakeProjectImporterQt_data();
    void testCMakeProjectImporterQt();

    void testCMakeProjectImporterToolChain_data();
    void testCMakeProjectImporterToolChain();
  #endif

private:
  auto initialize(const QStringList &arguments, QString *errorMessage) -> bool final;
  auto extensionsInitialized() -> void final;

  auto updateContextActions(ProjectExplorer::Node *node) -> void;

  class CMakeProjectPluginPrivate *d = nullptr;
};

} // namespace Internal
} // namespace CMakeProjectManager
