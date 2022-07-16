// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "abstractoverviewmodel.hpp"
#include "cppeditorwidget.hpp"

#include <texteditor/ioutlinewidget.hpp>

#include <utils/navigationtreeview.hpp>

#include <QSortFilterProxyModel>

namespace CppEditor {
namespace Internal {

class CppOutlineTreeView : public Utils::NavigationTreeView {
  Q_OBJECT

public:
  CppOutlineTreeView(QWidget *parent);

  auto contextMenuEvent(QContextMenuEvent *event) -> void override;
};

class CppOutlineFilterModel : public QSortFilterProxyModel {
  Q_OBJECT

public:
  CppOutlineFilterModel(AbstractOverviewModel &sourceModel, QObject *parent);
  // QSortFilterProxyModel

  auto filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const -> bool override;
  auto supportedDragActions() const -> Qt::DropActions override;

private:
  AbstractOverviewModel &m_sourceModel;
};

class CppOutlineWidget : public TextEditor::IOutlineWidget {
  Q_OBJECT

public:
  CppOutlineWidget(CppEditorWidget *editor);

  // IOutlineWidget
  auto filterMenuActions() const -> QList<QAction*> override;
  auto setCursorSynchronization(bool syncWithCursor) -> void override;
  auto isSorted() const -> bool override;
  auto setSorted(bool sorted) -> void override;
  auto restoreSettings(const QVariantMap &map) -> void override;
  auto settings() const -> QVariantMap override;

private:
  auto modelUpdated() -> void;
  auto updateSelectionInTree(const QModelIndex &index) -> void;
  auto updateTextCursor(const QModelIndex &index) -> void;
  auto onItemActivated(const QModelIndex &index) -> void;
  auto syncCursor() -> bool;
  
  CppEditorWidget *m_editor;
  CppOutlineTreeView *m_treeView;
  QSortFilterProxyModel *m_proxyModel;

  bool m_enableCursorSync;
  bool m_blockCursorSync;
  bool m_sorted;
};

class CppOutlineWidgetFactory : public TextEditor::IOutlineWidgetFactory {
  Q_OBJECT

public:
  auto supportsEditor(Orca::Plugin::Core::IEditor *editor) const -> bool override;
  auto supportsSorting() const -> bool override { return true; }
  auto createWidget(Orca::Plugin::Core::IEditor *editor) -> TextEditor::IOutlineWidget* override;
};

} // namespace Internal
} // namespace CppEditor
