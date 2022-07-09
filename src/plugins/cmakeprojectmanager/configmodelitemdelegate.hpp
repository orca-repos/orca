// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/fileutils.hpp>

#include <QComboBox>
#include <QStyledItemDelegate>

namespace CMakeProjectManager {
namespace Internal {

class ConfigModelItemDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  ConfigModelItemDelegate(const Utils::FilePath &base, QObject *parent = nullptr);

  auto createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const -> QWidget* final;
  auto setEditorData(QWidget *editor, const QModelIndex &index) const -> void final;
  auto setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const -> void final;
  auto sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize final;

private:
  Utils::FilePath m_base;
};

} // namespace Internal
} // namespace CMakeProjectManager
