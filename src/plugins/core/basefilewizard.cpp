// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "basefilewizard.h"
#include "basefilewizardfactory.h"
#include "ifilewizardextension.h"

#include <QMessageBox>

using namespace Utils;

namespace Core {

static QList<IFileWizardExtension*> g_file_wizard_extensions;

IFileWizardExtension::IFileWizardExtension()
{
  g_file_wizard_extensions.append(this);
}

IFileWizardExtension::~IFileWizardExtension()
{
  g_file_wizard_extensions.removeOne(this);
}

BaseFileWizard::BaseFileWizard(const BaseFileWizardFactory *factory, QVariantMap extra_values, QWidget *parent) : Wizard(parent), m_extra_values(std::move(extra_values)), m_factory(factory)
{
  for (const auto extension : qAsConst(g_file_wizard_extensions))
    m_extension_pages += extension->extensionPages(factory);

  if (!m_extension_pages.empty())
    m_first_extension_page = m_extension_pages.front();
}

auto BaseFileWizard::initializePage(const int id) -> void
{
  Wizard::initializePage(id);

  if (page(id) == m_first_extension_page) {
    generateFileList();
    for (const auto ex : qAsConst(g_file_wizard_extensions))
      ex->firstExtensionPageShown(m_files, m_extra_values);
  }
}

auto BaseFileWizard::extensionPages() -> QList<QWizardPage*>
{
  return m_extension_pages;
}

auto BaseFileWizard::accept() -> void
{
  if (m_files.isEmpty())
    generateFileList();

  QString error_message;

  // Compile result list and prompt for overwrite
  switch (BaseFileWizardFactory::promptOverwrite(&m_files, &error_message)) {
  case BaseFileWizardFactory::OverwriteCanceled:
    reject();
    return;
  case BaseFileWizardFactory::OverwriteError:
    QMessageBox::critical(nullptr, tr("Existing files"), error_message);
    reject();
    return;
  case BaseFileWizardFactory::OverwriteOk:
    break;
  }

  for (const auto ex : qAsConst(g_file_wizard_extensions)) {
    for (auto i = 0; i < m_files.count(); i++) {
      ex->applyCodeStyle(&m_files[i]);
    }
  }

  // Write
  if (!m_factory->writeFiles(m_files, &error_message)) {
    QMessageBox::critical(parentWidget(), tr("File Generation Failure"), error_message);
    reject();
    return;
  }

  auto remove_open_project_attribute = false;

  // Run the extensions
  for (const auto ex : qAsConst(g_file_wizard_extensions)) {
    bool remove;
    if (!ex->processFiles(m_files, &remove, &error_message)) {
      if (!error_message.isEmpty())
        QMessageBox::critical(parentWidget(), tr("File Generation Failure"), error_message);
      reject();
      return;
    }
    remove_open_project_attribute |= remove;
  }

  if (remove_open_project_attribute) {
    for (auto i = 0; i < m_files.count(); i++) {
      if (m_files[i].attributes() & GeneratedFile::OpenProjectAttribute)
        m_files[i].setAttributes(GeneratedFile::OpenEditorAttribute);
    }
  }

  // Post generation handler
  if (!m_factory->postGenerateFiles(this, m_files, &error_message))
    if (!error_message.isEmpty())
      QMessageBox::critical(nullptr, tr("File Generation Failure"), error_message);

  Wizard::accept();
}

auto BaseFileWizard::reject() -> void
{
  m_files.clear();
  Wizard::reject();
}

auto BaseFileWizard::generateFileList() -> void
{
  QString error_message;
  m_files = m_factory->generateFiles(this, &error_message);
  if (m_files.empty()) {
    QMessageBox::critical(parentWidget(), tr("File Generation Failure"), error_message);
    reject();
  }
}

} // namespace Core
