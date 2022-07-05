// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "removefiledialog.h"
#include "ui_removefiledialog.h"

#include <utils/filepath.h>

namespace Utils {

RemoveFileDialog::RemoveFileDialog(const FilePath &filePath, QWidget *parent) : QDialog(parent), m_ui(new Ui::RemoveFileDialog)
{
  m_ui->setupUi(this);
  m_ui->fileNameLabel->setText(filePath.toUserOutput());

  // TODO
  m_ui->removeVCCheckBox->setVisible(false);
}

RemoveFileDialog::~RemoveFileDialog()
{
  delete m_ui;
}

auto RemoveFileDialog::setDeleteFileVisible(bool visible) -> void
{
  m_ui->deleteFileCheckBox->setVisible(visible);
}

auto RemoveFileDialog::isDeleteFileChecked() const -> bool
{
  return m_ui->deleteFileCheckBox->isChecked();
}

} // Utils
