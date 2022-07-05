// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <QDialog>

QT_BEGIN_NAMESPACE
class QTreeView;
class QStandardItemModel;
class QStandardItem;
class QLabel;
QT_END_NAMESPACE

namespace Core {

// Documentation inside.
class CORE_EXPORT PromptOverwriteDialog final : public QDialog {
  Q_OBJECT

public:
  explicit PromptOverwriteDialog(QWidget *parent = nullptr);

  auto setFiles(const QStringList &) const -> void;
  auto setFileEnabled(const QString &f, bool e) const -> void;
  auto isFileEnabled(const QString &f) const -> bool;
  auto setFileChecked(const QString &f, bool e) const -> void;
  auto isFileChecked(const QString &f) const -> bool;
  auto checkedFiles() const -> QStringList { return files(Qt::Checked); }
  auto uncheckedFiles() const -> QStringList { return files(Qt::Unchecked); }

private:
  auto itemForFile(const QString &f) const -> QStandardItem*;
  auto files(Qt::CheckState cs) const -> QStringList;

  QLabel *m_label;
  QTreeView *m_view;
  QStandardItemModel *m_model;
};

} // namespace Core
