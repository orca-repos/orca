// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "configmodelitemdelegate.hpp"

#include "configmodel.hpp"

#include <utils/pathchooser.hpp>

#include <QCheckBox>

namespace CMakeProjectManager {
namespace Internal {

ConfigModelItemDelegate::ConfigModelItemDelegate(const Utils::FilePath &base, QObject *parent) : QStyledItemDelegate(parent), m_base(base) { }

auto ConfigModelItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const -> QWidget*
{
  if (index.column() == 1) {
    auto data = ConfigModel::dataItemFromIndex(index);
    if (data.type == ConfigModel::DataItem::FILE || data.type == ConfigModel::DataItem::DIRECTORY) {
      auto edit = new Utils::PathChooser(parent);
      edit->setAttribute(Qt::WA_MacSmallSize);
      edit->setFocusPolicy(Qt::StrongFocus);
      edit->setBaseDirectory(m_base);
      edit->setAutoFillBackground(true);
      if (data.type == ConfigModel::DataItem::FILE) {
        edit->setExpectedKind(Utils::PathChooser::File);
        edit->setPromptDialogTitle(tr("Select a file for %1").arg(data.key));
      } else {
        edit->setExpectedKind(Utils::PathChooser::Directory);
        edit->setPromptDialogTitle(tr("Select a directory for %1").arg(data.key));
      }
      return edit;
    } else if (!data.values.isEmpty()) {
      auto edit = new QComboBox(parent);
      edit->setAttribute(Qt::WA_MacSmallSize);
      edit->setFocusPolicy(Qt::StrongFocus);
      for (const auto &s : qAsConst(data.values))
        edit->addItem(s);
      return edit;
    } else if (data.type == ConfigModel::DataItem::BOOLEAN) {
      auto edit = new QCheckBox(parent);
      edit->setFocusPolicy(Qt::StrongFocus);
      return edit;
    } else if (data.type == ConfigModel::DataItem::STRING) {
      auto edit = new QLineEdit(parent);
      edit->setFocusPolicy(Qt::StrongFocus);
      return edit;
    }
  }

  return QStyledItemDelegate::createEditor(parent, option, index);
}

auto ConfigModelItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const -> void
{
  if (index.column() == 1) {
    auto data = ConfigModel::dataItemFromIndex(index);
    if (data.type == ConfigModel::DataItem::FILE || data.type == ConfigModel::DataItem::DIRECTORY) {
      auto edit = static_cast<Utils::PathChooser*>(editor);
      edit->setFilePath(Utils::FilePath::fromUserInput(data.value));
      return;
    } else if (!data.values.isEmpty()) {
      auto edit = static_cast<QComboBox*>(editor);
      edit->setCurrentText(data.value);
      return;
    } else if (data.type == ConfigModel::DataItem::BOOLEAN) {
      auto edit = static_cast<QCheckBox*>(editor);
      edit->setChecked(index.data(Qt::CheckStateRole).toBool());
      edit->setText(data.value);
      return;
    } else if (data.type == ConfigModel::DataItem::STRING) {
      auto edit = static_cast<QLineEdit*>(editor);
      edit->setText(data.value);
      return;
    }
  }
  QStyledItemDelegate::setEditorData(editor, index);
}

auto ConfigModelItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const -> void
{
  if (index.column() == 1) {
    auto data = ConfigModel::dataItemFromIndex(index);
    if (data.type == ConfigModel::DataItem::FILE || data.type == ConfigModel::DataItem::DIRECTORY) {
      auto edit = static_cast<Utils::PathChooser*>(editor);
      if (edit->rawPath() != data.value)
        model->setData(index, edit->filePath().toString(), Qt::EditRole);
      return;
    } else if (!data.values.isEmpty()) {
      auto edit = static_cast<QComboBox*>(editor);
      model->setData(index, edit->currentText(), Qt::EditRole);
      return;
    } else if (data.type == ConfigModel::DataItem::BOOLEAN) {
      auto edit = static_cast<QCheckBox*>(editor);
      model->setData(index, edit->text(), Qt::EditRole);
    } else if (data.type == ConfigModel::DataItem::STRING) {
      auto edit = static_cast<QLineEdit*>(editor);
      model->setData(index, edit->text(), Qt::EditRole);
    }
  }
  QStyledItemDelegate::setModelData(editor, model, index);
}

auto ConfigModelItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const -> QSize
{
  static auto height = -1;
  if (height < 0) {
    const auto setMaxSize = [](const QWidget &w) {
      if (w.sizeHint().height() > height)
        height = w.sizeHint().height();
    };
    QComboBox box;
    box.setAttribute(Qt::WA_MacSmallSize);
    QCheckBox check;
    setMaxSize(box);
    setMaxSize(check);
    // Do not take the path chooser into consideration, because that would make the height
    // larger on Windows, leading to less items displayed, and the size of PathChooser looks
    // "fine enough" as is.
  }
  Q_UNUSED(option)
  Q_UNUSED(index)
  return QSize(100, height);
}

} // namespace Internal
} // namespace CMakeProjectManager

