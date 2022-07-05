// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

static constexpr char activationModeC[] = "ActivationMode";

#include <QListView>
#include <QListWidget>
#include <QTreeView>
#include <QTreeWidget>

#include <QKeyEvent>


namespace Utils {

enum ActivationMode {
  DoubleClickActivation = 0,
  SingleClickActivation = 1,
  PlatformDefaultActivation = 2
};

template <class BaseT>
class View : public BaseT {
public:
  View(QWidget *parent = nullptr) : BaseT(parent) {}

  auto setActivationMode(ActivationMode mode) -> void
  {
    if (mode == PlatformDefaultActivation)
      BaseT::setProperty(activationModeC, QVariant());
    else
      BaseT::setProperty(activationModeC, QVariant(bool(mode)));
  }

  auto activationMode() const -> ActivationMode
  {
    QVariant v = BaseT::property(activationModeC);
    if (!v.isValid())
      return PlatformDefaultActivation;
    return v.toBool() ? SingleClickActivation : DoubleClickActivation;
  }

  auto keyPressEvent(QKeyEvent *event) -> void override
  {
    // Note: This always eats the event
    // whereas QAbstractItemView never eats it
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == 0 && BaseT::currentIndex().isValid() && BaseT::state() != QAbstractItemView::EditingState) {
      emit BaseT::activated(BaseT::currentIndex());
      return;
    }
    BaseT::keyPressEvent(event);
  }

};

class ORCA_UTILS_EXPORT TreeView : public View<QTreeView> {
  Q_OBJECT

public:
  TreeView(QWidget *parent = nullptr) : View<QTreeView>(parent) {}
};

class ORCA_UTILS_EXPORT TreeWidget : public View<QTreeWidget> {
  Q_OBJECT

public:
  TreeWidget(QWidget *parent = nullptr) : View<QTreeWidget>(parent) {}
};

class ORCA_UTILS_EXPORT ListView : public View<QListView> {
  Q_OBJECT

public:
  ListView(QWidget *parent = nullptr) : View<QListView>(parent) {}
};

class ORCA_UTILS_EXPORT ListWidget : public View<QListWidget> {
  Q_OBJECT

public:
  ListWidget(QWidget *parent = nullptr) : View<QListWidget>(parent) {}
};

} // Utils
