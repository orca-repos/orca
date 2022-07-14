// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-new-dialog.hpp"

#include <utils/qtcassert.hpp>

namespace Orca::Plugin::Core {

NewDialog::NewDialog()
{
  QTC_CHECK(m_current_dialog == nullptr);

  m_current_dialog = this;
}

NewDialog::~NewDialog()
{
  QTC_CHECK(m_current_dialog != nullptr);
  m_current_dialog = nullptr;
}

} // namespace Orca::Plugin::Core
