// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "sessionmodel.hpp"

#include <utils/itemviews.hpp>

#include <QAbstractTableModel>

namespace ProjectExplorer {
namespace Internal {

class SessionView : public Utils::TreeView {
  Q_OBJECT
public:
  explicit SessionView(QWidget *parent = nullptr);

  auto createNewSession() -> void;
  auto deleteSelectedSessions() -> void;
  auto cloneCurrentSession() -> void;
  auto renameCurrentSession() -> void;
  auto switchToCurrentSession() -> void;
  auto currentSession() -> QString;
  auto sessionModel() -> SessionModel*;
  auto selectActiveSession() -> void;
  auto selectSession(const QString &sessionName) -> void;

signals:
  auto sessionActivated(const QString &session) -> void;
  auto sessionsSelected(const QStringList &sessions) -> void;
  auto sessionSwitched() -> void;

private:
  auto showEvent(QShowEvent *event) -> void override;
  auto keyPressEvent(QKeyEvent *event) -> void override;
  auto deleteSessions(const QStringList &sessions) -> void;
  auto selectedSessions() const -> QStringList;

  SessionModel m_sessionModel;
};

} // namespace Internal
} // namespace ProjectExplorer
