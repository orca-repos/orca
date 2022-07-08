// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "simplecodestylepreferences.hpp"

#include <QVariant>

namespace TextEditor {

SimpleCodeStylePreferences::SimpleCodeStylePreferences(QObject *parent) : ICodeStylePreferences(parent)
{
  setSettingsSuffix("TabPreferences");
}

auto SimpleCodeStylePreferences::value() const -> QVariant
{
  return QVariant();
}

auto SimpleCodeStylePreferences::setValue(const QVariant &value) -> void
{
  Q_UNUSED(value)
}

} // TextEditor
