// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QAbstractItemModel>

#include <functional>

namespace ProjectExplorer {
class Target;
class ProjectConfiguration;

// Documentation inside.
class ProjectConfigurationModel : public QAbstractListModel {
  Q_OBJECT

public:
  explicit ProjectConfigurationModel(Target *target);

  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto projectConfigurationAt(int i) const -> ProjectConfiguration*;
  auto indexFor(ProjectConfiguration *pc) const -> int;
  auto addProjectConfiguration(ProjectConfiguration *pc) -> void;
  auto removeProjectConfiguration(ProjectConfiguration *pc) -> void;

private:
  auto displayNameChanged() -> void;

  Target *m_target;
  QList<ProjectConfiguration*> m_projectConfigurations;
};

} // namespace ProjectExplorer
