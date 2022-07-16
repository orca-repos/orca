// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "sessionmodel.hpp"
#include "session.hpp"

#include "sessiondialog.hpp"

#include <core/core-action-manager.hpp>

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/stringutils.hpp>

#include <QFileInfo>
#include <QDir>

using namespace Orca::Plugin::Core;
using namespace Utils;

namespace ProjectExplorer {
namespace Internal {

SessionModel::SessionModel(QObject *parent) : QAbstractTableModel(parent)
{
  m_sortedSessions = SessionManager::sessions();
  connect(SessionManager::instance(), &SessionManager::sessionLoaded, this, &SessionModel::resetSessions);
}

auto SessionModel::indexOfSession(const QString &session) -> int
{
  return m_sortedSessions.indexOf(session);
}

auto SessionModel::sessionAt(int row) const -> QString
{
  return m_sortedSessions.value(row, QString());
}

auto SessionModel::headerData(int section, Qt::Orientation orientation, int role) const -> QVariant
{
  QVariant result;
  if (orientation == Qt::Horizontal) {
    switch (role) {
    case Qt::DisplayRole:
      switch (section) {
      case 0:
        result = tr("Session");
        break;
      case 1:
        result = tr("Last Modified");
        break;
      } // switch (section)
      break;
    } // switch (role) {
  }
  return result;
}

auto SessionModel::columnCount(const QModelIndex &) const -> int
{
  static auto sectionCount = 0;
  if (sectionCount == 0) {
    // headers sections defining possible columns
    while (!headerData(sectionCount, Qt::Horizontal, Qt::DisplayRole).isNull())
      sectionCount++;
  }

  return sectionCount;
}

auto SessionModel::rowCount(const QModelIndex &) const -> int
{
  return m_sortedSessions.count();
}

auto pathsToBaseNames(const QStringList &paths) -> QStringList
{
  return transform(paths, [](const QString &path) {
    return QFileInfo(path).completeBaseName();
  });
}

auto pathsWithTildeHomePath(const QStringList &paths) -> QStringList
{
  return transform(paths, [](const QString &path) {
    return withTildeHomePath(QDir::toNativeSeparators(path));
  });
}

auto SessionModel::data(const QModelIndex &index, int role) const -> QVariant
{
  QVariant result;
  if (index.isValid()) {
    const auto sessionName = m_sortedSessions.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
      switch (index.column()) {
      case 0:
        result = sessionName;
        break;
      case 1:
        result = SessionManager::sessionDateTime(sessionName);
        break;
      } // switch (section)
      break;
    case Qt::FontRole: {
      QFont font;
      if (SessionManager::isDefaultSession(sessionName))
        font.setItalic(true);
      else
        font.setItalic(false);
      if (SessionManager::activeSession() == sessionName && !SessionManager::isDefaultVirgin())
        font.setBold(true);
      else
        font.setBold(false);
      result = font;
    }
    break;
    case DefaultSessionRole:
      result = SessionManager::isDefaultSession(sessionName);
      break;
    case LastSessionRole:
      result = SessionManager::lastSession() == sessionName;
      break;
    case ActiveSessionRole:
      result = SessionManager::activeSession() == sessionName;
      break;
    case ProjectsPathRole:
      result = pathsWithTildeHomePath(SessionManager::projectsForSessionName(sessionName));
      break;
    case ProjectsDisplayRole:
      result = pathsToBaseNames(SessionManager::projectsForSessionName(sessionName));
      break;
    case ShortcutRole: {
      const Id sessionBase = SESSION_BASE_ID;
      if (const auto cmd = ActionManager::command(sessionBase.withSuffix(index.row() + 1)))
        result = cmd->keySequence().toString(QKeySequence::NativeText);
    }
    break;
    } // switch (role)
  }

