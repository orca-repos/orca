// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include <core/basefilewizard.hpp>
#include <core/basefilewizardfactory.hpp>

#include <projectexplorer/customwizard/customwizard.hpp>

#include <utils/filewizardpage.hpp>

namespace LIEF {
namespace {

struct FileWizard : Core::BaseFileWizardFactory {
  FileWizard() {
    setDisplayCategory(QLatin1String("LIEF"));
    setIcon(QIcon{":/core/images/orcalogo-big.png"}); // TODO: Use appropriate icons to represent each file format.
  }

  auto create(QWidget *parent, const Core::WizardDialogParameters &params) const -> Core::BaseFileWizard* override;
  auto generateFiles(const QWizard *wizard, QString *error_message) const -> Core::GeneratedFiles override;
  auto postGenerateFiles(const QWizard *wizard, const Core::GeneratedFiles &files, QString *error_message) const -> bool override;
};

auto FileWizard::create(QWidget *parent, const Core::WizardDialogParameters &params) const -> Core::BaseFileWizard *
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

auto FileWizard::generateFiles(const QWizard *wizard, QString *error_message) const -> Core::GeneratedFiles
{
  return {};
}

auto FileWizard::postGenerateFiles(const QWizard *wizard, const Core::GeneratedFiles &files, QString *error_message) const -> bool
{
  return ProjectExplorer::CustomProjectWizard::postGenerateOpen(files, error_message);
}

struct PortableExecutable final : FileWizard {
  PortableExecutable() {
    setId("LIEF.NewFileWizard.PE");
    setDisplayName(tr("Portable Executable (PE)"));
    setDescription(tr("File format for executables, object code, DLLs and others used in 32-bit and 64-bit versions of Windows operating systems."));
  }
};

} // namespace

auto newFileWizardFactory() -> void
{
  Core::IWizardFactory::registerFactoryCreator([] {
    return QList<Core::IWizardFactory*>{new PortableExecutable};
  });
}

} // namespace LIEF
