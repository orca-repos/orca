// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textfieldcheckbox.h"

namespace Utils {

/*!
    \class Utils::TextFieldCheckBox
    \brief The TextFieldCheckBox class is a aheckbox that plays with
    \c QWizard::registerField.

    Provides a settable 'text' property containing predefined strings for
    \c true and \c false.
*/

TextFieldCheckBox::TextFieldCheckBox(const QString &text, QWidget *parent) : QCheckBox(text, parent), m_trueText(QLatin1String("true")), m_falseText(QLatin1String("false"))
{
  connect(this, &QCheckBox::stateChanged, this, &TextFieldCheckBox::slotStateChanged);
}

auto TextFieldCheckBox::text() const -> QString
{
  return isChecked() ? m_trueText : m_falseText;
}

auto TextFieldCheckBox::setText(const QString &s) -> void
{
  setChecked(s == m_trueText);
}

auto TextFieldCheckBox::slotStateChanged(int cs) -> void
{
  emit textChanged(cs == Qt::Checked ? m_trueText : m_falseText);
}

} // namespace Utils