  return result;
}

auto SessionModel::roleNames() const -> QHash<int, QByteArray>
{
  static const QHash<int, QByteArray> extraRoles{{Qt::DisplayRole, "sessionName"}, {DefaultSessionRole, "defaultSession"}, {ActiveSessionRole, "activeSession"}, {LastSessionRole, "lastSession"}, {ProjectsPathRole, "projectsPath"}, {ProjectsDisplayRole, "projectsName"}};
  auto roles = QAbstractTableModel::roleNames();
  addToHash(&roles, extraRoles);
  return roles;
}

auto SessionModel::sort(int column, Qt::SortOrder order) -> void
{
  beginResetModel();
  const auto cmp = [column, order](const QString &s1, const QString &s2) {
    bool isLess;
    if (column == 0) {
      if (s1 == s2)
        return false;
      isLess = s1 < s2;
    } else {
      const auto s1time = SessionManager::sessionDateTime(s1);
      const auto s2time = SessionManager::sessionDateTime(s2);
      if (s1time == s2time)
        return false;
      isLess = s1time < s2time;
    }
    if (order == Qt::DescendingOrder)
      isLess = !isLess;
    return isLess;
  };
  Utils::sort(m_sortedSessions, cmp);
  m_currentSortColumn = column;
  m_currentSortOrder = order;
  endResetModel();
}

auto SessionModel::isDefaultVirgin() const -> bool
{
  return SessionManager::isDefaultVirgin();
}

auto SessionModel::resetSessions() -> void
{
  beginResetModel();
  m_sortedSessions = SessionManager::sessions();
  endResetModel();
}

auto SessionModel::newSession(QWidget *parent) -> void
{
  SessionNameInputDialog sessionInputDialog(parent);
  sessionInputDialog.setWindowTitle(tr("New Session Name"));
  sessionInputDialog.setActionText(tr("&Create"), tr("Create and &Open"));

  runSessionNameInputDialog(&sessionInputDialog, [](const QString &newName) {
    SessionManager::createSession(newName);
  });
}

auto SessionModel::cloneSession(QWidget *parent, const QString &session) -> void
{
  SessionNameInputDialog sessionInputDialog(parent);
  sessionInputDialog.setWindowTitle(tr("New Session Name"));
  sessionInputDialog.setActionText(tr("&Clone"), tr("Clone and &Open"));
  sessionInputDialog.setValue(session + " (2)");

  runSessionNameInputDialog(&sessionInputDialog, [session](const QString &newName) {
    SessionManager::cloneSession(session, newName);
  });
}

auto SessionModel::deleteSessions(const QStringList &sessions) -> void
{
  if (!SessionManager::confirmSessionDelete(sessions))
    return;
  beginResetModel();
  SessionManager::deleteSessions(sessions);
  m_sortedSessions = SessionManager::sessions();
  sort(m_currentSortColumn, m_currentSortOrder);
  endResetModel();
}

auto SessionModel::renameSession(QWidget *parent, const QString &session) -> void
{
  SessionNameInputDialog sessionInputDialog(parent);
  sessionInputDialog.setWindowTitle(tr("Rename Session"));
  sessionInputDialog.setActionText(tr("&Rename"), tr("Rename and &Open"));
  sessionInputDialog.setValue(session);

  runSessionNameInputDialog(&sessionInputDialog, [session](const QString &newName) {
    SessionManager::renameSession(session, newName);
  });
}

auto SessionModel::switchToSession(const QString &session) -> void
{
  SessionManager::loadSession(session);
  emit sessionSwitched();
}

auto SessionModel::runSessionNameInputDialog(SessionNameInputDialog *sessionInputDialog, std::function<void(const QString &)> createSession) -> void
{
  if (sessionInputDialog->exec() == QDialog::Accepted) {
    const auto newSession = sessionInputDialog->value();
    if (newSession.isEmpty() || SessionManager::sessions().contains(newSession))
      return;
    beginResetModel();
    createSession(newSession);
    m_sortedSessions = SessionManager::sessions();
    endResetModel();
    sort(m_currentSortColumn, m_currentSortOrder);

    if (sessionInputDialog->isSwitchToRequested())
      switchToSession(newSession);
    emit sessionCreated(newSession);
  }
}

} // namespace Internal
} // namespace ProjectExplorer
