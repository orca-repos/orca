// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "wizardpage.h"

#include "wizard.h"

/*! \class Utils::WizardPage

  \brief QWizardPage with a couple of improvements.

  Adds a way to register fields so that a Utils::Wizard can check
  whether those fields are actually defined and a new method
  that is called once the page was added to the wizard.
*/

namespace Utils {

WizardPage::WizardPage(QWidget *parent) : QWizardPage(parent) { }

auto WizardPage::pageWasAdded() -> void
{
  auto wiz = qobject_cast<Wizard*>(wizard());
  if (!wiz)
    return;

  for (auto i = m_toRegister.constBegin(); i != m_toRegister.constEnd(); ++i)
    wiz->registerFieldName(*i);

  m_toRegister.clear();
}

auto WizardPage::registerFieldWithName(const QString &name, QWidget *widget, const char *property, const char *changedSignal) -> void
{
  registerFieldName(name);
  registerField(name, widget, property, changedSignal);
}

auto WizardPage::registerFieldName(const QString &name) -> void
{
  auto wiz = qobject_cast<Wizard*>(wizard());
  if (wiz)
    wiz->registerFieldName(name);
  else
    m_toRegister.insert(name);
}

auto WizardPage::handleReject() -> bool
{
  return false;
}

auto WizardPage::handleAccept() -> bool
{
  return false;
}

} // namespace Utils
