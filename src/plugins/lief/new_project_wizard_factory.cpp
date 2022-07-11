// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <core/basefilewizard.hpp>
#include <core/basefilewizardfactory.hpp>

#include <projectexplorer/customwizard/customwizard.hpp>

#include <utils/filewizardpage.hpp>

namespace LIEF {
namespace Constants {

constexpr char LIEFPROJECT_ID[] = "LIEF.Project";

} // namespace Constants

namespace {

struct ProjectWizard : Core::BaseFileWizardFactory {
  ProjectWizard() {
    setSupportedProjectTypes({Constants::LIEFPROJECT_ID});
    setDisplayCategory(QLatin1String("LIEF"));
    setIcon(QIcon{":/core/images/orcalogo-big.png"}); // TODO: Use appropriate icons to represent each Projects.
  }

  auto create(QWidget *parent, const Core::WizardDialogParameters &params) const -> Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *wizard, QString *error_message) const -> Core::GeneratedFiles override;
  auto postGenerateFiles(const QWizard *wizard, const Core::GeneratedFiles &files, QString *error_message) const -> bool override;
};

auto ProjectWizard::create(QWidget *parent, const Core::WizardDialogParameters &params) const -> Core::BaseFileWizard *
{
  const auto file_wizard = new Core::BaseFileWizard(this, params.extraValues(), parent);
  const auto file_wizard_page = new Utils::FileWizardPage;

  file_wizard->setWindowTitle(displayName());
  file_wizard_page->setPath(params.defaultPath().toString());
  file_wizard->addPage(file_wizard_page);

  for (const auto &page : file_wizard->extensionPages())
    file_wizard->addPage(page);

  return file_wizard;
}

auto ProjectWizard::generateFiles(const QWizard *wizard, QString *error_message) const -> Core::GeneratedFiles
{
  return {};
}

auto ProjectWizard::postGenerateFiles(const QWizard *wizard, const Core::GeneratedFiles &files, QString *error_message) const -> bool
{
  return ProjectExplorer::CustomProjectWizard::postGenerateOpen(files, error_message);
}

// Only Windows is supported at the moment.
// TODO: Refactor this to support other platforms.

struct Windows final : ProjectWizard {
  Windows() {
    setId("LIEF.NewFileWizard.Windows");
    setDisplayName(tr("Microsoft Windows"));
    setDescription(tr("Proprietary graphical operating system families developed and marketed by Microsoft."));
  }
};

} // namespace

auto newProjectWizardFactory() -> void
{
  Core::IWizardFactory::registerFactoryCreator([] {
    return QList<Core::IWizardFactory*>{new Windows};
  });
}

} // namespace LIEF
