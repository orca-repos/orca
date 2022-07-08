// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "jsonwizardscannergenerator.hpp"

#include "../projectexplorer.hpp"
#include "../projectmanager.hpp"
#include "jsonwizard.hpp"
#include "jsonwizardfactory.hpp"

#include <core/editormanager/editormanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/qtcassert.hpp>
#include <utils/macroexpander.hpp>
#include <utils/mimetypes/mimedatabase.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QVariant>

#include <limits>

namespace ProjectExplorer {
namespace Internal {

auto JsonWizardScannerGenerator::setup(const QVariant &data, QString *errorMessage) -> bool
{
  if (data.isNull())
    return true;

  if (data.type() != QVariant::Map) {
    *errorMessage = QCoreApplication::translate("ProjectExplorer::Internal::JsonWizard", "Key is not an object.");
    return false;
  }

  const auto gen = data.toMap();

  m_binaryPattern = gen.value(QLatin1String("binaryPattern")).toString();
  auto patterns = gen.value(QLatin1String("subdirectoryPatterns")).toStringList();
  foreach(const QString pattern, patterns) {
    QRegularExpression regexp(pattern);
    if (!regexp.isValid()) {
      *errorMessage = QCoreApplication::translate("ProjectExplorer::Internal::JsonWizard", "Pattern \"%1\" is no valid regular expression.");
      return false;
    }
    m_subDirectoryExpressions << regexp;
  }

  return true;
}

auto JsonWizardScannerGenerator::fileList(Utils::MacroExpander *expander, const QString &wizardDir, const QString &projectDir, QString *errorMessage) -> Core::GeneratedFiles
{
  Q_UNUSED(wizardDir)
  errorMessage->clear();

  const QDir project(projectDir);
  Core::GeneratedFiles result;

  QRegularExpression binaryPattern;
  if (!m_binaryPattern.isEmpty()) {
    binaryPattern = QRegularExpression(expander->expand(m_binaryPattern));
    if (!binaryPattern.isValid()) {
      qWarning() << QCoreApplication::translate("ProjectExplorer::Internal::JsonWizard", "ScannerGenerator: Binary pattern \"%1\" not valid.").arg(m_binaryPattern);
      return result;
    }
  }

  result = scan(project.absolutePath(), project);

  static const auto getDepth = [](const QString &filePath) { return int(filePath.count('/')); };
  auto minDepth = std::numeric_limits<int>::max();
  for (auto it = result.begin(); it != result.end(); ++it) {
    const QString relPath = project.relativeFilePath(it->path());
    it->setBinary(binaryPattern.match(relPath).hasMatch());
    const bool found = ProjectManager::canOpenProjectForMimeType(Utils::mimeTypeForFile(relPath));
    if (found) {
      it->setAttributes(it->attributes() | Core::GeneratedFile::OpenProjectAttribute);
      minDepth = std::min(minDepth, getDepth(it->path()));
    }
  }

  // Project files that appear on a lower level in the file system hierarchy than
  // other project files are not candidates for opening.
  for (Core::GeneratedFile &f : result) {
    if (f.attributes().testFlag(Core::GeneratedFile::OpenProjectAttribute) && getDepth(f.path()) > minDepth) {
      f.setAttributes(f.attributes().setFlag(Core::GeneratedFile::OpenProjectAttribute, false));
    }
  }

  return result;
}

auto JsonWizardScannerGenerator::matchesSubdirectoryPattern(const QString &path) -> bool
{
  foreach(const QRegularExpression &regexp, m_subDirectoryExpressions) {
    if (regexp.match(path).hasMatch())
      return true;
  }
  return false;
}

auto JsonWizardScannerGenerator::scan(const QString &dir, const QDir &base) -> Core::GeneratedFiles
{
  Core::GeneratedFiles result;
  const QDir directory(dir);

  if (!directory.exists())
    return result;

  auto entries = directory.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsLast | QDir::Name);
  foreach(const QFileInfo &fi, entries) {
    const auto relativePath = base.relativeFilePath(fi.absoluteFilePath());
    if (fi.isDir() && matchesSubdirectoryPattern(relativePath)) {
      result += scan(fi.absoluteFilePath(), base);
    } else {
      Core::GeneratedFile f(fi.absoluteFilePath());
      f.setAttributes(f.attributes() | Core::GeneratedFile::KeepExistingFileAttribute);

      result.append(f);
    }
  }

  return result;
}

} // namespace Internal
} // namespace ProjectExplorer
