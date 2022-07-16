// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "customwidgetwizard.hpp"
#include "customwidgetwizarddialog.hpp"
#include "plugingenerator.hpp"
#include "filenamingparameters.hpp"
#include "pluginoptions.hpp"

#include <projectexplorer/projectexplorerconstants.hpp>

#include <qtsupport/qtsupportconstants.hpp>

#include <utils/filepath.hpp>

#include <QCoreApplication>

namespace QmakeProjectManager {
namespace Internal {

CustomWidgetWizard::CustomWidgetWizard()
{
  setId("P.Qt4CustomWidget");
  setCategory(QLatin1String(ProjectExplorer::Constants::QT_PROJECT_WIZARD_CATEGORY));
  setDisplayCategory(QCoreApplication::translate("ProjectExplorer", ProjectExplorer::Constants::QT_PROJECT_WIZARD_CATEGORY_DISPLAY));
  setDisplayName(tr("Qt Custom Designer Widget"));
  setDescription(tr("Creates a Qt Custom Designer Widget or a Custom Widget Collection."));
  setIcon(themedIcon(":/wizards/images/gui.png"));
  setRequiredFeatures({QtSupport::Constants::FEATURE_QWIDGETS});
}

auto CustomWidgetWizard::create(QWidget *parent, const Orca::Plugin::Core::WizardDialogParameters &parameters) const -> Orca::Plugin::Core::BaseFileWizard*
{
  auto rc = new CustomWidgetWizardDialog(this, displayName(), icon(), parent, parameters);
  rc->setProjectName(CustomWidgetWizardDialog::uniqueProjectName(parameters.defaultPath()));
  rc->setFileNamingParameters(FileNamingParameters(headerSuffix(), sourceSuffix(), QtWizard::lowerCaseFiles()));
  return rc;
}

auto CustomWidgetWizard::generateFiles(const QWizard *w, QString *errorMessage) const -> Orca::Plugin::Core::GeneratedFiles
{
  const auto *cw = qobject_cast<const CustomWidgetWizardDialog*>(w);
  Q_ASSERT(w);
  GenerationParameters p;
  p.fileName = cw->projectName();
  p.path = cw->filePath().toString();
  p.templatePath = QtWizard::templateDir();
  p.templatePath += QLatin1String("/customwidgetwizard");
  return PluginGenerator::generatePlugin(p, *(cw->pluginOptions()), errorMessage);
}

} // namespace Internal
} // namespace QmakeProjectManager
