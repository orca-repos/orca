// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "../projectexplorer_export.hpp"

#include "jsonwizard.hpp"

#include <utils/id.hpp>

#include <QList>
#include <QObject>

namespace Utils {
class MacroExpander;
}

namespace ProjectExplorer {

class JsonWizardGenerator {
public:
  virtual ~JsonWizardGenerator() = default;

  virtual auto fileList(Utils::MacroExpander *expander, const QString &baseDir, const QString &projectDir, QString *errorMessage) -> Orca::Plugin::Core::GeneratedFiles = 0;
  virtual auto formatFile(const JsonWizard *wizard, Orca::Plugin::Core::GeneratedFile *file, QString *errorMessage) -> bool;
  virtual auto writeFile(const JsonWizard *wizard, Orca::Plugin::Core::GeneratedFile *file, QString *errorMessage) -> bool;
  virtual auto postWrite(const JsonWizard *wizard, Orca::Plugin::Core::GeneratedFile *file, QString *errorMessage) -> bool;
  virtual auto polish(const JsonWizard *wizard, Orca::Plugin::Core::GeneratedFile *file, QString *errorMessage) -> bool;
  virtual auto allDone(const JsonWizard *wizard, Orca::Plugin::Core::GeneratedFile *file, QString *errorMessage) -> bool;
  virtual auto canKeepExistingFiles() const -> bool { return true; }

  enum OverwriteResult {
    OverwriteOk,
    OverwriteError,
    OverwriteCanceled
  };

  static auto promptForOverwrite(JsonWizard::GeneratorFiles *files, QString *errorMessage) -> OverwriteResult;
  static auto formatFiles(const JsonWizard *wizard, QList<JsonWizard::GeneratorFile> *files, QString *errorMessage) -> bool;
  static auto writeFiles(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool;
  static auto postWrite(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool;
  static auto polish(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool;
  static auto allDone(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool;
};

class JsonWizardGeneratorFactory : public QObject {
  Q_OBJECT

public:
  auto canCreate(Utils::Id typeId) const -> bool { return m_typeIds.contains(typeId); }
  auto supportedIds() const -> QList<Utils::Id> { return m_typeIds; }
  virtual auto create(Utils::Id typeId, const QVariant &data, const QString &path, Utils::Id platform, const QVariantMap &variables) -> JsonWizardGenerator* = 0;
  // Basic syntax check for the data taken from the wizard.json file:
  virtual auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool = 0;

protected:
  // This will add "PE.Wizard.Generator." in front of the suffixes and set those as supported typeIds
  auto setTypeIdsSuffixes(const QStringList &suffixes) -> void;
  auto setTypeIdsSuffix(const QString &suffix) -> void;

private:
  QList<Utils::Id> m_typeIds;
};

namespace Internal {

class FileGeneratorFactory : public JsonWizardGeneratorFactory {
  Q_OBJECT

public:
  FileGeneratorFactory();

  auto create(Utils::Id typeId, const QVariant &data, const QString &path, Utils::Id platform, const QVariantMap &variables) -> JsonWizardGenerator* override;
  auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool override;
};

class ScannerGeneratorFactory : public JsonWizardGeneratorFactory {
  Q_OBJECT

public:
  ScannerGeneratorFactory();

  auto create(Utils::Id typeId, const QVariant &data, const QString &path, Utils::Id platform, const QVariantMap &variables) -> JsonWizardGenerator* override;
  auto validateData(Utils::Id typeId, const QVariant &data, QString *errorMessage) -> bool override;
};

} // namespace Internal
} // namespace ProjectExplorer
