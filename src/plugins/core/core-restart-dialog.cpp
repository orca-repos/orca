// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-restart-dialog.hpp"

#include "core-interface.hpp"

namespace Orca::Plugin::Core {

RestartDialog::RestartDialog(QWidget *parent, const QString &text) : QMessageBox(parent)
{
  setWindowTitle(tr("Restart Required"));
  setText(text);
  setIcon(Information);
  addButton(tr("Later"), NoRole);
  addButton(tr("Restart Now"), YesRole);

  connect(this, &QDialog::accepted, ICore::instance(), &ICore::restart, Qt::QueuedConnection);
}

} // namespace Orca::Plugin::Core
