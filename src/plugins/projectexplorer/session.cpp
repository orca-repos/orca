// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "session.hpp"

#include "buildconfiguration.hpp"
#include "deployconfiguration.hpp"
#include "editorconfiguration.hpp"
#include "kit.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "projectnodes.hpp"
#include "target.hpp"

#include <core/core-constants.hpp>
#include <core/core-editor-manager.hpp>
#include <core/core-folder-navigation-widget.hpp>
#include <core/core-interface.hpp>
#include <core/core-document-interface.hpp>
#include <core/core-mode-interface.hpp>
#include <core/core-mode-manager.hpp>
#include <core/core-progress-manager.hpp>

#include <texteditor/texteditor.hpp>

#include <utils/algorithm.hpp>
#include <utils/stylehelper.hpp>

#include <QDebug>
#include <QDir>
#include <QFileInfo>

#include <QMessageBox>
#include <QPushButton>

#ifdef WITH_TESTS
#include <QTemporaryFile>
#include <QTest>
#include <vector>
#endif

using namespace Orca::Plugin::Core;
using namespace Utils;
using namespace ProjectExplorer::Internal;

namespace ProjectExplorer {

constexpr char DEFAULT_SESSION[] = "default";

/*!
     \class ProjectExplorer::SessionManager

     \brief The SessionManager class manages sessions.

     TODO the interface of this class is not really great.
     The implementation suffers from that all the functions from the
     public interface just wrap around functions which do the actual work.
     This could be improved.
*/

class SessionManagerPrivate {
public:
  auto restoreValues(const PersistentSettingsReader &reader) -> void;
  auto restoreDependencies(const PersistentSettingsReader &reader) -> void;
  auto restoreStartupProject(const PersistentSettingsReader &reader) -> void;
  auto restoreEditors(const PersistentSettingsReader &reader) -> void;
  auto restoreProjects(const FilePaths &fileList) -> void;
  auto askUserAboutFailedProjects() -> void;
  auto sessionLoadingProgress() -> void;
  auto recursiveDependencyCheck(const QString &newDep, const QString &checkDep) const -> bool;
  auto dependencies(const QString &proName) const -> QStringList;
  auto dependenciesOrder() const -> QStringList;
  auto dependencies(const QString &proName, QStringList &result) const -> void;
  static auto windowTitleAddition(const FilePath &filePath) -> QString;
  static auto sessionTitle(const FilePath &filePath) -> QString;
  auto hasProjects() const -> bool { return !m_projects.isEmpty(); }

