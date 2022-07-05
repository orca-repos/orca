// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "extensionsystem_global.h"

#include <utils/treemodel.h>

#include <QWidget>

namespace Utils {
class CategorySortFilterModel;
class TreeView;
} // Utils

namespace ExtensionSystem {

class PluginSpec;

namespace Internal {
class CollectionItem;
class PluginItem;
} // Internal

class EXTENSIONSYSTEM_EXPORT PluginView : public QWidget {
  Q_OBJECT

public:
  explicit PluginView(QWidget *parent = nullptr);
  ~PluginView() override;

  auto currentPlugin() const -> PluginSpec*;
  auto setFilter(const QString &filter) -> void;

signals:
  auto currentPluginChanged(ExtensionSystem::PluginSpec *spec) -> void;
  auto pluginActivated(ExtensionSystem::PluginSpec *spec) -> void;
  auto pluginSettingsChanged(ExtensionSystem::PluginSpec *spec) -> void;

private:
  auto pluginForIndex(const QModelIndex &index) const -> PluginSpec*;
  auto updatePlugins() -> void;
  auto setPluginsEnabled(const QSet<PluginSpec*> &plugins, bool enable) -> bool;

  Utils::TreeView *m_categoryView;
  Utils::TreeModel<Utils::TreeItem, Internal::CollectionItem, Internal::PluginItem> *m_model;
  Utils::CategorySortFilterModel *m_sortModel;

  friend class Internal::CollectionItem;
  friend class Internal::PluginItem;
};

} // namespae ExtensionSystem
