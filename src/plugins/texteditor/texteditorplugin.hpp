// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include <extensionsystem/iplugin.hpp>

namespace TextEditor {
namespace Internal {

class LineNumberFilter;

class TextEditorPlugin final : public ExtensionSystem::IPlugin {
  Q_OBJECT Q_PLUGIN_METADATA(IID "org.orca-repos.orca.plugin" FILE "texteditor.json")public:
  TextEditorPlugin();
  ~TextEditorPlugin() override;

  static auto instance() -> TextEditorPlugin*;
  static auto lineNumberFilter() -> LineNumberFilter*;
  auto aboutToShutdown() -> ShutdownFlag override;

private:
  auto initialize(const QStringList &arguments, QString *errorMessage) -> bool override;
  auto extensionsInitialized() -> void override;

  class TextEditorPluginPrivate *d = nullptr;

  #ifdef WITH_TESTS
private slots:
    void testSnippetParsing_data();
    void testSnippetParsing();

    void testIndentationClean_data();
    void testIndentationClean();
  #endif
};

} // namespace Internal
} // namespace TextEditor
