// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "addtovcsdialog.h"
#include "ui_addtovcsdialog.h"

#include <QListWidgetItem>

namespace Core {
namespace Internal {

AddToVcsDialog::AddToVcsDialog(QWidget *parent, const QString &title, const Utils::FilePaths &files, const QString &vcs_display_name) : QDialog(parent), ui(new Ui::AddToVcsDialog)
{
  ui->setupUi(this);
  const auto add_to = files.size() == 1 ? tr("Add the file to version control (%1)").arg(vcs_display_name) : tr("Add the files to version control (%1)").arg(vcs_display_name);

  ui->addFilesLabel->setText(add_to);
  setWindowTitle(title);

  for (const auto &file : files) {
    const auto item = new QListWidgetItem(file.toUserOutput());
    ui->filesListWidget->addItem(item);
  }
}

AddToVcsDialog::~AddToVcsDialog()
{
  delete ui;
}

} // namespace Internal
} // namespace Core
