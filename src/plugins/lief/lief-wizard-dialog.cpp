// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lief-wizard-dialog.hpp"

namespace Orca::Plugin::LIEF {

WizardDialog::WizardDialog(const Core::BaseFileWizardFactory *factory, QWidget *parent) : BaseFileWizard(factory, QVariantMap(), parent)
{
  setWindowTitle(tr("LIEF New Project"));
  m_lief = new LIEF;
  m_lief->setTitle(tr("LIEF Details"));
  addPage(m_lief);
}

auto WizardDialog::setPath(const QString &path) const -> void
{
  m_lief->setPath(path);
}

} // namespace Orca::Plugin::LIEF
