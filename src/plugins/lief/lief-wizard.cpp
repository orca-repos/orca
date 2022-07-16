// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lief-wizard.hpp"

#include "LIEF/Abstract/Parser.hpp"

#include <QFileDialog>
#include <QMessageBox>

// TODO (?): support for ELF files.
// TODO (?): support for Mach-O files.
// TODO (?): support for COFF files.

namespace Orca::Plugin::LIEF {

Wizard::Wizard()
{
  setDisplayCategory(QLatin1String("LIEF"));
}

auto Wizard::create(QWidget *parent, const Core::WizardDialogParameters &params) const -> Core::BaseFileWizard*
{
  const auto format = QFileDialog::getOpenFileName(nullptr, "LIEF Executable File Formats", "", "(*.exe)");
  if (format.isEmpty())
    return {};

  binary = ::LIEF::Parser::parse(format.toStdString());
  if (!binary) {
    QMessageBox(QMessageBox::Warning, "Error", "LIEF Failed to parse the file format.").exec();
    return {};
  }

  m_wizard_dialog = new WizardDialog(this, parent);
  m_wizard_dialog->setPath(params.defaultPath().toString());

  for (const auto &page : m_wizard_dialog->extensionPages())
    m_wizard_dialog->addPage(page);

  return m_wizard_dialog;
}

} // namespace Orca::Plugin::LIEF
