// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QMap>
#include <QStringList>

namespace Orca::Plugin::Core {
class GeneratedFile;
}

namespace ProjectExplorer {
namespace Internal {

class GeneratorScriptArgument;

// Parse the script arguments apart and expand the binary.
auto fixGeneratorScript(const QString &configFile, QString attributeIn) -> QStringList;

// Step 1) Do a dry run of the generation script to get a list of files on stdout
auto dryRunCustomWizardGeneratorScript(const QString &targetPath, const QStringList &script, const QList<GeneratorScriptArgument> &arguments, const QMap<QString, QString> &fieldMap, QString *errorMessage) -> QList<Orca::Plugin::Core::GeneratedFile>;

// Step 2) Generate files
auto runCustomWizardGeneratorScript(const QString &targetPath, const QStringList &script, const QList<GeneratorScriptArgument> &arguments, const QMap<QString, QString> &fieldMap, QString *errorMessage) -> bool;

} // namespace Internal
} // namespace ProjectExplorer
