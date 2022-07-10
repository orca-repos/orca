// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.hpp>

namespace ResourceEditor {
namespace Internal {

class ResourceEditorW;

class ResourceEditorPlugin final : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.orca-repos.orca.plugin" FILE "ResourceEditor.json")

  ~ResourceEditorPlugin() final;

public:
  auto onUndoStackChanged(ResourceEditorW const *editor, bool canUndo, bool canRedo) -> void;

private:
  auto initialize(const QStringList &arguments, QString *errorMessage = nullptr) -> bool final;
  auto extensionsInitialized() -> void override;

  class ResourceEditorPluginPrivate *d = nullptr;
};

} // namespace Internal
} // namespace ResourceEditor
