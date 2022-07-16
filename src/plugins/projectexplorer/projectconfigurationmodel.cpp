// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectconfigurationmodel.hpp"

#include "buildconfiguration.hpp"
#include "deployconfiguration.hpp"
#include "runconfiguration.hpp"
#include "target.hpp"
#include "projectconfiguration.hpp"

#include <utils/algorithm.hpp>
#include <utils/stringutils.hpp>

/*!
    \class ProjectExplorer::ProjectConfigurationModel
    \brief The ProjectConfigurationModel class is a model to represent the build,
    deploy and run configurations of a target.

    To be used in the dropdown lists of comboboxes.
*/

namespace ProjectExplorer {

static auto isOrderedBefore(const ProjectConfiguration *a, const ProjectConfiguration *b) -> bool
{
  return Utils::caseFriendlyCompare(a->displayName(), b->displayName()) < 0;
}

ProjectConfigurationModel::ProjectConfigurationModel(Target *target) : m_target(target) {}

auto ProjectConfigurationModel::rowCount(const QModelIndex &parent) const -> int
{
  return parent.isValid() ? 0 : m_projectConfigurations.size();
}

auto ProjectConfigurationModel::columnCount(const QModelIndex &parent) const -> int
{
  return parent.isValid() ? 0 : 1;
}

auto ProjectConfigurationModel::displayNameChanged() -> void
{
  const auto pc = qobject_cast<ProjectConfiguration*>(sender());
  if (!pc)
    return;

  // Find the old position
  const int oldPos = m_projectConfigurations.indexOf(pc);
  if (oldPos < 0)
    return;

  QModelIndex itemIndex;
  if (oldPos >= 1 && isOrderedBefore(m_projectConfigurations.at(oldPos), m_projectConfigurations.at(oldPos - 1))) {
    // We need to move up
    auto newPos = oldPos - 1;
    while (newPos >= 0 && isOrderedBefore(m_projectConfigurations.at(oldPos), m_projectConfigurations.at(newPos))) {
      --newPos;
    }
    ++newPos;

    beginMoveRows(QModelIndex(), oldPos, oldPos, QModelIndex(), newPos);
    m_projectConfigurations.insert(newPos, pc);
    m_projectConfigurations.removeAt(oldPos + 1);
    endMoveRows();
    // Not only did we move, we also changed...
    itemIndex = index(newPos, 0);
  } else if (oldPos < m_projectConfigurations.size() - 1 && isOrderedBefore(m_projectConfigurations.at(oldPos + 1), m_projectConfigurations.at(oldPos))) {
    // We need to move down
    auto newPos = oldPos + 1;
    while (newPos < m_projectConfigurations.size() && isOrderedBefore(m_projectConfigurations.at(newPos), m_projectConfigurations.at(oldPos))) {
      ++newPos;
    }
    beginMoveRows(QModelIndex(), oldPos, oldPos, QModelIndex(), newPos);
    m_projectConfigurations.insert(newPos, pc);
    m_projectConfigurations.removeAt(oldPos);
    endMoveRows();

    // We need to subtract one since removing at the old place moves the newIndex down
    itemIndex = index(newPos - 1, 0);
  } else {
    itemIndex = index(oldPos, 0);
  }
  emit dataChanged(itemIndex, itemIndex);
}

auto ProjectConfigurationModel::data(const QModelIndex &index, int role) const -> QVariant
{
  if (role == Qt::DisplayRole) {
    const auto row = index.row();
    if (row < m_projectConfigurations.size())
      return m_projectConfigurations.at(row)->expandedDisplayName();
  }

  return QVariant();
}

auto ProjectConfigurationModel::projectConfigurationAt(int i) const -> ProjectConfiguration*
{
  if (i > m_projectConfigurations.size() || i < 0)
    return nullptr;
  return m_projectConfigurations.at(i);
}

auto ProjectConfigurationModel::indexFor(ProjectConfiguration *pc) const -> int
{
  return m_projectConfigurations.indexOf(pc);
}

auto ProjectConfigurationModel::addProjectConfiguration(ProjectConfiguration *pc) -> void
{
  // Find the right place to insert
  auto i = 0;
  for (; i < m_projectConfigurations.size(); ++i) {
    if (isOrderedBefore(pc, m_projectConfigurations.at(i)))
      break;
  }

  beginInsertRows(QModelIndex(), i, i);
  m_projectConfigurations.insert(i, pc);
  endInsertRows();

  connect(pc, &ProjectConfiguration::displayNameChanged, this, &ProjectConfigurationModel::displayNameChanged);
}

auto ProjectConfigurationModel::removeProjectConfiguration(ProjectConfiguration *pc) -> void
{
  const int i = m_projectConfigurations.indexOf(pc);
  if (i < 0)
    return;
  beginRemoveRows(QModelIndex(), i, i);
  m_projectConfigurations.removeAt(i);
  endRemoveRows();
}

} // ProjectExplorer
