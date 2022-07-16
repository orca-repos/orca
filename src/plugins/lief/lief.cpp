// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "lief.hpp"

namespace Orca::Plugin::LIEF {

class LIEFPrivate : public Utils::WizardPage {
public:
  explicit LIEFPrivate() = default;
  Ui::LIEF m_ui{};
};

LIEF::LIEF(QWidget *parent) : WizardPage(parent), d(new LIEFPrivate)
{
  d->m_ui.setupUi(this);
}

LIEF::~LIEF()
{
  delete d;
}

auto LIEF::setPath(const QString &path) const -> void
{
  d->m_ui.pathChooser->setPath(path);
}

} // namespace Orca::Plugin::LIEF
