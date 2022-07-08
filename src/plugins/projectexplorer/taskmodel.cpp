// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "taskmodel.hpp"

#include "fileinsessionfinder.hpp"
#include "task.hpp"
#include "taskhub.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

#include <QFileInfo>
#include <QFontMetrics>

#include <algorithm>
#include <functional>

using namespace std::placeholders;

namespace ProjectExplorer {
namespace Internal {

/////
// TaskModel
/////

TaskModel::TaskModel(QObject *parent) : QAbstractItemModel(parent)
{
  m_categories.insert(Utils::Id(), CategoryData());
}

auto TaskModel::taskCount(Utils::Id categoryId) -> int
{
  return m_categories.value(categoryId).count;
}

auto TaskModel::errorTaskCount(Utils::Id categoryId) -> int
{
  return m_categories.value(categoryId).errors;
}

auto TaskModel::warningTaskCount(Utils::Id categoryId) -> int
{
  return m_categories.value(categoryId).warnings;
}

auto TaskModel::unknownTaskCount(Utils::Id categoryId) -> int
{
  return m_categories.value(categoryId).count - m_categories.value(categoryId).errors - m_categories.value(categoryId).warnings;
}

auto TaskModel::hasFile(const QModelIndex &index) const -> bool
{
  const auto row = index.row();
  if (!index.isValid() || row < 0 || row >= m_tasks.count())
    return false;
  return !m_tasks.at(row).file.isEmpty();
}

auto TaskModel::addCategory(Utils::Id categoryId, const QString &categoryName, int priority) -> void
{
  QTC_ASSERT(categoryId.isValid(), return);
  CategoryData data;
  data.displayName = categoryName;
  data.priority = priority;
  m_categories.insert(categoryId, data);
}

auto TaskModel::tasks(Utils::Id categoryId) const -> Tasks
{
  if (!categoryId.isValid())
    return m_tasks;

  Tasks taskList;
  foreach(const Task &t, m_tasks) {
    if (t.category == categoryId)
      taskList.append(t);
  }
  return taskList;
}

auto TaskModel::compareTasks(const Task &task1, const Task &task2) -> bool
{
  if (task1.category == task2.category)
    return task1.taskId < task2.taskId;

  // Higher-priority task should appear higher up in the view and thus compare less-than.
  const auto prio1 = m_categories.value(task1.category).priority;
  const auto prio2 = m_categories.value(task2.category).priority;
  if (prio1 < prio2)
    return false;
  if (prio1 > prio2)
    return true;

  return task1.taskId < task2.taskId;
}

auto TaskModel::addTask(const Task &task) -> void
{
  Q_ASSERT(m_categories.keys().contains(task.category));
  auto &data = m_categories[task.category];
  auto &global = m_categories[Utils::Id()];

  const auto it = std::lower_bound(m_tasks.begin(), m_tasks.end(), task, std::bind(&TaskModel::compareTasks, this, _1, task));
  const int i = it - m_tasks.begin();
  beginInsertRows(QModelIndex(), i, i);
  m_tasks.insert(it, task);
  data.addTask(task);
  global.addTask(task);
  endInsertRows();
}

auto TaskModel::removeTask(unsigned int id) -> void
{
  for (auto index = 0; index < m_tasks.length(); ++index) {
    if (m_tasks.at(index).taskId != id)
      continue;
    const auto &t = m_tasks.at(index);
    beginRemoveRows(QModelIndex(), index, index);
    m_categories[t.category].removeTask(t);
    m_categories[Utils::Id()].removeTask(t);
    m_tasks.removeAt(index);
    endRemoveRows();
    break;
  }
}

auto TaskModel::rowForTask(const Task &task) -> int
{
  const auto it = std::lower_bound(m_tasks.constBegin(), m_tasks.constEnd(), task, std::bind(&TaskModel::compareTasks, this, _1, task));
  if (it == m_tasks.constEnd())
    return -1;
  return it - m_tasks.constBegin();
}

auto TaskModel::updateTaskFileName(const Task &task, const QString &fileName) -> void
{
  const auto i = rowForTask(task);
  QTC_ASSERT(i != -1, return);
  if (m_tasks.at(i).taskId == task.taskId) {
    m_tasks[i].file = Utils::FilePath::fromString(fileName);
    const auto itemIndex = index(i, 0);
    emit dataChanged(itemIndex, itemIndex);
  }
}

auto TaskModel::updateTaskLineNumber(const Task &task, int line) -> void
{
  const auto i = rowForTask(task);
  QTC_ASSERT(i != -1, return);
  if (m_tasks.at(i).taskId == task.taskId) {
    m_tasks[i].movedLine = line;
    const auto itemIndex = index(i, 0);
    emit dataChanged(itemIndex, itemIndex);
  }
}

auto TaskModel::clearTasks(Utils::Id categoryId) -> void
{
  using IdCategoryConstIt = QHash<Utils::Id, CategoryData>::ConstIterator;

  if (!categoryId.isValid()) {
    if (m_tasks.isEmpty())
      return;
    beginRemoveRows(QModelIndex(), 0, m_tasks.count() - 1);
    m_tasks.clear();
    const auto cend = m_categories.constEnd();
    for (auto it = m_categories.constBegin(); it != cend; ++it)
      m_categories[it.key()].clear();
    endRemoveRows();
  } else {
    auto index = 0;
    auto start = 0;
    auto &global = m_categories[Utils::Id()];
    auto &cat = m_categories[categoryId];

    while (index < m_tasks.count()) {
      while (index < m_tasks.count() && m_tasks.at(index).category != categoryId) {
        ++start;
        ++index;
      }
      if (index == m_tasks.count())
        break;
      while (index < m_tasks.count() && m_tasks.at(index).category == categoryId)
        ++index;

      // Index is now on the first non category
      beginRemoveRows(QModelIndex(), start, index - 1);

      for (auto i = start; i < index; ++i) {
        global.removeTask(m_tasks.at(i));
        cat.removeTask(m_tasks.at(i));
      }

      m_tasks.erase(m_tasks.begin() + start, m_tasks.begin() + index);

      endRemoveRows();
      index = start;
    }
  }
  m_maxSizeOfFileName = 0;
  m_lastMaxSizeIndex = 0;
}

auto TaskModel::index(int row, int column, const QModelIndex &parent) const -> QModelIndex
{
  if (parent.isValid())
    return QModelIndex();
  return createIndex(row, column);
}

auto TaskModel::parent(const QModelIndex &child) const -> QModelIndex
{
  Q_UNUSED(child)
  return QModelIndex();
}

auto TaskModel::rowCount(const QModelIndex &parent) const -> int
{
  return parent.isValid() ? 0 : m_tasks.count();
}

auto TaskModel::columnCount(const QModelIndex &parent) const -> int
{
  return parent.isValid() ? 0 : 1;
}

auto TaskModel::data(const QModelIndex &index, int role) const -> QVariant
{
  const auto row = index.row();
  if (!index.isValid() || row < 0 || row >= m_tasks.count() || index.column() != 0)
    return QVariant();

  if (role == File)
    return m_tasks.at(index.row()).file.toString();
  else if (role == Line)
    return m_tasks.at(index.row()).line;
  else if (role == MovedLine)
    return m_tasks.at(index.row()).movedLine;
  else if (role == Description)
    return m_tasks.at(index.row()).description();
  else if (role == FileNotFound)
    return m_fileNotFound.value(m_tasks.at(index.row()).file.toString());
  else if (role == Type)
    return (int)m_tasks.at(index.row()).type;
  else if (role == Category)
    return m_tasks.at(index.row()).category.uniqueIdentifier();
  else if (role == Icon)
    return m_tasks.at(index.row()).icon();
  else if (role == Task_t)
    return QVariant::fromValue(task(index));
  return QVariant();
}

auto TaskModel::task(const QModelIndex &index) const -> Task
{
  const auto row = index.row();
  if (!index.isValid() || row < 0 || row >= m_tasks.count())
    return Task();
  return m_tasks.at(row);
}

auto TaskModel::tasks(const QModelIndexList &indexes) const -> Tasks
{
  return Utils::filtered(Utils::transform<Tasks>(indexes, [this](const QModelIndex &i) { return task(i); }), [](const Task &t) { return !t.isNull(); });
}

auto TaskModel::categoryIds() const -> QList<Utils::Id>
{
  auto categories = m_categories.keys();
  categories.removeAll(Utils::Id()); // remove global category we added for bookkeeping
  return categories;
}

auto TaskModel::categoryDisplayName(Utils::Id categoryId) const -> QString
{
  return m_categories.value(categoryId).displayName;
}

auto TaskModel::sizeOfFile(const QFont &font) -> int
{
  const int count = m_tasks.count();
  if (count == 0)
    return 0;

  if (m_maxSizeOfFileName > 0 && font == m_fileMeasurementFont && m_lastMaxSizeIndex == count - 1)
    return m_maxSizeOfFileName;

  const QFontMetrics fm(font);
  m_fileMeasurementFont = font;

  for (auto i = m_lastMaxSizeIndex; i < count; ++i) {
    auto filename = m_tasks.at(i).file.toString();
    const int pos = filename.lastIndexOf(QLatin1Char('/'));
    if (pos != -1)
      filename = filename.mid(pos + 1);

    m_maxSizeOfFileName = qMax(m_maxSizeOfFileName, fm.horizontalAdvance(filename));
  }
  m_lastMaxSizeIndex = count - 1;
  return m_maxSizeOfFileName;
}

auto TaskModel::sizeOfLineNumber(const QFont &font) -> int
{
  if (m_sizeOfLineNumber == 0 || font != m_lineMeasurementFont) {
    const QFontMetrics fm(font);
    m_lineMeasurementFont = font;
    m_sizeOfLineNumber = fm.horizontalAdvance(QLatin1String("88888"));
  }
  return m_sizeOfLineNumber;
}

auto TaskModel::setFileNotFound(const QModelIndex &idx, bool b) -> void
{
  const auto row = idx.row();
  if (!idx.isValid() || row < 0 || row >= m_tasks.count())
    return;
  m_fileNotFound.insert(m_tasks[row].file.toUserOutput(), b);
  emit dataChanged(idx, idx);
}

/////
// TaskFilterModel
/////

TaskFilterModel::TaskFilterModel(TaskModel *sourceModel, QObject *parent) : QSortFilterProxyModel(parent)
{
  QTC_ASSERT(sourceModel, return);
  setSourceModel(sourceModel);
  m_includeUnknowns = m_includeWarnings = m_includeErrors = true;
}

auto TaskFilterModel::setFilterIncludesWarnings(bool b) -> void
{
  m_includeWarnings = b;
  m_includeUnknowns = b; // "Unknowns" are often associated with warnings
  invalidateFilter();
}

auto TaskFilterModel::tasks(const QModelIndexList &indexes) const -> Tasks
{
  return taskModel()->tasks(Utils::transform(indexes, [this](const QModelIndex &i) {
    return mapToSource(i);
  }));
}

auto TaskFilterModel::issuesCount(int startRow, int endRow) const -> int
{
  auto count = 0;
  for (auto r = startRow; r <= endRow; ++r) {
    if (task(index(r, 0)).type != Task::Unknown)
      ++count;
  }
  return count;
}

auto TaskFilterModel::updateFilterProperties(const QString &filterText, Qt::CaseSensitivity caseSensitivity, bool isRegexp, bool isInverted) -> void
{
  if (filterText == m_filterText && m_filterCaseSensitivity == caseSensitivity && m_filterStringIsRegexp == isRegexp && m_filterIsInverted == isInverted) {
    return;
  }
  m_filterText = filterText;
  m_filterCaseSensitivity = caseSensitivity;
  m_filterStringIsRegexp = isRegexp;
  m_filterIsInverted = isInverted;
  if (m_filterStringIsRegexp) {
    m_filterRegexp.setPattern(m_filterText);
    m_filterRegexp.setPatternOptions(m_filterCaseSensitivity == Qt::CaseInsensitive ? QRegularExpression::CaseInsensitiveOption : QRegularExpression::NoPatternOption);
  }
  invalidateFilter();
}

auto TaskFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const -> bool
{
  Q_UNUSED(source_parent)
  return filterAcceptsTask(taskModel()->tasks().at(source_row));
}

auto TaskFilterModel::filterAcceptsTask(const Task &task) const -> bool
{
  auto accept = true;
  switch (task.type) {
  case Task::Unknown:
    accept = m_includeUnknowns;
    break;
  case Task::Warning:
    accept = m_includeWarnings;
    break;
  case Task::Error:
    accept = m_includeErrors;
    break;
  }

  if (accept && m_categoryIds.contains(task.category))
    accept = false;

  if (accept && !m_filterText.isEmpty()) {
    const auto accepts = [this](const QString &s) {
      return m_filterStringIsRegexp ? m_filterRegexp.isValid() && s.contains(m_filterRegexp) : s.contains(m_filterText, m_filterCaseSensitivity);
    };
    if ((accepts(task.file.toString()) || accepts(task.description())) == m_filterIsInverted)
      accept = false;
  }

  return accept;
}

} // namespace Internal
} // namespace ProjectExplorer
