// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-generated-file.hpp"
#include "core-global.hpp"

#include <utils/wizard.hpp>

namespace Orca::Plugin::Core {

class BaseFileWizardFactory;

class CORE_EXPORT BaseFileWizard : public Utils::Wizard {
  Q_OBJECT

public:
  explicit BaseFileWizard(const BaseFileWizardFactory *factory, QVariantMap extra_values, QWidget *parent = nullptr);

  auto initializePage(int id) -> void override;
  auto extensionPages() -> QList<QWizardPage*>;
  auto accept() -> void override;
  auto reject() -> void override;

private:
  auto generateFileList() -> void;

  QVariantMap m_extra_values;
  const BaseFileWizardFactory *m_factory;
  QList<QWizardPage*> m_extension_pages;
  QWizardPage *m_first_extension_page = nullptr;
  GeneratedFiles m_files;
};

} // namespace Orca::Plugin::Core
