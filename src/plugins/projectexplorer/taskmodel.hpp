// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QSortFilterProxyModel>

#include <QIcon>
#include <QRegularExpression>

#include "task.hpp"

namespace ProjectExplorer {
namespace Internal {

class TaskModel : public QAbstractItemModel {
  Q_OBJECT

public:
  // Model stuff
  explicit TaskModel(QObject *parent);

  auto index(int row, int column, const QModelIndex &parent = QModelIndex()) const -> QModelIndex override;
  auto parent(const QModelIndex &child) const -> QModelIndex override;
  auto rowCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto columnCount(const QModelIndex &parent = QModelIndex()) const -> int override;
  auto data(const QModelIndex &index, int role = Qt::DisplayRole) const -> QVariant override;
  auto task(const QModelIndex &index) const -> Task;
  auto tasks(const QModelIndexList &indexes) const -> Tasks;
  auto categoryIds() const -> QList<Utils::Id>;
  auto categoryDisplayName(Utils::Id categoryId) const -> QString;
  auto addCategory(Utils::Id categoryId, const QString &categoryName, int priority) -> void;
  auto tasks(Utils::Id categoryId = Utils::Id()) const -> Tasks;
  auto addTask(const Task &t) -> void;
  auto removeTask(unsigned int id) -> void;
  auto clearTasks(Utils::Id categoryId = Utils::Id()) -> void;
  auto updateTaskFileName(const Task &task, const QString &fileName) -> void;
  auto updateTaskLineNumber(const Task &task, int line) -> void;
  auto sizeOfFile(const QFont &font) -> int;
  auto sizeOfLineNumber(const QFont &font) -> int;
  auto setFileNotFound(const QModelIndex &index, bool b) -> void;

  enum Roles {
    File = Qt::UserRole,
    Line,
    MovedLine,
    Description,
    FileNotFound,
    Type,
    Category,
    Icon,
    Task_t
  };

  auto taskCount(Utils::Id categoryId) -> int;
  auto errorTaskCount(Utils::Id categoryId) -> int;
  auto warningTaskCount(Utils::Id categoryId) -> int;
  auto unknownTaskCount(Utils::Id categoryId) -> int;
  auto hasFile(const QModelIndex &index) const -> bool;
  auto rowForTask(const Task &task) -> int;

private:
  auto compareTasks(const Task &t1, const Task &t2) -> bool;

  class CategoryData {
  public:
    auto addTask(const Task &task) -> void
    {
      ++count;
      if (task.type == Task::Warning)
        ++warnings;
      else if (task.type == Task::Error)
        ++errors;
    }

    auto removeTask(const Task &task) -> void
    {
      --count;
      if (task.type == Task::Warning)
        --warnings;
      else if (task.type == Task::Error)
        --errors;
    }

    auto clear() -> void
    {
      count = 0;
      warnings = 0;
      errors = 0;
    }

    QString displayName;
    int priority = 0;
    int count = 0;
    int warnings = 0;
    int errors = 0;
  };

  QHash<Utils::Id, CategoryData> m_categories; // category id to data
  Tasks m_tasks;                               // all tasks (in order of id)
  QHash<QString, bool> m_fileNotFound;
  QFont m_fileMeasurementFont;
  QFont m_lineMeasurementFont;
  int m_maxSizeOfFileName = 0;
  int m_lastMaxSizeIndex = 0;
  int m_sizeOfLineNumber = 0;
};

class TaskFilterModel : public QSortFilterProxyModel {
  Q_OBJECT

public:
  TaskFilterModel(TaskModel *sourceModel, QObject *parent = nullptr);

  auto taskModel() const -> TaskModel* { return static_cast<TaskModel*>(sourceModel()); }
  auto filterIncludesWarnings() const -> bool { return m_includeWarnings; }
  auto setFilterIncludesWarnings(bool b) -> void;
  auto filterIncludesErrors() const -> bool { return m_includeErrors; }

  auto setFilterIncludesErrors(bool b) -> void
  {
    m_includeErrors = b;
    invalidateFilter();
  }

  auto filteredCategories() const -> QList<Utils::Id> { return m_categoryIds; }

  auto setFilteredCategories(const QList<Utils::Id> &categoryIds) -> void
  {
    m_categoryIds = categoryIds;
    invalidateFilter();
  }

  auto task(const QModelIndex &index) const -> Task { return taskModel()->task(mapToSource(index)); }
  auto tasks(const QModelIndexList &indexes) const -> Tasks;
  auto issuesCount(int startRow, int endRow) const -> int;
  auto hasFile(const QModelIndex &index) const -> bool { return taskModel()->hasFile(mapToSource(index)); }
  auto updateFilterProperties(const QString &filterText, Qt::CaseSensitivity caseSensitivity, bool isRegex, bool isInverted) -> void;

private:
  auto filterAcceptsRow(int source_row, const QModelIndex &source_parent) const -> bool override;
  auto filterAcceptsTask(const Task &task) const -> bool;

  bool m_beginRemoveRowsSent = false;
  bool m_includeUnknowns;
  bool m_includeWarnings;
  bool m_includeErrors;
  bool m_filterStringIsRegexp = false;
  bool m_filterIsInverted = false;
  Qt::CaseSensitivity m_filterCaseSensitivity = Qt::CaseInsensitive;
  QList<Utils::Id> m_categoryIds;
  QString m_filterText;
  QRegularExpression m_filterRegexp;
};

} // namespace Internal
} // namespace ProjectExplorer
