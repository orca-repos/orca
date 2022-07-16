// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_lief.h"

#include <utils/wizardpage.hpp>

namespace Orca::Plugin::LIEF {

namespace Ui {
class LIEF;
} // namespace Ui

class LIEFPrivate;
class LIEF : public Utils::WizardPage {
public:
  explicit LIEF(QWidget *parent = 0);
  ~LIEF() override;

  auto setPath(const QString &path) const -> void;

private:
  LIEFPrivate *d;
};

} // namespace Orca::Plugin::LIEF
