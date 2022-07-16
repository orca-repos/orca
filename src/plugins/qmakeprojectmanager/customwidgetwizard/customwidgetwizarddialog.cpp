// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customwidgetwizarddialog.hpp"
#include "customwidgetwidgetswizardpage.hpp"
#include "customwidgetpluginwizardpage.hpp"
#include "pluginoptions.hpp"
#include <projectexplorer/projectexplorerconstants.hpp>

#include <qtsupport/qtkitinformation.hpp>
#include <qtsupport/qtsupportconstants.hpp>

namespace QmakeProjectManager {
namespace Internal {

enum {
  IntroPageId = 0
};

CustomWidgetWizardDialog::CustomWidgetWizardDialog(const Orca::Plugin::Core::BaseFileWizardFactory *factory, const QString &templateName, const QIcon &icon, QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters) : BaseQmakeProjectWizardDialog(factory, parent, parameters), m_widgetsPage(new CustomWidgetWidgetsWizardPage), m_pluginPage(new CustomWidgetPluginWizardPage)
{
  setWindowIcon(icon);
  setWindowTitle(templateName);

  setIntroDescription(tr("This wizard generates a Qt Designer Custom Widget " "or a Qt Designer Custom Widget Collection project."));

  if (!parameters.extraValues().contains(QLatin1String(ProjectExplorer::Constants::PROJECT_KIT_IDS)))
    addTargetSetupPage();
  addPage(m_widgetsPage);
  m_pluginPageId = addPage(m_pluginPage);

  addExtensionPages(extensionPages());
  connect(this, &QWizard::currentIdChanged, this, &CustomWidgetWizardDialog::slotCurrentIdChanged);
}

auto CustomWidgetWizardDialog::fileNamingParameters() const -> FileNamingParameters
{
  return m_widgetsPage->fileNamingParameters();
}

auto CustomWidgetWizardDialog::setFileNamingParameters(const FileNamingParameters &fnp) -> void
{
  m_widgetsPage->setFileNamingParameters(fnp);
  m_pluginPage->setFileNamingParameters(fnp);
}

auto CustomWidgetWizardDialog::slotCurrentIdChanged(int id) -> void
{
  if (id == m_pluginPageId)
    m_pluginPage->init(m_widgetsPage);
}

auto CustomWidgetWizardDialog::pluginOptions() const -> QSharedPointer<PluginOptions>
{
  auto rc = m_pluginPage->basicPluginOptions();
  rc->widgetOptions = m_widgetsPage->widgetOptions();
  return rc;
}

} // namespace Internal
} // namespace QmakeProjectManager
