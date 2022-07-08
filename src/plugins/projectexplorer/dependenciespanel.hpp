// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QAbstractListModel>

#include <QTreeView>

QT_BEGIN_NAMESPACE
class QCheckBox;
QT_END_NAMESPACE

namespace Utils {
class DetailsWidget;
}

namespace ProjectExplorer {

class Project;

namespace Internal {

//
// DependenciesModel
//

class DependenciesModel : public QAbstractListModel {
  Q_OBJECT

public:
  explicit DependenciesModel(Project *project, QObject *parent = nullptr);

  auto rowCount(const QModelIndex &index) const -> int override;
  auto columnCount(const QModelIndex &index) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) -> bool override;
  auto flags(const QModelIndex &index) const -> Qt::ItemFlags override;

private:
  auto resetModel() -> void;

  Project *m_project;
  QList<Project*> m_projects;
};

class DependenciesView : public QTreeView {
  Q_OBJECT

public:
  explicit DependenciesView(QWidget *parent);

  auto sizeHint() const -> QSize override;
  auto setModel(QAbstractItemModel *model) -> void override;

private:
  auto updateSizeHint() -> void;

  QSize m_sizeHint;
};

class DependenciesWidget : public QWidget {
  Q_OBJECT

public:
  explicit DependenciesWidget(Project *project, QWidget *parent = nullptr);

private:
  Project *m_project;
  DependenciesModel *m_model;
  Utils::DetailsWidget *m_detailsContainer;
  QCheckBox *m_cascadeSetActiveCheckBox;
};

} // namespace Internal
} // namespace ProjectExplorer
