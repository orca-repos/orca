// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <QWidget>

QT_BEGIN_NAMESPACE
class QTreeWidget;
class QTreeWidgetItem;
QT_END_NAMESPACE

namespace Utils {
class FancyLineEdit;
}

namespace Orca::Plugin::Core {

class CommandMappingsPrivate;

class CORE_EXPORT CommandMappings : public QWidget {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(CommandMappings)

public:
  explicit CommandMappings(QWidget *parent = nullptr);
  ~CommandMappings() override;

signals:
  auto currentCommandChanged(QTreeWidgetItem *current) -> void;
  auto resetRequested() -> void;

protected:
  virtual auto defaultAction() -> void = 0;
  virtual auto exportAction() -> void {}
  virtual auto importAction() -> void {}
  virtual auto filterColumn(const QString &filter_string, QTreeWidgetItem *item, int column) const -> bool;
  auto filterChanged(const QString &f) -> void;
  auto setImportExportEnabled(bool enabled) const -> void;
  auto setResetVisible(bool visible) const -> void;
  auto commandList() const -> QTreeWidget*;
  auto filterText() const -> QString;
  auto setFilterText(const QString &text) const -> void;
  auto setPageTitle(const QString &s) const -> void;
  auto setTargetHeader(const QString &s) const -> void;
  static auto setModified(QTreeWidgetItem *item, bool modified) -> void;

private:
  auto filter(const QString &filter_string, QTreeWidgetItem *item) -> bool;
  friend class CommandMappingsPrivate;
  CommandMappingsPrivate *d;
};

} // namespace Orca::Plugin::Core
