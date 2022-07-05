// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QComboBox>
#include <QTreeView>

namespace Utils {

class ORCA_UTILS_EXPORT TreeViewComboBoxView : public QTreeView {
  Q_OBJECT
public:
  TreeViewComboBoxView(QWidget *parent = nullptr);

  auto adjustWidth(int width) -> void;
};

class ORCA_UTILS_EXPORT TreeViewComboBox : public QComboBox {
  Q_OBJECT
public:
  TreeViewComboBox(QWidget *parent = nullptr);

  auto wheelEvent(QWheelEvent *e) -> void override;
  auto keyPressEvent(QKeyEvent *e) -> void override;
  auto setCurrentIndex(const QModelIndex &index) -> void;
  auto eventFilter(QObject *object, QEvent *event) -> bool override;
  auto showPopup() -> void override;
  auto hidePopup() -> void override;
  auto view() const -> TreeViewComboBoxView*;

private:
  auto indexBelow(QModelIndex index) -> QModelIndex;
  auto indexAbove(QModelIndex index) -> QModelIndex;
  auto lastIndex(const QModelIndex &index) -> QModelIndex;

  TreeViewComboBoxView *m_view;
  bool m_skipNextHide = false;
};

}