  QString m_sessionName = QLatin1String(DEFAULT_SESSION);
  bool m_virginSession = true;
  bool m_loadingSession = false;
  bool m_casadeSetActive = false;
  mutable QStringList m_sessions;
  mutable QHash<QString, QDateTime> m_sessionDateTimes;
  Project *m_startupProject = nullptr;
  QList<Project*> m_projects;
  FilePaths m_failedProjects;
  QMap<QString, QStringList> m_depMap;
  QMap<QString, QVariant> m_values;
  QFutureInterface<void> m_future;
  PersistentSettingsWriter *m_writer = nullptr;

private:
  static auto locationInProject(const FilePath &filePath) -> QString;
};

static SessionManager *m_instance = nullptr;
static SessionManagerPrivate *d = nullptr;

static auto projectFolderId(Project *pro) -> QString
{
  return pro->projectFilePath().toString();
}

constexpr int PROJECT_SORT_VALUE = 100;

SessionManager::SessionManager(QObject *parent) : QObject(parent)
{
  m_instance = this;
  d = new SessionManagerPrivate;

  connect(ModeManager::instance(), &ModeManager::currentModeChanged, this, &SessionManager::saveActiveMode);

  connect(EditorManager::instance(), &EditorManager::editorCreated, this, &SessionManager::configureEditor);
  connect(this, &SessionManager::projectAdded, EditorManager::instance(), &EditorManager::updateWindowTitles);
  connect(this, &SessionManager::projectRemoved, EditorManager::instance(), &EditorManager::updateWindowTitles);
  connect(this, &SessionManager::projectDisplayNameChanged, EditorManager::instance(), &EditorManager::updateWindowTitles);
  connect(EditorManager::instance(), &EditorManager::editorOpened, this, &SessionManager::markSessionFileDirty);
  connect(EditorManager::instance(), &EditorManager::editorsClosed, this, &SessionManager::markSessionFileDirty);

  EditorManager::setWindowTitleAdditionHandler(&SessionManagerPrivate::windowTitleAddition);
  EditorManager::setSessionTitleHandler(&SessionManagerPrivate::sessionTitle);
}

SessionManager::~SessionManager()
{
  EditorManager::setWindowTitleAdditionHandler({});
  EditorManager::setSessionTitleHandler({});
  emit m_instance->aboutToUnloadSession(d->m_sessionName);
  delete d->m_writer;
  delete d;
  d = nullptr;
}

auto SessionManager::instance() -> SessionManager*
{
  return m_instance;
}

auto SessionManager::isDefaultVirgin() -> bool
{
  return isDefaultSession(d->m_sessionName) && d->m_virginSession;
}

auto SessionManager::isDefaultSession(const QString &session) -> bool
{
  return session == QLatin1String(DEFAULT_SESSION);
}

auto SessionManager::saveActiveMode(Id mode) -> void
{
  if (mode != Orca::Plugin::Core::MODE_WELCOME)
    setValue(QLatin1String("ActiveMode"), mode.toString());
}

auto SessionManagerPrivate::recursiveDependencyCheck(const QString &newDep, const QString &checkDep) const -> bool
{
  if (newDep == checkDep)
    return false;

  foreach(const QString &dependency, m_depMap.value(checkDep)) {
    if (!recursiveDependencyCheck(newDep, dependency))
      return false;
  }

  return true;
}

/*
 * The dependency management exposes an interface based on projects, but
 * is internally purely string based. This is suboptimal. Probably it would be
 * nicer to map the filenames to projects on load and only map it back to
 * filenames when saving.
 */

auto SessionManager::dependencies(const Project *project) -> QList<Project*>
{
  const auto proName = project->projectFilePath().toString();
  const auto proDeps = d->m_depMap.value(proName);

  QList<Project*> projects;
  foreach(const QString &dep, proDeps) {
    const auto fn = FilePath::fromString(dep);
    const auto pro = findOrDefault(d->m_projects, [&fn](Project *p) { return p->projectFilePath() == fn; });
    if (pro)
      projects += pro;
  }

  return projects;
}

auto SessionManager::hasDependency(const Project *project, const Project *depProject) -> bool
{
  const auto proName = project->projectFilePath().toString();
  const auto depName = depProject->projectFilePath().toString();

  const auto proDeps = d->m_depMap.value(proName);
  return proDeps.contains(depName);
}

auto SessionManager::canAddDependency(const Project *project, const Project *depProject) -> bool
{
  const auto newDep = project->projectFilePath().toString();
  const auto checkDep = depProject->projectFilePath().toString();

  return d->recursiveDependencyCheck(newDep, checkDep);
}

auto SessionManager::addDependency(Project *project, Project *depProject) -> bool
{
  const auto proName = project->projectFilePath().toString();
  const auto depName = depProject->projectFilePath().toString();

  // check if this dependency is valid
  if (!d->recursiveDependencyCheck(proName, depName))
    return false;

  auto proDeps = d->m_depMap.value(proName);
  if (!proDeps.contains(depName)) {
    proDeps.append(depName);
    d->m_depMap[proName] = proDeps;
  }
  emit m_instance->dependencyChanged(project, depProject);

  return true;
}

auto SessionManager::removeDependency(Project *project, Project *depProject) -> void
{
  const auto proName = project->projectFilePath().toString();
  const auto depName = depProject->projectFilePath().toString();

  auto proDeps = d->m_depMap.value(proName);
  proDeps.removeAll(depName);
  if (proDeps.isEmpty())
    d->m_depMap.remove(proName);
  else
    d->m_depMap[proName] = proDeps;
  emit m_instance->dependencyChanged(project, depProject);
}

auto SessionManager::isProjectConfigurationCascading() -> bool
{
  return d->m_casadeSetActive;
}

auto SessionManager::setProjectConfigurationCascading(bool b) -> void
{
  d->m_casadeSetActive = b;
  markSessionFileDirty();
}

auto SessionManager::setActiveTarget(Project *project, Target *target, SetActive cascade) -> void
{
  QTC_ASSERT(project, return);

  if (project->isShuttingDown())
    return;

  project->setActiveTarget(target);

  if (!target) // never cascade setting no target
    return;

  if (cascade != SetActive::Cascade || !d->m_casadeSetActive)
    return;

  auto kitId = target->kit()->id();
  for (const auto otherProject : projects()) {
    if (otherProject == project)
      continue;
    if (const auto otherTarget = findOrDefault(otherProject->targets(), [kitId](Target *t) { return t->kit()->id() == kitId; }))
      otherProject->setActiveTarget(otherTarget);
  }
}

auto SessionManager::setActiveBuildConfiguration(Target *target, BuildConfiguration *bc, SetActive cascade) -> void
{
  QTC_ASSERT(target, return);
  QTC_ASSERT(target->project(), return);

  if (target->project()->isShuttingDown() || target->isShuttingDown())
    return;

  target->setActiveBuildConfiguration(bc);

  if (!bc)
    return;
  if (cascade != SetActive::Cascade || !d->m_casadeSetActive)
    return;

  const auto kitId = target->kit()->id();
  const auto name = bc->displayName(); // We match on displayname
  for (const auto otherProject : projects()) {
    if (otherProject == target->project())
      continue;
    const auto otherTarget = otherProject->activeTarget();
    if (!otherTarget || otherTarget->kit()->id() != kitId)
      continue;

    for (const auto otherBc : otherTarget->buildConfigurations()) {
      if (otherBc->displayName() == name) {
        otherTarget->setActiveBuildConfiguration(otherBc);
        break;
      }
    }
  }
}

auto SessionManager::setActiveDeployConfiguration(Target *target, DeployConfiguration *dc, SetActive cascade) -> void
{
  QTC_ASSERT(target, return);
  QTC_ASSERT(target->project(), return);

  if (target->project()->isShuttingDown() || target->isShuttingDown())
    return;

  target->setActiveDeployConfiguration(dc);

  if (!dc)
    return;
  if (cascade != SetActive::Cascade || !d->m_casadeSetActive)
    return;

  const auto kitId = target->kit()->id();
  const auto name = dc->displayName(); // We match on displayname
  for (const auto otherProject : projects()) {
    if (otherProject == target->project())
      continue;
    const auto otherTarget = otherProject->activeTarget();
    if (!otherTarget || otherTarget->kit()->id() != kitId)
      continue;

    for (const auto otherDc : otherTarget->deployConfigurations()) {
      if (otherDc->displayName() == name) {
        otherTarget->setActiveDeployConfiguration(otherDc);
        break;
      }
    }
  }
}

auto SessionManager::setStartupProject(Project *startupProject) -> void
{
  QTC_ASSERT((!startupProject && d->m_projects.isEmpty()) || (startupProject && d->m_projects.contains(startupProject)), return);

  if (d->m_startupProject == startupProject)
    return;

  d->m_startupProject = startupProject;
  if (d->m_startupProject && d->m_startupProject->needsConfiguration()) {
    ModeManager::activateMode(Constants::MODE_SESSION);
    ModeManager::setFocusToCurrentMode();
  }
  FolderNavigationWidgetFactory::setFallbackSyncFilePath(startupProject ? startupProject->projectFilePath().parentDir() : FilePath());
  emit m_instance->startupProjectChanged(startupProject);
}

auto SessionManager::startupProject() -> Project*
{
  return d->m_startupProject;
}

auto SessionManager::startupTarget() -> Target*
{
  return d->m_startupProject ? d->m_startupProject->activeTarget() : nullptr;
}

auto SessionManager::startupBuildSystem() -> BuildSystem*
{
  const auto t = startupTarget();
  return t ? t->buildSystem() : nullptr;
}

/*!
 * Returns the RunConfiguration of the currently active target
 * of the startup project, if such exists, or \c nullptr otherwise.
 */

auto SessionManager::startupRunConfiguration() -> RunConfiguration*
{
  const auto t = startupTarget();
  return t ? t->activeRunConfiguration() : nullptr;
}

auto SessionManager::addProject(Project *pro) -> void
{
  QTC_ASSERT(pro, return);
  QTC_CHECK(!pro->displayName().isEmpty());
  QTC_CHECK(pro->id().isValid());

  d->m_virginSession = false;
  QTC_ASSERT(!d->m_projects.contains(pro), return);

  d->m_projects.append(pro);

  connect(pro, &Project::displayNameChanged, m_instance, [pro]() { emit m_instance->projectDisplayNameChanged(pro); });

  emit m_instance->projectAdded(pro);
  const auto updateFolderNavigation = [pro] {
    // destructing projects might trigger changes, so check if the project is actually there
    if (QTC_GUARD(d->m_projects.contains(pro))) {
      const auto icon = pro->rootProjectNode() ? pro->rootProjectNode()->icon() : QIcon();
      FolderNavigationWidgetFactory::insertRootDirectory({projectFolderId(pro), PROJECT_SORT_VALUE, pro->displayName(), pro->projectFilePath().parentDir(), icon});
    }
  };
  updateFolderNavigation();
  configureEditors(pro);
  connect(pro, &Project::fileListChanged, m_instance, [pro, updateFolderNavigation]() {
    configureEditors(pro);
    updateFolderNavigation(); // update icon
  });
  connect(pro, &Project::displayNameChanged, m_instance, updateFolderNavigation);

  if (!startupProject())
    setStartupProject(pro);
}

auto SessionManager::removeProject(Project *project) -> void
{
  d->m_virginSession = false;
  QTC_ASSERT(project, return);
  removeProjects({project});
}

auto SessionManager::loadingSession() -> bool
{
  return d->m_loadingSession;
}

auto SessionManager::save() -> bool
{
  emit m_instance->aboutToSaveSession();

  const auto filePath = sessionNameToFileName(d->m_sessionName);
  QVariantMap data;

  // See the explanation at loadSession() for how we handle the implicit default session.
  if (isDefaultVirgin()) {
    if (filePath.exists()) {
      PersistentSettingsReader reader;
      if (!reader.load(filePath)) {
        QMessageBox::warning(ICore::dialogParent(), tr("Error while saving session"), tr("Could not save session %1").arg(filePath.toUserOutput()));
        return false;
      }
      data = reader.restoreValues();
    }
  } else {
    // save the startup project
    if (d->m_startupProject) {
      data.insert(QLatin1String("StartupProject"), d->m_startupProject->projectFilePath().toString());
    }

    const auto c = StyleHelper::requestedBaseColor();
    if (c.isValid()) {
      auto tmp = QString::fromLatin1("#%1%2%3").arg(c.red(), 2, 16, QLatin1Char('0')).arg(c.green(), 2, 16, QLatin1Char('0')).arg(c.blue(), 2, 16, QLatin1Char('0'));
      data.insert(QLatin1String("Color"), tmp);
    }

    auto projectFiles = transform(projects(), &Project::projectFilePath);
    // Restore information on projects that failed to load:
    // don't read projects to the list, which the user loaded
    for (const auto &failed : qAsConst(d->m_failedProjects)) {
      if (!projectFiles.contains(failed))
        projectFiles << failed;
    }

    data.insert("ProjectList", Utils::transform<QStringList>(projectFiles, &FilePath::toString));
    data.insert("CascadeSetActive", d->m_casadeSetActive);

    QVariantMap depMap;
    auto i = d->m_depMap.constBegin();
    while (i != d->m_depMap.constEnd()) {
      auto key = i.key();
      QStringList values;
      foreach(const QString &value, i.value())
        values << value;
      depMap.insert(key, values);
      ++i;
    }
    data.insert(QLatin1String("ProjectDependencies"), QVariant(depMap));
    data.insert(QLatin1String("EditorSettings"), EditorManager::saveState().toBase64());
  }

  const auto end = d->m_values.constEnd();
  QStringList keys;
  for (auto it = d->m_values.constBegin(); it != end; ++it) {
    data.insert(QLatin1String("value-") + it.key(), it.value());
    keys << it.key();
  }
  data.insert(QLatin1String("valueKeys"), keys);

  if (!d->m_writer || d->m_writer->fileName() != filePath) {
    delete d->m_writer;
    d->m_writer = new PersistentSettingsWriter(filePath, "QtCreatorSession");
  }
  const auto result = d->m_writer->save(data, ICore::dialogParent());
  if (result) {
    if (!isDefaultVirgin())
      d->m_sessionDateTimes.insert(activeSession(), QDateTime::currentDateTime());
  } else {
    QMessageBox::warning(ICore::dialogParent(), tr("Error while saving session"), tr("Could not save session to file %1").arg(d->m_writer->fileName().toUserOutput()));
  }

  return result;
}

/*!
  Closes all projects
  */
auto SessionManager::closeAllProjects() -> void
{
  removeProjects(projects());
}

auto SessionManager::projects() -> const QList<Project*>
{
  return d->m_projects;
}

auto SessionManager::hasProjects() -> bool
{
  return d->hasProjects();
}

auto SessionManager::hasProject(Project *p) -> bool
{
  return d->m_projects.contains(p);
}

auto SessionManagerPrivate::dependencies(const QString &proName) const -> QStringList
{
  QStringList result;
  dependencies(proName, result);
  return result;
}

auto SessionManagerPrivate::dependencies(const QString &proName, QStringList &result) const -> void
{
  auto depends = m_depMap.value(proName);

  foreach(const QString &dep, depends)
    dependencies(dep, result);

  if (!result.contains(proName))
    result.append(proName);
}

auto SessionManagerPrivate::sessionTitle(const FilePath &filePath) -> QString
{
  if (SessionManager::isDefaultSession(d->m_sessionName)) {
    if (filePath.isEmpty()) {
      // use single project's name if there is only one loaded.
      const auto projects = SessionManager::projects();
      if (projects.size() == 1)
        return projects.first()->displayName();
    }
  } else {
    auto sessionName = d->m_sessionName;
    if (sessionName.isEmpty())
      sessionName = SessionManager::tr("Untitled");
    return sessionName;
  }
  return QString();
}

auto SessionManagerPrivate::locationInProject(const FilePath &filePath) -> QString
{
  const Project *project = SessionManager::projectForFile(filePath);
  if (!project)
    return QString();

  const auto parentDir = filePath.parentDir();

  if (parentDir == project->projectDirectory())
    return "@ " + project->displayName();

  if (filePath.isChildOf(project->projectDirectory())) {
    const auto dirInProject = parentDir.relativeChildPath(project->projectDirectory());
    return "(" + dirInProject.toUserOutput() + " @ " + project->displayName() + ")";
  }

  // For a file that is "outside" the project it belongs to, we display its
  // dir's full path because it is easier to read than a series of  "../../.".
  // Example: /home/hugo/GenericProject/App.files lists /home/hugo/lib/Bar.cpp
  return "(" + parentDir.toUserOutput() + " @ " + project->displayName() + ")";
}

auto SessionManagerPrivate::windowTitleAddition(const FilePath &filePath) -> QString
{
  return locationInProject(filePath);
}

auto SessionManagerPrivate::dependenciesOrder() const -> QStringList
{
  QList<QPair<QString, QStringList>> unordered;
  QStringList ordered;

  // copy the map to a temporary list
  for (const Project *pro : m_projects) {
    const auto proName = pro->projectFilePath().toString();
    const auto depList = filtered(m_depMap.value(proName), [this](const QString &proPath) {
      return contains(m_projects, [proPath](const Project *p) {
        return p->projectFilePath().toString() == proPath;
      });
    });
    unordered << qMakePair(proName, depList);
  }

  while (!unordered.isEmpty()) {
    for (int i = (unordered.count() - 1); i >= 0; --i) {
      if (unordered.at(i).second.isEmpty()) {
        ordered << unordered.at(i).first;
        unordered.removeAt(i);
      }
    }

    // remove the handled projects from the dependency lists
    // of the remaining unordered projects
    for (auto i = 0; i < unordered.count(); ++i) {
      foreach(const QString &pro, ordered) {
        auto depList = unordered.at(i).second;
        depList.removeAll(pro);
        unordered[i].second = depList;
      }
    }
  }

  return ordered;
}

auto SessionManager::projectOrder(const Project *project) -> QList<Project*>
{
  QList<Project*> result;

  QStringList pros;
  if (project)
    pros = d->dependencies(project->projectFilePath().toString());
  else
    pros = d->dependenciesOrder();

  foreach(const QString &proFile, pros) {
    for (const auto pro : projects()) {
      if (pro->projectFilePath().toString() == proFile) {
        result << pro;
        break;
      }
    }
  }

  return result;
}

auto SessionManager::projectForFile(const FilePath &fileName) -> Project*
{
  return findOrDefault(projects(), [&fileName](const Project *p) { return p->isKnownFile(fileName); });
}

auto SessionManager::projectWithProjectFilePath(const FilePath &filePath) -> Project*
{
  return findOrDefault(projects(), [&filePath](const Project *p) { return p->projectFilePath() == filePath; });
}

auto SessionManager::configureEditor(IEditor *editor, const QString &fileName) -> void
{
  if (const auto textEditor = qobject_cast<TextEditor::BaseTextEditor*>(editor)) {
    const auto project = projectForFile(FilePath::fromString(fileName));
    // Global settings are the default.
    if (project)
      project->editorConfiguration()->configureEditor(textEditor);
  }
}

auto SessionManager::configureEditors(Project *project) -> void
{
  foreach(IDocument *document, DocumentModel::openedDocuments()) {
    if (project->isKnownFile(document->filePath())) {
      foreach(IEditor *editor, DocumentModel::editorsForDocument(document)) {
        if (const auto textEditor = qobject_cast<TextEditor::BaseTextEditor*>(editor)) {
          project->editorConfiguration()->configureEditor(textEditor);
        }
      }
    }
  }
}

auto SessionManager::removeProjects(const QList<Project*> &remove) -> void
{
  for (const auto pro : remove) emit m_instance->aboutToRemoveProject(pro);

  auto changeStartupProject = false;

  // Delete projects
  for (auto pro : remove) {
    pro->saveSettings();
    pro->markAsShuttingDown();

    // Remove the project node:
    d->m_projects.removeOne(pro);

    if (pro == d->m_startupProject)
      changeStartupProject = true;

    FolderNavigationWidgetFactory::removeRootDirectory(projectFolderId(pro));
    disconnect(pro, nullptr, m_instance, nullptr);
    emit m_instance->projectRemoved(pro);
  }

  if (changeStartupProject)
    setStartupProject(hasProjects() ? projects().first() : nullptr);

  qDeleteAll(remove);
}

/*!
    Lets other plugins store persistent values within the session file.
*/

auto SessionManager::setValue(const QString &name, const QVariant &value) -> void
{
  if (d->m_values.value(name) == value)
    return;
  d->m_values.insert(name, value);
}

auto SessionManager::value(const QString &name) -> QVariant
{
  const auto it = d->m_values.constFind(name);
  return (it == d->m_values.constEnd()) ? QVariant() : *it;
}

auto SessionManager::activeSession() -> QString
{
  return d->m_sessionName;
}

auto SessionManager::sessions() -> QStringList
{
  if (d->m_sessions.isEmpty()) {
    // We are not initialized yet, so do that now
    const auto sessionFiles = ICore::userResourcePath().dirEntries({{"*qws"}}, QDir::Time | QDir::Reversed);
    for (const auto &file : sessionFiles) {
      const auto &name = file.completeBaseName();
      d->m_sessionDateTimes.insert(name, file.lastModified());
      if (name != QLatin1String(DEFAULT_SESSION))
        d->m_sessions << name;
    }
    d->m_sessions.prepend(QLatin1String(DEFAULT_SESSION));
  }
  return d->m_sessions;
}

auto SessionManager::sessionDateTime(const QString &session) -> QDateTime
{
  return d->m_sessionDateTimes.value(session);
}

auto SessionManager::sessionNameToFileName(const QString &session) -> FilePath
{
  return ICore::userResourcePath(session + ".qws");
}

/*!
    Creates \a session, but does not actually create the file.
*/

auto SessionManager::createSession(const QString &session) -> bool
{
  if (sessions().contains(session))
    return false;
  Q_ASSERT(d->m_sessions.size() > 0);
  d->m_sessions.insert(1, session);
  return true;
}

auto SessionManager::renameSession(const QString &original, const QString &newName) -> bool
{
  if (!cloneSession(original, newName))
    return false;
  if (original == activeSession())
    loadSession(newName);
  emit instance()->sessionRenamed(original, newName);
  return deleteSession(original);
}

/*!
    \brief Shows a dialog asking the user to confirm deleting the session \p session
*/
auto SessionManager::confirmSessionDelete(const QStringList &sessions) -> bool
{
  const auto title = sessions.size() == 1 ? tr("Delete Session") : tr("Delete Sessions");
  const auto question = sessions.size() == 1 ? tr("Delete session %1?").arg(sessions.first()) : tr("Delete these sessions?\n    %1").arg(sessions.join("\n    "));
  return QMessageBox::question(ICore::dialogParent(), title, question, QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}

/*!
     Deletes \a session name from session list and the file from disk.
*/
auto SessionManager::deleteSession(const QString &session) -> bool
{
  if (!d->m_sessions.contains(session))
    return false;
  d->m_sessions.removeOne(session);
  emit instance()->sessionRemoved(session);
  QFile fi(sessionNameToFileName(session).toString());
  if (fi.exists())
    return fi.remove();
  return false;
}

auto SessionManager::deleteSessions(const QStringList &sessions) -> void
{
  for (const auto &session : sessions)
    deleteSession(session);
}

auto SessionManager::cloneSession(const QString &original, const QString &clone) -> bool
{
  if (!d->m_sessions.contains(original))
    return false;

  QFile fi(sessionNameToFileName(original).toString());
  // If the file does not exist, we can still clone
  if (!fi.exists() || fi.copy(sessionNameToFileName(clone).toString())) {
    d->m_sessions.insert(1, clone);
    d->m_sessionDateTimes.insert(clone, sessionNameToFileName(clone).lastModified());
    return true;
  }
  return false;
}

auto SessionManagerPrivate::restoreValues(const PersistentSettingsReader &reader) -> void
{
  const auto keys = reader.restoreValue(QLatin1String("valueKeys")).toStringList();
  foreach(const QString &key, keys) {
    auto value = reader.restoreValue(QLatin1String("value-") + key);
    m_values.insert(key, value);
  }
}

auto SessionManagerPrivate::restoreDependencies(const PersistentSettingsReader &reader) -> void
{
  const auto depMap = reader.restoreValue(QLatin1String("ProjectDependencies")).toMap();
  auto i = depMap.constBegin();
  while (i != depMap.constEnd()) {
    const auto &key = i.key();
    QStringList values;
    foreach(const QString &value, i.value().toStringList())
      values << value;
    m_depMap.insert(key, values);
    ++i;
  }
}

auto SessionManagerPrivate::askUserAboutFailedProjects() -> void
{
  const auto failedProjects = m_failedProjects;
  if (!failedProjects.isEmpty()) {
    const auto fileList = FilePath::formatFilePaths(failedProjects, "<br>");
    QMessageBox box(QMessageBox::Warning, SessionManager::tr("Failed to restore project files"), SessionManager::tr("Could not restore the following project files:<br><b>%1</b>").arg(fileList));
    const auto keepButton = new QPushButton(SessionManager::tr("Keep projects in Session"), &box);
    const auto removeButton = new QPushButton(SessionManager::tr("Remove projects from Session"), &box);
    box.addButton(keepButton, QMessageBox::AcceptRole);
    box.addButton(removeButton, QMessageBox::DestructiveRole);

    box.exec();

    if (box.clickedButton() == removeButton)
      m_failedProjects.clear();
  }
}

auto SessionManagerPrivate::restoreStartupProject(const PersistentSettingsReader &reader) -> void
{
  const auto startupProject = reader.restoreValue(QLatin1String("StartupProject")).toString();
  if (!startupProject.isEmpty()) {
    for (const auto pro : qAsConst(m_projects)) {
      if (pro->projectFilePath().toString() == startupProject) {
        m_instance->setStartupProject(pro);
        break;
      }
    }
  }
  if (!m_startupProject) {
    if (!startupProject.isEmpty())
      qWarning() << "Could not find startup project" << startupProject;
    if (hasProjects())
      m_instance->setStartupProject(m_projects.first());
  }
}

auto SessionManagerPrivate::restoreEditors(const PersistentSettingsReader &reader) -> void
{
  const auto editorsettings = reader.restoreValue(QLatin1String("EditorSettings"));
  if (editorsettings.isValid()) {
    EditorManager::restoreState(QByteArray::fromBase64(editorsettings.toByteArray()));
    sessionLoadingProgress();
  }
}

/*!
     Loads a session, takes a session name (not filename).
*/
auto SessionManagerPrivate::restoreProjects(const FilePaths &fileList) -> void
{
  // indirectly adds projects to session
  // Keep projects that failed to load in the session!
  m_failedProjects = fileList;
  if (!fileList.isEmpty()) {
    const auto result = ProjectExplorerPlugin::openProjects(fileList);
    if (!result)
      ProjectExplorerPlugin::showOpenProjectError(result);
    foreach(Project *p, result.projects())
      m_failedProjects.removeAll(p->projectFilePath());
  }
}

/*
 * ========== Notes on storing and loading the default session ==========
 * The default session comes in two flavors: implicit and explicit. The implicit one,
 * also referred to as "default virgin" in the code base, is the one that is active
 * at start-up, if no session has been explicitly loaded due to command-line arguments
 * or the "restore last session" setting in the session manager.
 * The implicit default session silently turns into the explicit default session
 * by loading a project or a file or changing settings in the Dependencies panel. The explicit
 * default session can also be loaded by the user via the Welcome Screen.
 * This mechanism somewhat complicates the handling of session-specific settings such as
 * the ones in the task pane: Users expect that changes they make there become persistent, even
 * when they are in the implicit default session. However, we can't just blindly store
 * the implicit default session, because then we'd overwrite the project list of the explicit
 * default session. Therefore, we use the following logic:
 *     - Upon start-up, if no session is to be explicitly loaded, we restore the parts of the
 *       explicit default session that are not related to projects, editors etc; the
 *       "general settings" of the session, so to speak.
 *     - When storing the implicit default session, we overwrite only these "general settings"
 *       of the explicit default session and keep the others as they are.
 *     - When switching from the implicit to the explicit default session, we keep the
 *       "general settings" and load everything else from the session file.
 * This guarantees that user changes are properly transferred and nothing gets lost from
 * either the implicit or the explicit default session.
 *
 */
auto SessionManager::loadSession(const QString &session, bool initial) -> bool
{
  const auto loadImplicitDefault = session.isEmpty();
  const auto switchFromImplicitToExplicitDefault = session == DEFAULT_SESSION && d->m_sessionName == DEFAULT_SESSION && !initial;

  // Do nothing if we have that session already loaded,
  // exception if the session is the default virgin session
  // we still want to be able to load the default session
  if (session == d->m_sessionName && !isDefaultVirgin())
    return true;

  if (!loadImplicitDefault && !sessions().contains(session))
    return false;

  FilePaths fileList;
  // Try loading the file
  const auto fileName = sessionNameToFileName(loadImplicitDefault ? DEFAULT_SESSION : session);
  PersistentSettingsReader reader;
  if (fileName.exists()) {
    if (!reader.load(fileName)) {
      QMessageBox::warning(ICore::dialogParent(), tr("Error while restoring session"), tr("Could not restore session %1").arg(fileName.toUserOutput()));

      return false;
    }

    if (loadImplicitDefault) {
      d->restoreValues(reader);
      emit m_instance->sessionLoaded(DEFAULT_SESSION);
      return true;
    }

    fileList = transform(reader.restoreValue("ProjectList").toStringList(), &FilePath::fromString);
  } else if (loadImplicitDefault) {
    return true;
  }

  d->m_loadingSession = true;

  // Allow everyone to set something in the session and before saving
  emit m_instance->aboutToUnloadSession(d->m_sessionName);

  if (!save()) {
    d->m_loadingSession = false;
    return false;
  }

  // Clean up
  if (!EditorManager::closeAllEditors()) {
    d->m_loadingSession = false;
    return false;
  }

  // find a list of projects to close later
  const auto projectsToRemove = filtered(projects(), [&fileList](Project *p) {
    return !fileList.contains(p->projectFilePath());
  });
  const auto openProjects = projects();
  const auto projectPathsToLoad = filtered(fileList, [&openProjects](const FilePath &path) {
    return !contains(openProjects, [&path](Project *p) {
      return p->projectFilePath() == path;
    });
  });
  d->m_failedProjects.clear();
  d->m_depMap.clear();
  if (!switchFromImplicitToExplicitDefault)
    d->m_values.clear();
  d->m_casadeSetActive = false;

  d->m_sessionName = session;
  delete d->m_writer;
  d->m_writer = nullptr;
  EditorManager::updateWindowTitles();

  if (fileName.exists()) {
    d->m_virginSession = false;

    ProgressManager::addTask(d->m_future.future(), tr("Loading Session"), "ProjectExplorer.SessionFile.Load");

    d->m_future.setProgressRange(0, 1);
    d->m_future.setProgressValue(0);

    if (!switchFromImplicitToExplicitDefault)
      d->restoreValues(reader);
    emit m_instance->aboutToLoadSession(session);

    // retrieve all values before the following code could change them again
    auto modeId = Id::fromSetting(value(QLatin1String("ActiveMode")));
    if (!modeId.isValid())
      modeId = Id(Orca::Plugin::Core::MODE_EDIT);

    const auto c = QColor(reader.restoreValue(QLatin1String("Color")).toString());
    if (c.isValid())
      StyleHelper::setBaseColor(c);

    d->m_future.setProgressRange(0, projectPathsToLoad.count() + 1/*initialization above*/ + 1/*editors*/);
    d->m_future.setProgressValue(1);
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    d->restoreProjects(projectPathsToLoad);
    d->sessionLoadingProgress();
    d->restoreDependencies(reader);
    d->restoreStartupProject(reader);

    removeProjects(projectsToRemove); // only remove old projects now that the startup project is set!

    d->restoreEditors(reader);

    d->m_future.reportFinished();
    d->m_future = QFutureInterface<void>();

    // Fall back to Project mode if the startup project is unconfigured and
    // use the mode saved in the session otherwise
    if (d->m_startupProject && d->m_startupProject->needsConfiguration())
      modeId = Id(Constants::MODE_SESSION);

    ModeManager::activateMode(modeId);
    ModeManager::setFocusToCurrentMode();
  } else {
    removeProjects(projects());
    ModeManager::activateMode(Id(Orca::Plugin::Core::MODE_EDIT));
    ModeManager::setFocusToCurrentMode();
  }

  d->m_casadeSetActive = reader.restoreValue(QLatin1String("CascadeSetActive"), false).toBool();

  emit m_instance->sessionLoaded(session);

  // Starts a event loop, better do that at the very end
  d->askUserAboutFailedProjects();
  d->m_loadingSession = false;
  return true;
}

/*!
    Returns the last session that was opened by the user.
*/
auto SessionManager::lastSession() -> QString
{
  return ICore::settings()->value(Constants::LASTSESSION_KEY).toString();
}

/*!
    Returns the session that was active when Qt Creator was last closed, if any.
*/
auto SessionManager::startupSession() -> QString
{
  return ICore::settings()->value(Constants::STARTUPSESSION_KEY).toString();
}

auto SessionManager::reportProjectLoadingProgress() -> void
{
  d->sessionLoadingProgress();
}

auto SessionManager::markSessionFileDirty() -> void
{
  d->m_virginSession = false;
}

auto SessionManagerPrivate::sessionLoadingProgress() -> void
{
  m_future.setProgressValue(m_future.progressValue() + 1);
  QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

auto SessionManager::projectsForSessionName(const QString &session) -> QStringList
{
  const auto fileName = sessionNameToFileName(session);
  PersistentSettingsReader reader;
  if (fileName.exists()) {
    if (!reader.load(fileName)) {
      qWarning() << "Could not restore session" << fileName.toUserOutput();
      return QStringList();
    }
  }
  return reader.restoreValue(QLatin1String("ProjectList")).toStringList();
}

#ifdef WITH_TESTS

void ProjectExplorerPlugin::testSessionSwitch()
{
    QVERIFY(SessionManager::createSession("session1"));
    QVERIFY(SessionManager::createSession("session2"));
    QTemporaryFile cppFile("main.cpp");
    QVERIFY(cppFile.open());
    cppFile.close();
    QTemporaryFile projectFile1("XXXXXX.pro");
    QTemporaryFile projectFile2("XXXXXX.pro");
    struct SessionSpec {
        SessionSpec(const QString &n, QTemporaryFile &f) : name(n), projectFile(f) {}
        const QString name;
        QTemporaryFile &projectFile;
    };
    std::vector<SessionSpec> sessionSpecs{SessionSpec("session1", projectFile1),
                SessionSpec("session2", projectFile2)};
    for (const SessionSpec &sessionSpec : sessionSpecs) {
        static const QByteArray proFileContents
                = "TEMPLATE = app\n"
                  "CONFIG -= qt\n"
                  "SOURCES = " + cppFile.fileName().toLocal8Bit();
        QVERIFY(sessionSpec.projectFile.open());
        sessionSpec.projectFile.write(proFileContents);
        sessionSpec.projectFile.close();
        QVERIFY(SessionManager::loadSession(sessionSpec.name));
        const OpenProjectResult openResult
                = ProjectExplorerPlugin::openProject(
                    FilePath::fromString(sessionSpec.projectFile.fileName()));
        if (openResult.errorMessage().contains("text/plain"))
            QSKIP("This test requires the presence of QmakeProjectManager to be fully functional");
        QVERIFY(openResult);
        QCOMPARE(openResult.projects().count(), 1);
        QVERIFY(openResult.project());
        QCOMPARE(SessionManager::projects().count(), 1);
    }
    for (int i = 0; i < 30; ++i) {
        QVERIFY(SessionManager::loadSession("session1"));
        QCOMPARE(SessionManager::activeSession(), "session1");
        QCOMPARE(SessionManager::projects().count(), 1);
        QVERIFY(SessionManager::loadSession("session2"));
        QCOMPARE(SessionManager::activeSession(), "session2");
        QCOMPARE(SessionManager::projects().count(), 1);
    }
    QVERIFY(SessionManager::loadSession("session1"));
    SessionManager::closeAllProjects();
    QVERIFY(SessionManager::loadSession("session2"));
    SessionManager::closeAllProjects();
    QVERIFY(SessionManager::deleteSession("session1"));
    QVERIFY(SessionManager::deleteSession("session2"));
}

#endif // WITH_TESTS

} // namespace ProjectExplorer
