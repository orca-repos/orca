// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "newdialog.h"

#include <utils/qtcassert.h>

using namespace Core;

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
