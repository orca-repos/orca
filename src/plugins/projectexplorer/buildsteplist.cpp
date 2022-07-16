// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "buildsteplist.hpp"

#include "buildconfiguration.hpp"
#include "buildmanager.hpp"
#include "buildstep.hpp"
#include "deployconfiguration.hpp"
#include "projectexplorer.hpp"
#include "target.hpp"

#include <utils/algorithm.hpp>

namespace ProjectExplorer {

constexpr char STEPS_COUNT_KEY[] = "ProjectExplorer.BuildStepList.StepsCount";
constexpr char STEPS_PREFIX[] = "ProjectExplorer.BuildStepList.Step.";

BuildStepList::BuildStepList(QObject *parent, Utils::Id id) : QObject(parent), m_id(id)
{
  QTC_ASSERT(parent, return);
  QTC_ASSERT(parent->parent(), return);
  m_target = qobject_cast<Target*>(parent->parent());
  QTC_ASSERT(m_target, return);
}

BuildStepList::~BuildStepList()
{
  clear();
}

auto BuildStepList::clear() -> void
{
  qDeleteAll(m_steps);
  m_steps.clear();
}

auto BuildStepList::toMap() const -> QVariantMap
{
  QVariantMap map;
  {
    // Only written for compatibility reasons within the 4.11 cycle
    const char CONFIGURATION_ID_KEY[] = "ProjectExplorer.ProjectConfiguration.Id";
    const char DISPLAY_NAME_KEY[] = "ProjectExplorer.ProjectConfiguration.DisplayName";
    const char DEFAULT_DISPLAY_NAME_KEY[] = "ProjectExplorer.ProjectConfiguration.DefaultDisplayName";
    map.insert(QLatin1String(CONFIGURATION_ID_KEY), m_id.toSetting());
    map.insert(QLatin1String(DISPLAY_NAME_KEY), displayName());
    map.insert(QLatin1String(DEFAULT_DISPLAY_NAME_KEY), displayName());
  }

  // Save build steps
  map.insert(QString::fromLatin1(STEPS_COUNT_KEY), m_steps.count());
  for (auto i = 0; i < m_steps.count(); ++i)
    map.insert(QString::fromLatin1(STEPS_PREFIX) + QString::number(i), m_steps.at(i)->toMap());

  return map;
}

auto BuildStepList::count() const -> int
{
  return m_steps.count();
}

auto BuildStepList::isEmpty() const -> bool
{
  return m_steps.isEmpty();
}

auto BuildStepList::contains(Utils::Id id) const -> bool
{
  return Utils::anyOf(steps(), [id](BuildStep *bs) {
    return bs->id() == id;
  });
}

auto BuildStepList::displayName() const -> QString
{
  if (m_id == Constants::BUILDSTEPS_BUILD) {
    //: Display name of the build build step list. Used as part of the labels in the project window.
    return tr("Build");
  }
  if (m_id == Constants::BUILDSTEPS_CLEAN) {
    //: Display name of the clean build step list. Used as part of the labels in the project window.
    return tr("Clean");
  }
  if (m_id == Constants::BUILDSTEPS_DEPLOY) {
    //: Display name of the deploy build step list. Used as part of the labels in the project window.
    return tr("Deploy");
  }
  QTC_CHECK(false);
  return {};
}

auto BuildStepList::fromMap(const QVariantMap &map) -> bool
{
  clear();

  const auto factories = BuildStepFactory::allBuildStepFactories();

  const auto maxSteps = map.value(QString::fromLatin1(STEPS_COUNT_KEY), 0).toInt();
  for (auto i = 0; i < maxSteps; ++i) {
    auto bsData(map.value(QString::fromLatin1(STEPS_PREFIX) + QString::number(i)).toMap());
    if (bsData.isEmpty()) {
      qWarning() << "No step data found for" << i << "(continuing).";
      continue;
    }
    auto handled = false;
    auto stepId = idFromMap(bsData);
    for (const auto factory : factories) {
      if (factory->stepId() == stepId) {
        if (factory->canHandle(this)) {
          if (const auto bs = factory->restore(this, bsData)) {
            appendStep(bs);
            handled = true;
          } else {
            qWarning() << "Restoration of step" << i << "failed (continuing).";
          }
        }
      }
    }
    QTC_ASSERT(handled, qDebug() << "No factory for build step" << stepId.toString() << "found.");
  }
  return true;
}

auto BuildStepList::steps() const -> QList<BuildStep*>
{
  return m_steps;
}

auto BuildStepList::firstStepWithId(Utils::Id id) const -> BuildStep*
{
  return findOrDefault(m_steps, equal(&BuildStep::id, id));
}

auto BuildStepList::insertStep(int position, BuildStep *step) -> void
{
  m_steps.insert(position, step);
  emit stepInserted(position);
}

auto BuildStepList::insertStep(int position, Utils::Id stepId) -> void
{
  for (const auto factory : BuildStepFactory::allBuildStepFactories()) {
    if (factory->stepId() == stepId) {
      const auto step = factory->create(this);
      QTC_ASSERT(step, break);
      insertStep(position, step);
      return;
    }
  }
  QTC_ASSERT(false, qDebug() << "No factory for build step" << stepId.toString() << "found.");
}

auto BuildStepList::removeStep(int position) -> bool
{
  const auto bs = at(position);
  if (BuildManager::isBuilding(bs))
    return false;

  emit aboutToRemoveStep(position);
  m_steps.removeAt(position);
  delete bs;
  emit stepRemoved(position);
  return true;
}

auto BuildStepList::moveStepUp(int position) -> void
{
  m_steps.swapItemsAt(position - 1, position);
  emit stepMoved(position, position - 1);
}

auto BuildStepList::at(int position) -> BuildStep*
{
  return m_steps.at(position);
}

} // ProjectExplorer
