// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "openwithdialog.h"

#include <utils/fileutils.h>

#include <QPushButton>

namespace Core {
namespace Internal {

OpenWithDialog::OpenWithDialog(const Utils::FilePath &file_path, QWidget *parent) : QDialog(parent)
{
  setupUi(this);

  label->setText(tr("Open file \"%1\" with:").arg(file_path.fileName()));
  buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);

  connect(buttonBox->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, this, &QDialog::accept);
  connect(buttonBox->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, this, &QDialog::reject);
  connect(editorListWidget, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
  connect(editorListWidget, &QListWidget::currentItemChanged, this, &OpenWithDialog::currentItemChanged);

  setOkButtonEnabled(false);
}

auto OpenWithDialog::setOkButtonEnabled(const bool v) const -> void
{
  buttonBox->button(QDialogButtonBox::Ok)->setEnabled(v);
}

auto OpenWithDialog::setEditors(const QStringList &editors) const -> void
{
  foreach(const QString &e, editors)
    editorListWidget->addItem(e);
}

auto OpenWithDialog::editor() const -> int
{
  return editorListWidget->currentRow();
}

auto OpenWithDialog::setCurrentEditor(const int index) const -> void
{
  editorListWidget->setCurrentRow(index);
}

auto OpenWithDialog::currentItemChanged(const QListWidgetItem *current, QListWidgetItem *) const -> void
{
  setOkButtonEnabled(current);
}

} // namespace Internal
} // namespace Core
