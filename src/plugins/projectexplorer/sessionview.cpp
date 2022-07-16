// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "sessionview.hpp"

#include "session.hpp"

#include <utils/algorithm.hpp>

#include <QHeaderView>
#include <QItemSelection>
#include <QStringList>
#include <QStyledItemDelegate>

namespace ProjectExplorer {
namespace Internal {

// custom item delegate class
class RemoveItemFocusDelegate : public QStyledItemDelegate {
public:
  RemoveItemFocusDelegate(QObject *parent = nullptr) : QStyledItemDelegate(parent) { }

protected:
  auto paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void override;
};

auto RemoveItemFocusDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const -> void
{
  auto opt = option;
  opt.state &= ~QStyle::State_HasFocus;
  QStyledItemDelegate::paint(painter, opt, index);
}

SessionView::SessionView(QWidget *parent) : TreeView(parent)
{
  setItemDelegate(new RemoveItemFocusDelegate(this));
  setSelectionBehavior(SelectRows);
  setSelectionMode(ExtendedSelection);
  setWordWrap(false);
  setRootIsDecorated(false);
  setSortingEnabled(true);

  setModel(&m_sessionModel);
  sortByColumn(0, Qt::AscendingOrder);

  // Ensure that the full session name is visible.
  header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

  const QItemSelection firstRow(m_sessionModel.index(0, 0), m_sessionModel.index(0, m_sessionModel.columnCount() - 1));
  selectionModel()->select(firstRow, QItemSelectionModel::SelectCurrent);

  connect(this, &TreeView::activated, [this](const QModelIndex &index) {
    emit sessionActivated(m_sessionModel.sessionAt(index.row()));
  });
  connect(selectionModel(), &QItemSelectionModel::selectionChanged, [this] {
    emit sessionsSelected(selectedSessions());
  });

  connect(&m_sessionModel, &SessionModel::sessionSwitched, this, &SessionView::sessionSwitched);
  connect(&m_sessionModel, &SessionModel::modelReset, this, &SessionView::selectActiveSession);
  connect(&m_sessionModel, &SessionModel::sessionCreated, this, &SessionView::selectSession);
}

auto SessionView::createNewSession() -> void
{
  m_sessionModel.newSession(this);
}

auto SessionView::deleteSelectedSessions() -> void
{
  deleteSessions(selectedSessions());
}

auto SessionView::deleteSessions(const QStringList &sessions) -> void
{
  m_sessionModel.deleteSessions(sessions);
}

auto SessionView::cloneCurrentSession() -> void
{
  m_sessionModel.cloneSession(this, currentSession());
}

auto SessionView::renameCurrentSession() -> void
{
  m_sessionModel.renameSession(this, currentSession());
}

auto SessionView::switchToCurrentSession() -> void
{
  m_sessionModel.switchToSession(currentSession());
}

auto SessionView::currentSession() -> QString
{
  return m_sessionModel.sessionAt(selectionModel()->currentIndex().row());
}

auto SessionView::sessionModel() -> SessionModel*
{
  return &m_sessionModel;
}

auto SessionView::selectActiveSession() -> void
{
  selectSession(SessionManager::activeSession());
}

auto SessionView::selectSession(const QString &sessionName) -> void
{
  const auto row = m_sessionModel.indexOfSession(sessionName);
  selectionModel()->setCurrentIndex(model()->index(row, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

auto SessionView::showEvent(QShowEvent *event) -> void
{
  TreeView::showEvent(event);
  selectActiveSession();
  setFocus();
}

auto SessionView::keyPressEvent(QKeyEvent *event) -> void
{
  if (event->key() != Qt::Key_Delete && event->key() != Qt::Key_Backspace) {
    TreeView::keyPressEvent(event);
    return;
  }
  const auto sessions = selectedSessions();
  if (!sessions.contains("default") && !Utils::anyOf(sessions, [](const QString &session) { return session == SessionManager::activeSession(); })) {
    deleteSessions(sessions);
  }
}

auto SessionView::selectedSessions() const -> QStringList
{
  return Utils::transform(selectionModel()->selectedRows(), [this](const QModelIndex &index) {
    return m_sessionModel.sessionAt(index.row());
  });
}

} // namespace Internal
} // namespace ProjectExplorer
