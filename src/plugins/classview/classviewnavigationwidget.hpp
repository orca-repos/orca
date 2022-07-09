// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "classviewtreeitemmodel.hpp"

#include <QList>
#include <QPointer>
#include <QSharedPointer>
#include <QStandardItem>
#include <QToolButton>
#include <QWidget>

namespace Utils {
class NavigationTreeView;
}

namespace ClassView {
namespace Internal {

class NavigationWidgetPrivate;

class NavigationWidget : public QWidget {
  Q_OBJECT

public:
  explicit NavigationWidget(QWidget *parent = nullptr);
  ~NavigationWidget() override;

  auto createToolButtons() -> QList<QToolButton*>;
  auto flatMode() const -> bool;
  auto setFlatMode(bool flatMode) -> void;

signals:
  auto visibilityChanged(bool visibility) -> void;
  auto requestGotoLocations(const QList<QVariant> &locations) -> void;

public:
  auto onItemActivated(const QModelIndex &index) -> void;
  auto onItemDoubleClicked(const QModelIndex &index) -> void;
  auto onDataUpdate(QSharedPointer<QStandardItem> result) -> void;
  auto onFullProjectsModeToggled(bool state) -> void;

protected:
  auto fetchExpandedItems(QStandardItem *item, const QStandardItem *target) const -> void;

  //! implements QWidget::hideEvent
  auto hideEvent(QHideEvent *event) -> void override;
  //! implements QWidget::showEvent
  auto showEvent(QShowEvent *event) -> void override;

private:
  Utils::NavigationTreeView *treeView;
  TreeItemModel *treeModel;
  QPointer<QToolButton> fullProjectsModeButton;
};

} // namespace Internal
} // namespace ClassView
