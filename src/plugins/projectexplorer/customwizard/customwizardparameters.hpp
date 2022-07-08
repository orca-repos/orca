// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/iwizardfactory.hpp>

#include <utils/filepath.hpp>

#include <QStringList>
#include <QMap>
#include <QSharedPointer>

QT_BEGIN_NAMESPACE
class QIODevice;
class QDebug;
class QJSEngine;
QT_END_NAMESPACE

namespace Utils {
class TemporaryFile;
}

namespace ProjectExplorer {
namespace Internal {

class CustomWizardField {
public:
  // Parameters of the widget control are stored as map
  using ControlAttributeMap = QMap<QString, QString>;
  CustomWizardField();
  auto clear() -> void;

  // Attribute map keys for combo entries
  static auto comboEntryValueKey(int i) -> QString;
  static auto comboEntryTextKey(int i) -> QString;

  QString description;
  QString name;
  ControlAttributeMap controlAttributes;
  bool mandatory;
};

class CustomWizardFile {
public:
  CustomWizardFile();

  QString source;
  QString target;
  bool openEditor;
  bool openProject;
  bool binary;
};

// Documentation inside.
class CustomWizardValidationRule {
public:
  // Validate a set of rules and return false + message on the first failing one.
  static auto validateRules(const QList<CustomWizardValidationRule> &rules, const QMap<QString, QString> &replacementMap, QString *errorMessage) -> bool;
  auto validate(QJSEngine &, const QMap<QString, QString> &replacementMap) const -> bool;
  QString condition;
  QString message;
};

// Documentation inside.
class GeneratorScriptArgument {
public:
  enum Flags {
    // Omit this arguments if all field placeholders expanded to empty strings.
    OmitEmpty = 0x1,
    // Do use the actual field value, but write it to a temporary
    // text file and inserts its file name (suitable for multiline texts).
    WriteFile = 0x2
  };

  explicit GeneratorScriptArgument(const QString &value = QString());

  QString value;
  unsigned flags;
};

class CustomWizardParameters {
public:
  enum ParseResult {
    ParseOk,
    ParseDisabled,
    ParseFailed
  };

  CustomWizardParameters() = default;
  auto clear() -> void;
  auto parse(QIODevice &device, const QString &configFileFullPath, QString *errorMessage) -> ParseResult;
  auto parse(const QString &configFileFullPath, QString *errorMessage) -> ParseResult;

  Utils::Id id;
  QString directory;
  QString klass;
  QList<CustomWizardFile> files;
  QStringList filesGeneratorScript; // Complete binary, such as 'cmd /c myscript.pl'.
  QString filesGeneratorScriptWorkingDirectory;
  QList<GeneratorScriptArgument> filesGeneratorScriptArguments;
  QString fieldPageTitle;
  QList<CustomWizardField> fields;
  QList<CustomWizardValidationRule> rules;
  int firstPageId = -1;

  // Wizard Factory data:
  Core::IWizardFactory::WizardKind kind = Core::IWizardFactory::FileWizard;
  QIcon icon;
  QString description;
  QString displayName;
  QString category;
  QString displayCategory;
  QSet<Utils::Id> requiredFeatures;
  Core::IWizardFactory::WizardFlags flags;
};

// Documentation inside.
class CustomWizardContext {
public:
  using FieldReplacementMap = QMap<QString, QString>;
  using TemporaryFilePtr = QSharedPointer<Utils::TemporaryFile>;
  using TemporaryFilePtrList = QList<TemporaryFilePtr>;

  auto reset() -> void;
  static auto replaceFields(const FieldReplacementMap &fm, QString *s) -> bool;
  static auto replaceFields(const FieldReplacementMap &fm, QString *s, TemporaryFilePtrList *files) -> bool;
  static auto processFile(const FieldReplacementMap &fm, QString in) -> QString;

  FieldReplacementMap baseReplacements;
  FieldReplacementMap replacements;

  Utils::FilePath path;
  // Where files should be created, that is, 'path' for simple wizards
  // or "path + project" for project wizards.
  Utils::FilePath targetPath;
};

extern const char customWizardFileOpenEditorAttributeC[];
extern const char customWizardFileOpenProjectAttributeC[];

} // namespace Internal
} // namespace ProjectExplorer
