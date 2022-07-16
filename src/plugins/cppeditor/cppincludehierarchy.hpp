// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-navigation-widget-factory-interface.hpp>
#include <utils/treemodel.hpp>

#include <QSet>

namespace CppEditor {
namespace Internal {

class CppIncludeHierarchyItem;

class CppIncludeHierarchyModel : public Utils::TreeModel<CppIncludeHierarchyItem> {
  Q_OBJECT
  using base_type = Utils::TreeModel<CppIncludeHierarchyItem>;

public:
  CppIncludeHierarchyModel();

  auto supportedDragActions() const -> Qt::DropActions override;
  auto mimeTypes() const -> QStringList override;
  auto mimeData(const QModelIndexList &indexes) const -> QMimeData* override;
  auto buildHierarchy(const QString &filePath) -> void;
  auto editorFilePath() const -> QString { return m_editorFilePath; }
  auto setSearching(bool on) -> void;
  auto toString() const -> QString;

  #if WITH_TESTS
    using base_type::canFetchMore;
    using base_type::fetchMore;
  #endif

private:
  friend class CppIncludeHierarchyItem;
  QString m_editorFilePath;
  QSet<QString> m_seen;
  bool m_searching = false;
};

class CppIncludeHierarchyFactory : public Orca::Plugin::Core::INavigationWidgetFactory {
  Q_OBJECT

public:
  CppIncludeHierarchyFactory();

  auto createWidget() -> Orca::Plugin::Core::NavigationView override;
  auto saveSettings(Utils::QtcSettings *settings, int position, QWidget *widget) -> void override;
  auto restoreSettings(QSettings *settings, int position, QWidget *widget) -> void override;
};

} // namespace Internal
} // namespace CppEditor
