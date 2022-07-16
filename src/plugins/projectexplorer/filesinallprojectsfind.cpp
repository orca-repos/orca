// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filesinallprojectsfind.hpp"

#include "project.hpp"
#include "session.hpp"

#include <core/core-editor-manager.hpp>
#include <utils/algorithm.hpp>
#include <utils/filesearch.hpp>

#include <QSettings>

using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

auto FilesInAllProjectsFind::id() const -> QString
{
  return QLatin1String("Files in All Project Directories");
}

auto FilesInAllProjectsFind::displayName() const -> QString
{
  return tr("Files in All Project Directories");
}

constexpr char kSettingsKey[] = "FilesInAllProjectDirectories";

auto FilesInAllProjectsFind::writeSettings(QSettings *settings) -> void
{
  settings->beginGroup(kSettingsKey);
  writeCommonSettings(settings);
  settings->endGroup();
}

auto FilesInAllProjectsFind::readSettings(QSettings *settings) -> void
{
  settings->beginGroup(kSettingsKey);
  readCommonSettings(settings, "CMakeLists.txt,*.cmake,*.pro,*.pri,*.qbs,*.cpp,*.h,*.mm,*.qml,*.md,*.txt,*.qdoc", "*/.git/*,*/.cvs/*,*/.svn/*,*.autosave");
  settings->endGroup();
}

auto FilesInAllProjectsFind::files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> FileIterator*
{
  Q_UNUSED(additionalParameters)
  const auto dirs = Utils::transform<QSet>(SessionManager::projects(), [](Project *p) {
    return p->projectFilePath().parentDir();
  });
  const auto dirStrings = Utils::transform<QStringList>(dirs, &FilePath::toString);
  return new SubDirFileIterator(dirStrings, nameFilters, exclusionFilters, Orca::Plugin::Core::EditorManager::defaultTextCodec());
}

auto FilesInAllProjectsFind::label() const -> QString
{
  return tr("Files in All Project Directories:");
}

} // namespace Internal
} // namespace ProjectExplorer
