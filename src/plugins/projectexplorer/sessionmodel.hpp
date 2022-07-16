// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QAbstractTableModel>

#include <functional>

namespace ProjectExplorer {
namespace Internal {

constexpr char SESSION_BASE_ID[] = "Welcome.OpenSession";

class SessionNameInputDialog;

class SessionModel : public QAbstractTableModel {
  Q_OBJECT

public:
  enum {
    DefaultSessionRole = Qt::UserRole + 1,
    LastSessionRole,
    ActiveSessionRole,
    ProjectsPathRole,
    ProjectsDisplayRole,
    ShortcutRole
  };

  explicit SessionModel(QObject *parent = nullptr);

  auto indexOfSession(const QString &session) -> int;
  auto sessionAt(int row) const -> QString;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto headerData(int section, Qt::Orientation orientation, int role) const -> QVariant override;
  auto data(const QModelIndex &index, int role) const -> QVariant override;
  auto roleNames() const -> QHash<int, QByteArray> override;
  auto sort(int column, Qt::SortOrder order = Qt::AscendingOrder) -> void override;
  auto isDefaultVirgin() const -> Q_SCRIPTABLE bool;

signals:
  auto sessionSwitched() -> void;
  auto sessionCreated(const QString &sessionName) -> void;

public slots:
  auto resetSessions() -> void;
  auto newSession(QWidget *parent) -> void;
  auto cloneSession(QWidget *parent, const QString &session) -> void;
  auto deleteSessions(const QStringList &sessions) -> void;
  auto renameSession(QWidget *parent, const QString &session) -> void;
  auto switchToSession(const QString &session) -> void;

private:
  auto runSessionNameInputDialog(SessionNameInputDialog *sessionInputDialog, std::function<void(const QString &)> createSession) -> void;

  QStringList m_sortedSessions;
  int m_currentSortColumn = 0;
  Qt::SortOrder m_currentSortOrder = Qt::AscendingOrder;
};

} // namespace Internal
} // namespace ProjectExplorer
