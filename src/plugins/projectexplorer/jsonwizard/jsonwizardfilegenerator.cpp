// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonwizardfilegenerator.hpp"

#include "../projectexplorer.hpp"
#include "jsonwizard.hpp"
#include "jsonwizardfactory.hpp"

#include <core/core-editor-manager.hpp>

#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/macroexpander.hpp>
#include <utils/templateengine.hpp>
#include <utils/algorithm.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QVariant>

namespace ProjectExplorer {
namespace Internal {

auto JsonWizardFileGenerator::setup(const QVariant &data, QString *errorMessage) -> bool
{
  QTC_ASSERT(errorMessage && errorMessage->isEmpty(), return false);

  auto list = JsonWizardFactory::objectOrList(data, errorMessage);
  if (list.isEmpty())
    return false;

  foreach(const QVariant &d, list) {
    if (d.type() != QVariant::Map) {
      *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Files data list entry is not an object.");
      return false;
    }

    File f;

    const auto tmp = d.toMap();
    f.source = tmp.value(QLatin1String("source")).toString();
    f.target = tmp.value(QLatin1String("target")).toString();
    f.condition = tmp.value(QLatin1String("condition"), true);
    f.isBinary = tmp.value(QLatin1String("isBinary"), false);
    f.overwrite = tmp.value(QLatin1String("overwrite"), false);
    f.openInEditor = tmp.value(QLatin1String("openInEditor"), false);
    f.isTemporary = tmp.value(QLatin1String("temporary"), false);
    f.openAsProject = tmp.value(QLatin1String("openAsProject"), false);

    f.options = JsonWizard::parseOptions(tmp.value(QLatin1String("options")), errorMessage);
    if (!errorMessage->isEmpty())
      return false;

    if (f.source.isEmpty() && f.target.isEmpty()) {
      *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonFieldPage", "Source and target are both empty.");
      return false;
    }

    if (f.target.isEmpty())
      f.target = f.source;

    m_fileList << f;
  }

  return true;
}

auto JsonWizardFileGenerator::generateFile(const File &file, Utils::MacroExpander *expander, QString *errorMessage) -> Orca::Plugin::Core::GeneratedFile
{
  // Read contents of source file
  const auto openMode = file.isBinary.toBool() ? QIODevice::ReadOnly : (QIODevice::ReadOnly | QIODevice::Text);

  Utils::FileReader reader;
  if (!reader.fetch(Utils::FilePath::fromString(file.source), openMode, errorMessage))
    return Orca::Plugin::Core::GeneratedFile();

  // Generate file information:
  Orca::Plugin::Core::GeneratedFile gf;
  gf.setPath(file.target);

  if (!file.keepExisting) {
    if (file.isBinary.toBool()) {
      gf.setBinary(true);
      gf.setBinaryContents(reader.data());
    } else {
      // TODO: Document that input files are UTF8 encoded!
      gf.setBinary(false);
      Utils::MacroExpander nested;

      // evaluate file options once:
      QHash<QString, QString> options;
      foreach(const JsonWizard::OptionDefinition &od, file.options) {
        if (od.condition(*expander))
          options.insert(od.key(), od.value(*expander));
      }

      nested.registerExtraResolver([&options](QString n, QString *ret) -> bool {
        if (!options.contains(n))
          return false;
        *ret = options.value(n);
        return true;
      });
      nested.registerExtraResolver([expander](QString n, QString *ret) {
        return expander->resolveMacro(n, ret);
      });

      gf.setContents(Utils::TemplateEngine::processText(&nested, QString::fromUtf8(reader.data()), errorMessage));
      if (!errorMessage->isEmpty()) {
        *errorMessage = QCoreApplication::translate("ProjectExplorer::JsonWizard", "When processing \"%1\":<br>%2").arg(file.source, *errorMessage);
        return Orca::Plugin::Core::GeneratedFile();
      }
    }
  }

  Orca::Plugin::Core::GeneratedFile::Attributes attributes;
  if (JsonWizard::boolFromVariant(file.openInEditor, expander))
    attributes |= Orca::Plugin::Core::GeneratedFile::OpenEditorAttribute;
  if (JsonWizard::boolFromVariant(file.openAsProject, expander))
    attributes |= Orca::Plugin::Core::GeneratedFile::OpenProjectAttribute;
  if (JsonWizard::boolFromVariant(file.overwrite, expander))
    attributes |= Orca::Plugin::Core::GeneratedFile::ForceOverwrite;
  if (JsonWizard::boolFromVariant(file.isTemporary, expander))
    attributes |= Orca::Plugin::Core::GeneratedFile::TemporaryFile;

  if (file.keepExisting)
    attributes |= Orca::Plugin::Core::GeneratedFile::KeepExistingFileAttribute;

  gf.setAttributes(attributes);
  return gf;
}

auto JsonWizardFileGenerator::fileList(Utils::MacroExpander *expander, const QString &wizardDir, const QString &projectDir, QString *errorMessage) -> Orca::Plugin::Core::GeneratedFiles
{
  errorMessage->clear();

  QDir wizard(wizardDir);
  QDir project(projectDir);

  const auto enabledFiles = Utils::filtered(m_fileList, [&expander](const File &f) {
    return JsonWizard::boolFromVariant(f.condition, expander);
  });

  const auto concreteFiles = Utils::transform(enabledFiles, [&expander, &wizard, &project](const File &f) -> File {
    // Return a new file with concrete values based on input file:
    auto file = f;

    file.keepExisting = file.source.isEmpty();
    file.target = project.absoluteFilePath(expander->expand(file.target));
    file.source = file.keepExisting ? file.target : wizard.absoluteFilePath(expander->expand(file.source));
    file.isBinary = JsonWizard::boolFromVariant(file.isBinary, expander);

    return file;
  });

  QList<File> fileList;
  QList<File> dirList;
  std::tie(fileList, dirList) = Utils::partition(concreteFiles, [](const File &f) { return !QFileInfo(f.source).isDir(); });

  const auto knownFiles = Utils::transform<QSet>(fileList, &File::target);

  foreach(const File &dir, dirList) {
    QDir sourceDir(dir.source);
    QDirIterator it(dir.source, QDir::NoDotAndDotDot | QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);

    while (it.hasNext()) {
      const auto relativeFilePath = sourceDir.relativeFilePath(it.next());
      const QString targetPath = dir.target + QLatin1Char('/') + relativeFilePath;

      if (knownFiles.contains(targetPath))
        continue;

      // initialize each new file with properties (isBinary etc)
      // from the current directory json entry
      auto newFile = dir;
      newFile.source = dir.source + QLatin1Char('/') + relativeFilePath;
      newFile.target = targetPath;
      fileList.append(newFile);
    }
  }

  const Orca::Plugin::Core::GeneratedFiles result = Utils::transform(fileList, [this, &expander, &errorMessage](const File &f) {
    return generateFile(f, expander, errorMessage);
  });

  if (Utils::contains(result, [](const Orca::Plugin::Core::GeneratedFile &gf) { return gf.path().isEmpty(); }))
    return Orca::Plugin::Core::GeneratedFiles();

  return result;
}

auto JsonWizardFileGenerator::writeFile(const JsonWizard *wizard, Orca::Plugin::Core::GeneratedFile *file, QString *errorMessage) -> bool
{
  Q_UNUSED(wizard)
  if (!(file->attributes() & Orca::Plugin::Core::GeneratedFile::KeepExistingFileAttribute)) {
    if (!file->write(errorMessage))
      return false;
  }
  return true;
}

} // namespace Internal
} // namespace ProjectExplorer
