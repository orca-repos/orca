// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>
#include <QSharedPointer>
#include <QStandardItem>

#include <cplusplus/CppDocument.h>
#include <utils/id.hpp>

#include "classviewparsertreeitem.hpp"

namespace ClassView {
namespace Internal {

class ManagerPrivate;

class Manager : public QObject {
  Q_OBJECT

public:
  explicit Manager(QObject *parent = nullptr);
  ~Manager() override;

  static auto instance() -> Manager*;

  auto canFetchMore(QStandardItem *item, bool skipRoot = false) const -> bool;
  auto fetchMore(QStandardItem *item, bool skipRoot = false) -> void;
  auto hasChildren(QStandardItem *item) const -> bool;
  auto gotoLocation(const QString &fileName, int line = 0, int column = 0) -> void;
  auto gotoLocations(const QList<QVariant> &locations) -> void;
  auto setFlatMode(bool flat) -> void;
  auto onWidgetVisibilityIsChanged(bool visibility) -> void;

signals:
  auto treeDataUpdate(QSharedPointer<QStandardItem> result) -> void;

private:
  auto initialize() -> void;
  inline auto state() const -> bool;
  auto setState(bool state) -> void;

  ManagerPrivate *d;
};

} // namespace Internal
} // namespace ClassView
