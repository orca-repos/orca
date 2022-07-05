// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "promptoverwritedialog.h"

#include <utils/stringutils.h>

#include <QTreeView>
#include <QLabel>
#include <QStandardItem>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QDir>

enum class file_name_role {
  user_role = Qt::UserRole + 1
};

/*!
    \class Core::PromptOverwriteDialog
    \inmodule Orca
    \internal
    \brief The PromptOverwriteDialog class implements a dialog that asks
    users whether they want to overwrite files.

    The dialog displays the common folder and the files in a list where users
    can select the files to overwrite.
*/

static auto fileNameOfItem(const QStandardItem *item) -> QString
{
  return item->data(static_cast<int>(file_name_role::user_role)).toString();
}

namespace Core {

PromptOverwriteDialog::PromptOverwriteDialog(QWidget *parent) : QDialog(parent), m_label(new QLabel), m_view(new QTreeView), m_model(new QStandardItemModel(0, 1, this))
{
  setWindowTitle(tr("Overwrite Existing Files"));
  setModal(true);

  const auto main_layout = new QVBoxLayout(this);
  main_layout->addWidget(m_label);
  m_view->setRootIsDecorated(false);
  m_view->setUniformRowHeights(true);
  m_view->setHeaderHidden(true);
  m_view->setSelectionMode(QAbstractItemView::NoSelection);
  m_view->setModel(m_model);
  main_layout->addWidget(m_view);

  const auto bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
  connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
  main_layout->addWidget(bb);
}

auto PromptOverwriteDialog::setFiles(const QStringList &l) const -> void
{
  // Format checkable list excluding common path
  const auto native_common_path = QDir::toNativeSeparators(Utils::commonPath(l));

  for (const auto &file_name : l) {
    const auto native_file_name = QDir::toNativeSeparators(file_name);
    const int length = native_file_name.size() - native_common_path.size() - 1;
    const auto item = new QStandardItem(native_file_name.right(length));
    item->setData(QVariant(file_name), static_cast<int>(file_name_role::user_role));
    item->setFlags(Qt::ItemIsEnabled);
    item->setCheckable(true);
    item->setCheckState(Qt::Checked);
    m_model->appendRow(item);
  }

  const auto message = tr("The following files already exist in the folder\n%1.\n" "Would you like to overwrite them?").arg(native_common_path);
  m_label->setText(message);
}

auto PromptOverwriteDialog::itemForFile(const QString &f) const -> QStandardItem*
{
  const auto row_count = m_model->rowCount();

  for (auto r = 0; r < row_count; ++r) {
    if (const auto item = m_model->item(r, 0); fileNameOfItem(item) == f)
      return item;
  }

  return nullptr;
}

auto PromptOverwriteDialog::files(const Qt::CheckState cs) const -> QStringList
{
  QStringList result;
  const auto row_count = m_model->rowCount();

  for (auto r = 0; r < row_count; ++r) {
    if (const auto item = m_model->item(r, 0); item->checkState() == cs)
      result.push_back(fileNameOfItem(item));
  }

  return result;
}

auto PromptOverwriteDialog::setFileEnabled(const QString &f, const bool e) const -> void
{
  if (const auto item = itemForFile(f)) {
    auto flags = item->flags();
    if (e)
      flags |= Qt::ItemIsEnabled;
    else
      flags &= ~Qt::ItemIsEnabled;
    item->setFlags(flags);
  }
}

auto PromptOverwriteDialog::isFileEnabled(const QString &f) const -> bool
{
  if (const QStandardItem *item = itemForFile(f))
    return (item->flags() & Qt::ItemIsEnabled);
  return false;
}

auto PromptOverwriteDialog::setFileChecked(const QString &f, const bool e) const -> void
{
  if (const auto item = itemForFile(f))
    item->setCheckState(e ? Qt::Checked : Qt::Unchecked);
}

auto PromptOverwriteDialog::isFileChecked(const QString &f) const -> bool
{
  if (const QStandardItem *item = itemForFile(f))
    return item->checkState() == Qt::Checked;
  return false;
}

} // namespace Core
