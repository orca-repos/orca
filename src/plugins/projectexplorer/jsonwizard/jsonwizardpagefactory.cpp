// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonwizardpagefactory.hpp"

#include "../projectexplorerconstants.hpp"

#include <utils/algorithm.hpp>

namespace ProjectExplorer {

// --------------------------------------------------------------------
// JsonWizardPageFactory:
// --------------------------------------------------------------------

JsonWizardPageFactory::~JsonWizardPageFactory() = default;

auto JsonWizardPageFactory::setTypeIdsSuffixes(const QStringList &suffixes) -> void
{
  m_typeIds = Utils::transform(suffixes, [](const QString &suffix) {
    return Utils::Id::fromString(QString::fromLatin1(Constants::PAGE_ID_PREFIX) + suffix);
  });
}

auto JsonWizardPageFactory::setTypeIdsSuffix(const QString &suffix) -> void
{
  setTypeIdsSuffixes(QStringList() << suffix);
}

} // namespace ProjectExplorer
