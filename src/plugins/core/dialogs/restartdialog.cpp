// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "restartdialog.h"

#include <core/icore.h>

namespace Core {

RestartDialog::RestartDialog(QWidget *parent, const QString &text) : QMessageBox(parent)
{
  setWindowTitle(tr("Restart Required"));
  setText(text);
  setIcon(Information);
  addButton(tr("Later"), NoRole);
  addButton(tr("Restart Now"), YesRole);

  connect(this, &QDialog::accepted, ICore::instance(), &ICore::restart, Qt::QueuedConnection);
}

} // namespace Core
