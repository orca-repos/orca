// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <QCoreApplication>
#include <QMessageBox>

namespace Orca::Plugin::Core {

class CORE_EXPORT RestartDialog final : public QMessageBox {
  Q_DECLARE_TR_FUNCTIONS(Core::RestartDialog)

public:
  RestartDialog(QWidget *parent, const QString &text);
};

} // namespace Orca::Plugin::Core
