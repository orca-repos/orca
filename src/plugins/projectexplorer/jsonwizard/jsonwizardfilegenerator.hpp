// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "jsonwizardgeneratorfactory.hpp"

#include <QVariant>

namespace ProjectExplorer {
namespace Internal {

class JsonWizardFileGenerator : public JsonWizardGenerator {
public:
  auto setup(const QVariant &data, QString *errorMessage) -> bool;
  auto fileList(Utils::MacroExpander *expander, const QString &wizardDir, const QString &projectDir, QString *errorMessage) -> Orca::Plugin::Core::GeneratedFiles override;
  auto writeFile(const JsonWizard *wizard, Orca::Plugin::Core::GeneratedFile *file, QString *errorMessage) -> bool override;

private:
  class File {
  public:
    bool keepExisting = false;
    QString source;
    QString target;
    QVariant condition = true;
    QVariant isBinary = false;
    QVariant overwrite = false;
    QVariant openInEditor = false;
    QVariant openAsProject = false;
    QVariant isTemporary = false;
    QList<JsonWizard::OptionDefinition> options;
  };

  auto generateFile(const File &file, Utils::MacroExpander *expander, QString *errorMessage) -> Orca::Plugin::Core::GeneratedFile;

  QList<File> m_fileList;
  friend auto operator<<(QDebug &debug, const File &file) -> QDebug&;
};

inline auto operator<<(QDebug &debug, const JsonWizardFileGenerator::File &file) -> QDebug&
{
  debug << "WizardFile{" << "source:" << file.source << "; target:" << file.target << "; condition:" << file.condition << "; options:" << file.options << "}";
  return debug;
}

} // namespace Internal
} // namespace ProjectExplorer
