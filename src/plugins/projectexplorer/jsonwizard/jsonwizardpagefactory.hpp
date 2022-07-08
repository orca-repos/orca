// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include <utils/id.hpp>

#include <QVariant>
#include <QStringList>

namespace Utils {
class WizardPage;
}

namespace ProjectExplorer {

class JsonWizard;

class PROJECTEXPLORER_EXPORT JsonWizardPageFactory {
public:
  virtual ~JsonWizardPageFactory();

  auto canCreate(Utils::Id typeId) const -> bool { return m_typeIds.contains(typeId); }
  auto supportedIds() const -> const QList<Utils::Id>& { return m_typeIds; }
  virtual auto create(JsonWizard *wizard, Utils::Id typeId, const QVariant &data) -> Utils::WizardPage* = 0;
  // Basic syntax check for the data taken from the wizard.json file:
  virtual auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool = 0;

protected:
  // This will add "PE.Wizard.Page." in front of the suffixes and set those as supported typeIds
  auto setTypeIdsSuffixes(const QStringList &suffixes) -> void;
  auto setTypeIdsSuffix(const QString &suffix) -> void;

private:
  QList<Utils::Id> m_typeIds;
};

} // namespace ProjectExplorer
