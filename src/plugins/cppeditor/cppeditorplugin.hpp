// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <extensionsystem/iplugin.hpp>

namespace Utils { class FilePath; }

namespace CppEditor {
class CppCodeModelSettings;

namespace Internal {

class CppEditorPluginPrivate;
class CppFileSettings;
class CppQuickFixAssistProvider;

class CppEditorPlugin : public ExtensionSystem::IPlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.qt-project.Qt.OrcaPlugin" FILE "CppEditor.json")

public:
  CppEditorPlugin();
  ~CppEditorPlugin() override;

  static auto instance() -> CppEditorPlugin*;
  auto quickFixProvider() const -> CppQuickFixAssistProvider*;

  static auto headerSearchPaths() -> const QStringList&;
  static auto sourceSearchPaths() -> const QStringList&;
  static auto headerPrefixes() -> const QStringList&;
  static auto sourcePrefixes() -> const QStringList&;
  static auto clearHeaderSourceCache() -> void;
  static auto licenseTemplatePath() -> Utils::FilePath;
  static auto licenseTemplate() -> QString;
  static auto usePragmaOnce() -> bool;

  auto openDeclarationDefinitionInNextSplit() -> void;
  auto openTypeHierarchy() -> void;
  auto openIncludeHierarchy() -> void;
  auto showPreProcessorDialog() -> void;
  auto renameSymbolUnderCursor() -> void;
  auto switchDeclarationDefinition() -> void;
  auto switchHeaderSource() -> void;
  auto switchHeaderSourceInNextSplit() -> void;

  auto codeModelSettings() -> CppCodeModelSettings*;
  static auto fileSettings() -> CppFileSettings*;

signals:
  auto outlineSortingChanged(bool sort) -> void;
  auto typeHierarchyRequested() -> void;
  auto includeHierarchyRequested() -> void;

private:
  auto initialize(const QStringList &arguments, QString *errorMessage) -> bool override;
  auto extensionsInitialized() -> void override;
  auto createTestObjects() const -> QVector<QObject*> override;

  CppEditorPluginPrivate *d = nullptr;
};

} // namespace Internal
} // namespace CppEditor
