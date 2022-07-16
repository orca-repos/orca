// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "jsonwizardgeneratorfactory.hpp"

#include <QDir>
#include <QRegularExpression>
#include <QVariant>

namespace ProjectExplorer {
namespace Internal {

class JsonWizardScannerGenerator : public JsonWizardGenerator {
public:
  auto setup(const QVariant &data, QString *errorMessage) -> bool;
  auto fileList(Utils::MacroExpander *expander, const QString &wizardDir, const QString &projectDir, QString *errorMessage) -> Orca::Plugin::Core::GeneratedFiles override;

private:
  auto scan(const QString &dir, const QDir &base) -> Orca::Plugin::Core::GeneratedFiles;
  auto matchesSubdirectoryPattern(const QString &path) -> bool;

  QString m_binaryPattern;
  QList<QRegularExpression> m_subDirectoryExpressions;
};

} // namespace Internal
} // namespace ProjectExplorer
