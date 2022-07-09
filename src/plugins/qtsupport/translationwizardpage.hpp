// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <projectexplorer/jsonwizard/jsonwizardpagefactory.hpp>

namespace QtSupport {
namespace Internal {

class TranslationWizardPageFactory : public ProjectExplorer::JsonWizardPageFactory {
public:
  TranslationWizardPageFactory();

private:
  auto create(ProjectExplorer::JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage* override;
  auto validateData(Utils::Id, const QVariant &, QString *) -> bool override { return true; }
};

} // namespace Internal
} // namespace QtSupport
