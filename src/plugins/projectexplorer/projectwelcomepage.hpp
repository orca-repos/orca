// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core-welcome-page-interface.hpp>

#include <QAbstractListModel>

namespace ProjectExplorer {
namespace Internal {

class SessionModel;
class SessionsPage;

class ProjectModel : public QAbstractListModel {
  Q_OBJECT

public:
  enum {
    FilePathRole = Qt::UserRole + 1,
    PrettyFilePathRole,
    ShortcutRole
  };

  ProjectModel(QObject *parent = nullptr);

  auto rowCount(const QModelIndex &parent) const -> int override;
  auto data(const QModelIndex &index, int role) const -> QVariant override;
  auto roleNames() const -> QHash<int, QByteArray> override;

public slots:
  auto resetProjects() -> void;
};

class ProjectWelcomePage : public Orca::Plugin::Core::IWelcomePage {
  Q_OBJECT

public:
  ProjectWelcomePage();

  auto title() const -> QString override { return tr("Projects"); }
  auto priority() const -> int override { return 20; }
  auto id() const -> Utils::Id override;
  auto createWidget() const -> QWidget* override;

  auto reloadWelcomeScreenData() const -> void;

public slots:
  auto newProject() -> void;
  auto openProject() -> void;

signals:
  auto requestProject(const QString &project) -> void;
  auto manageSessions() -> void;

private:
  auto openSessionAt(int index) -> void;
  auto openProjectAt(int index) -> void;
  auto createActions() -> void;

  friend class SessionsPage;
  SessionModel *m_sessionModel = nullptr;
  ProjectModel *m_projectModel = nullptr;
};

} // namespace Internal
} // namespace ProjectExplorer
