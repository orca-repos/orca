// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonwizardgeneratorfactory.hpp"

#include "jsonwizard.hpp"
#include "jsonwizardfilegenerator.hpp"
#include "jsonwizardscannergenerator.hpp"

#include "../editorconfiguration.hpp"
#include "../project.hpp"
#include "../projectexplorerconstants.hpp"

#include <core/core-prompt-overwrite-dialog.hpp>
#include <texteditor/icodestylepreferences.hpp>
#include <texteditor/icodestylepreferencesfactory.hpp>
#include <texteditor/storagesettings.hpp>
#include <texteditor/tabsettings.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/textindenter.hpp>

#include <utils/algorithm.hpp>
#include <utils/mimetypes/mimedatabase.hpp>
#include <utils/stringutils.hpp>
#include <utils/qtcassert.hpp>

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTextCursor>
#include <QTextDocument>

using namespace Orca::Plugin::Core;
using namespace TextEditor;
using namespace Utils;

namespace ProjectExplorer {

// --------------------------------------------------------------------
// Helpers:
// --------------------------------------------------------------------

static auto codeStylePreferences(Project *project, Id languageId) -> ICodeStylePreferences*
{
  if (!languageId.isValid())
    return nullptr;

  if (project)
    return project->editorConfiguration()->codeStyle(languageId);

  return TextEditorSettings::codeStyle(languageId);
}

// --------------------------------------------------------------------
// JsonWizardGenerator:
// --------------------------------------------------------------------

auto JsonWizardGenerator::formatFile(const JsonWizard *wizard, GeneratedFile *file, QString *errorMessage) -> bool
{
  Q_UNUSED(errorMessage)

  if (file->isBinary() || file->contents().isEmpty())
    return true; // nothing to do

  const auto languageId = TextEditorSettings::languageId(mimeTypeForFile(file->path()).name());

  if (!languageId.isValid())
    return true; // don't modify files like *.ui, *.pro

  const auto baseProject = qobject_cast<Project*>(wizard->property("SelectedProject").value<QObject*>());
  const auto factory = TextEditorSettings::codeStyleFactory(languageId);

  QTextDocument doc(file->contents());
  QTextCursor cursor(&doc);
  Indenter *indenter = nullptr;
  if (factory) {
    indenter = factory->createIndenter(&doc);
    indenter->setFileName(FilePath::fromString(file->path()));
  }
  if (!indenter)
    indenter = new TextIndenter(&doc);
  const auto codeStylePrefs = codeStylePreferences(baseProject, languageId);
  indenter->setCodeStylePreferences(codeStylePrefs);

  cursor.select(QTextCursor::Document);
  indenter->indent(cursor, QChar::Null, codeStylePrefs->currentTabSettings());
  delete indenter;
  if (TextEditorSettings::storageSettings().m_cleanWhitespace) {
    auto block = doc.firstBlock();
    while (block.isValid()) {
      TabSettings::removeTrailingWhitespace(cursor, block);
      block = block.next();
    }
  }
  file->setContents(doc.toPlainText());

  return true;
}

auto JsonWizardGenerator::writeFile(const JsonWizard *wizard, GeneratedFile *file, QString *errorMessage) -> bool
{
  Q_UNUSED(wizard)
  Q_UNUSED(file)
  Q_UNUSED(errorMessage)
  return true;
}

auto JsonWizardGenerator::postWrite(const JsonWizard *wizard, GeneratedFile *file, QString *errorMessage) -> bool
{
  Q_UNUSED(wizard)
  Q_UNUSED(file)
  Q_UNUSED(errorMessage)
  return true;
}

auto JsonWizardGenerator::polish(const JsonWizard *wizard, GeneratedFile *file, QString *errorMessage) -> bool
{
  Q_UNUSED(wizard)
  Q_UNUSED(file)
  Q_UNUSED(errorMessage)
  return true;
}

auto JsonWizardGenerator::allDone(const JsonWizard *wizard, GeneratedFile *file, QString *errorMessage) -> bool
{
  Q_UNUSED(wizard)
  Q_UNUSED(file)
  Q_UNUSED(errorMessage)
  return true;
}

auto JsonWizardGenerator::promptForOverwrite(JsonWizard::GeneratorFiles *files, QString *errorMessage) -> OverwriteResult
{
  QStringList existingFiles;
  auto oddStuffFound = false;

  foreach(const JsonWizard::GeneratorFile &f, *files) {
    const QFileInfo fi(f.file.path());
    if (fi.exists() && !(f.file.attributes() & GeneratedFile::ForceOverwrite) && !(f.file.attributes() & GeneratedFile::KeepExistingFileAttribute))
      existingFiles.append(f.file.path());
  }
  if (existingFiles.isEmpty())
    return OverwriteOk;

  // Before prompting to overwrite existing files, loop over files and check
  // if there is anything blocking overwriting them (like them being links or folders).
  // Format a file list message as ( "<file1> [readonly], <file2> [folder]").
  const auto commonExistingPath = commonPath(existingFiles);
  QString fileNamesMsgPart;
  foreach(const QString &fileName, existingFiles) {
    const QFileInfo fi(fileName);
    if (fi.exists()) {
      if (!fileNamesMsgPart.isEmpty())
        fileNamesMsgPart += QLatin1String(", ");
      const auto namePart = QDir::toNativeSeparators(fileName.mid(commonExistingPath.size() + 1));
      if (fi.isDir()) {
        oddStuffFound = true;
        fileNamesMsgPart += QCoreApplication::translate("ProjectExplorer::JsonWizardGenerator", "%1 [folder]").arg(namePart);
      } else if (fi.isSymLink()) {
        oddStuffFound = true;
        fileNamesMsgPart += QCoreApplication::translate("ProjectExplorer::JsonWizardGenerator", "%1 [symbolic link]").arg(namePart);
      } else if (!fi.isWritable()) {
        oddStuffFound = true;
        fileNamesMsgPart += QCoreApplication::translate("ProjectExplorer::JsonWizardGenerator", "%1 [read only]").arg(namePart);
      }
    }
  }

  if (oddStuffFound) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizardGenerator", "The directory %1 contains files which cannot be overwritten:\n%2.").arg(QDir::toNativeSeparators(commonExistingPath)).arg(fileNamesMsgPart);
    return OverwriteError;
  }

