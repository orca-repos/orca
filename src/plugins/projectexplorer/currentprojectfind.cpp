// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "currentprojectfind.hpp"

#include "project.hpp"
#include "projecttree.hpp"
#include "session.hpp"

#include <utils/qtcassert.hpp>
#include <utils/filesearch.hpp>

#include <QDebug>
#include <QSettings>

using namespace ProjectExplorer;
using namespace Internal;
using namespace TextEditor;

CurrentProjectFind::CurrentProjectFind()
{
  connect(ProjectTree::instance(), &ProjectTree::currentProjectChanged, this, &CurrentProjectFind::handleProjectChanged);
  connect(SessionManager::instance(), &SessionManager::projectDisplayNameChanged, this, [this](Project *p) {
    if (p == ProjectTree::currentProject()) emit displayNameChanged();
  });
}

auto CurrentProjectFind::id() const -> QString
{
  return QLatin1String("Current Project");
}

auto CurrentProjectFind::displayName() const -> QString
{
  const auto p = ProjectTree::currentProject();
  if (p)
    return tr("Project \"%1\"").arg(p->displayName());
  else
    return tr("Current Project");
}

auto CurrentProjectFind::isEnabled() const -> bool
{
  return ProjectTree::currentProject() != nullptr && BaseFileFind::isEnabled();
}

auto CurrentProjectFind::additionalParameters() const -> QVariant
{
  const auto project = ProjectTree::currentProject();
  if (project)
    return QVariant::fromValue(project->projectFilePath().toString());
  return QVariant();
}

auto CurrentProjectFind::files(const QStringList &nameFilters, const QStringList &exclusionFilters, const QVariant &additionalParameters) const -> Utils::FileIterator*
{
  QTC_ASSERT(additionalParameters.isValid(), return new Utils::FileListIterator(QStringList(), QList<QTextCodec *>()));
  const auto projectFile = additionalParameters.toString();
  for (auto project : SessionManager::projects()) {
    if (project && projectFile == project->projectFilePath().toString())
      return filesForProjects(nameFilters, exclusionFilters, {project});
  }
  return new Utils::FileListIterator(QStringList(), QList<QTextCodec*>());
}

auto CurrentProjectFind::label() const -> QString
{
  const auto p = ProjectTree::currentProject();
  QTC_ASSERT(p, return QString());
  return tr("Project \"%1\":").arg(p->displayName());
}

auto CurrentProjectFind::handleProjectChanged() -> void
{
  emit enabledChanged(isEnabled());
  emit displayNameChanged();
}

auto CurrentProjectFind::recheckEnabled() -> void
{
  const auto search = qobject_cast<Orca::Plugin::Core::SearchResult*>(sender());
  if (!search)
    return;
  const auto projectFile = getAdditionalParameters(search).toString();
  for (const auto project : SessionManager::projects()) {
    if (projectFile == project->projectFilePath().toString()) {
      search->setSearchAgainEnabled(true);
      return;
    }
  }
  search->setSearchAgainEnabled(false);
}

auto CurrentProjectFind::writeSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("CurrentProjectFind"));
  writeCommonSettings(settings);
  settings->endGroup();
}

auto CurrentProjectFind::readSettings(QSettings *settings) -> void
{
  settings->beginGroup(QLatin1String("CurrentProjectFind"));
  readCommonSettings(settings, "*", "");
  settings->endGroup();
}
