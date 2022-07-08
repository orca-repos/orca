// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "jsonwizardpagefactory.hpp"

namespace ProjectExplorer {
namespace Internal {

class FieldPageFactory : public JsonWizardPageFactory {
public:
  FieldPageFactory();

  auto create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage* override;
  auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool override;
};

class FilePageFactory : public JsonWizardPageFactory {
public:
  FilePageFactory();

  auto create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage* override;
  auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool override;
};

class KitsPageFactory : public JsonWizardPageFactory {
public:
  KitsPageFactory();

  auto create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage* override;
  auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool override;
};

class ProjectPageFactory : public JsonWizardPageFactory {
public:
  ProjectPageFactory();

  auto create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage* override;
  auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool override;
};

class SummaryPageFactory : public JsonWizardPageFactory {
public:
  SummaryPageFactory();

  auto create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage* override;
  auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool override;
};

} // namespace Internal
} // namespace ProjectExplorer
