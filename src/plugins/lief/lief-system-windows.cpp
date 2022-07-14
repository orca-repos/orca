// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lief-system-windows.hpp"

#include <LIEF/PE/Parser.hpp>

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressDialog>

namespace Orca::Plugin::LIEF {

auto Windows::create(QWidget *parent, const Core::WizardDialogParameters &params) const -> Core::BaseFileWizard*
{
  const auto format = QFileDialog::getOpenFileName(nullptr, "Potable Executable (PE) Format.", "", "Portable Executable (PE) Format (*.exe)");
  if (format.isEmpty())
    return {};

  binary = ::LIEF::PE::Parser::parse(format.toStdString());
  if (!binary) {
    QMessageBox(QMessageBox::Warning, "Error", "Failed to parse format").exec();
    return {};
  }

  // Move to generateFiles

  QProgressDialog progress_dialog;
  progress_dialog.setWindowTitle("LIEF");
  progress_dialog.setRange(0, 0);
  progress_dialog.setModal(true);
  progress_dialog.show();

  return {};
}

auto Windows::generateFiles(const QWizard *wizard, QString *error_message) const -> Core::GeneratedFiles
{
  return {};
}

auto Windows::postGenerateFiles(const QWizard *wizard, const Core::GeneratedFiles &files, QString *error_message) const -> bool
{
  return {};
}

} // namespace Orca::Plugin::LIEF