  // Prompt to overwrite existing files.
  PromptOverwriteDialog overwriteDialog;

  // Scripts cannot handle overwrite
  overwriteDialog.setFiles(existingFiles);
  foreach(const JsonWizard::GeneratorFile &file, *files) if (!file.generator->canKeepExistingFiles())
    overwriteDialog.setFileEnabled(file.file.path(), false);
  if (overwriteDialog.exec() != QDialog::Accepted)
    return OverwriteCanceled;

  const auto existingFilesToKeep = toSet(overwriteDialog.uncheckedFiles());
  if (existingFilesToKeep.size() == files->size()) // All exist & all unchecked->Cancel.
    return OverwriteCanceled;

  // Set 'keep' attribute in files
  for (auto &file : *files) {
    if (!existingFilesToKeep.contains(file.file.path()))
      continue;

    file.file.setAttributes(file.file.attributes() | GeneratedFile::KeepExistingFileAttribute);
  }
  return OverwriteOk;
}

auto JsonWizardGenerator::formatFiles(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool
{
  for (auto i = files->begin(); i != files->end(); ++i) {
    if (!i->generator->formatFile(wizard, &(i->file), errorMessage))
      return false;
  }
  return true;
}

auto JsonWizardGenerator::writeFiles(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool
{
  for (auto i = files->begin(); i != files->end(); ++i) {
    if (!i->generator->writeFile(wizard, &(i->file), errorMessage))
      return false;
  }
  return true;
}

auto JsonWizardGenerator::postWrite(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool
{
  for (auto i = files->begin(); i != files->end(); ++i) {
    if (!i->generator->postWrite(wizard, &(i->file), errorMessage))
      return false;
  }
  return true;
}

auto JsonWizardGenerator::polish(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool
{
  for (auto i = files->begin(); i != files->end(); ++i) {
    if (!i->generator->polish(wizard, &(i->file), errorMessage))
      return false;
  }
  return true;
}

auto JsonWizardGenerator::allDone(const JsonWizard *wizard, JsonWizard::GeneratorFiles *files, QString *errorMessage) -> bool
{
  for (auto i = files->begin(); i != files->end(); ++i) {
    if (!i->generator->allDone(wizard, &(i->file), errorMessage))
      return false;
  }
  return true;
}

// --------------------------------------------------------------------
// JsonWizardGeneratorFactory:
// --------------------------------------------------------------------

auto JsonWizardGeneratorFactory::setTypeIdsSuffixes(const QStringList &suffixes) -> void
{
  m_typeIds = transform(suffixes, [](QString suffix) { return Id::fromString(QString::fromLatin1(Constants::GENERATOR_ID_PREFIX) + suffix); });
}

auto JsonWizardGeneratorFactory::setTypeIdsSuffix(const QString &suffix) -> void
{
  setTypeIdsSuffixes(QStringList() << suffix);
}

// --------------------------------------------------------------------
// FileGeneratorFactory:
// --------------------------------------------------------------------

namespace Internal {

FileGeneratorFactory::FileGeneratorFactory()
{
  setTypeIdsSuffix(QLatin1String("File"));
}

auto FileGeneratorFactory::create(Id typeId, const QVariant &data, const QString &path, Id platform, const QVariantMap &variables) -> JsonWizardGenerator*
{
  Q_UNUSED(path)
  Q_UNUSED(platform)
  Q_UNUSED(variables)

  QTC_ASSERT(canCreate(typeId), return nullptr);

  const auto gen = new JsonWizardFileGenerator;
  QString errorMessage;
  gen->setup(data, &errorMessage);

  if (!errorMessage.isEmpty()) {
    qWarning() << "FileGeneratorFactory setup error:" << errorMessage;
    delete gen;
    return nullptr;
  }

  return gen;
}

auto FileGeneratorFactory::validateData(Id typeId, const QVariant &data, QString *errorMessage) -> bool
{
  QTC_ASSERT(canCreate(typeId), return false);

  const QScopedPointer<JsonWizardFileGenerator> gen(new JsonWizardFileGenerator);
  return gen->setup(data, errorMessage);
}

// --------------------------------------------------------------------
// ScannerGeneratorFactory:
// --------------------------------------------------------------------

ScannerGeneratorFactory::ScannerGeneratorFactory()
{
  setTypeIdsSuffix(QLatin1String("Scanner"));
}

auto ScannerGeneratorFactory::create(Id typeId, const QVariant &data, const QString &path, Id platform, const QVariantMap &variables) -> JsonWizardGenerator*
{
  Q_UNUSED(path)
  Q_UNUSED(platform)
  Q_UNUSED(variables)

  QTC_ASSERT(canCreate(typeId), return nullptr);

  const auto gen = new JsonWizardScannerGenerator;
  QString errorMessage;
  gen->setup(data, &errorMessage);

  if (!errorMessage.isEmpty()) {
    qWarning() << "ScannerGeneratorFactory setup error:" << errorMessage;
    delete gen;
    return nullptr;
  }

  return gen;
}

auto ScannerGeneratorFactory::validateData(Id typeId, const QVariant &data, QString *errorMessage) -> bool
{
  QTC_ASSERT(canCreate(typeId), return false);

  const QScopedPointer<JsonWizardScannerGenerator> gen(new JsonWizardScannerGenerator);
  return gen->setup(data, errorMessage);
}

} // namespace Internal
} // namespace ProjectExplorer
