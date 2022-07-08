// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "projectconfiguration.hpp"

#include "kitinformation.hpp"
#include "target.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

#include <QFormLayout>
#include <QWidget>

using namespace ProjectExplorer;
using namespace Utils;

constexpr char CONFIGURATION_ID_KEY[] = "ProjectExplorer.ProjectConfiguration.Id";
constexpr char DISPLAY_NAME_KEY[] = "ProjectExplorer.ProjectConfiguration.DisplayName";

// ProjectConfiguration

ProjectConfiguration::ProjectConfiguration(QObject *parent, Id id) : QObject(parent), m_id(id)
{
  m_aspects.setOwnsSubAspects(true);

  QTC_CHECK(parent);
  QTC_CHECK(id.isValid());
  setObjectName(id.toString());

  for (QObject *obj = this; obj; obj = obj->parent()) {
    m_target = qobject_cast<Target*>(obj);
    if (m_target)
      break;
  }
  QTC_CHECK(m_target);
}

ProjectConfiguration::~ProjectConfiguration() = default;

auto ProjectConfiguration::project() const -> Project*
{
  return m_target->project();
}

auto ProjectConfiguration::kit() const -> Kit*
{
  return m_target->kit();
}

auto ProjectConfiguration::id() const -> Id
{
  return m_id;
}

auto ProjectConfiguration::settingsIdKey() -> QString
{
  return QString(CONFIGURATION_ID_KEY);
}

auto ProjectConfiguration::setDisplayName(const QString &name) -> void
{
  if (m_displayName.setValue(name)) emit displayNameChanged();
}

auto ProjectConfiguration::setDefaultDisplayName(const QString &name) -> void
{
  if (m_displayName.setDefaultValue(name)) emit displayNameChanged();
}

auto ProjectConfiguration::setToolTip(const QString &text) -> void
{
  if (text == m_toolTip)
    return;
  m_toolTip = text;
  emit toolTipChanged();
}

auto ProjectConfiguration::toolTip() const -> QString
{
  return m_toolTip;
}

auto ProjectConfiguration::toMap() const -> QVariantMap
{
  QTC_CHECK(m_id.isValid());
  QVariantMap map;
  map.insert(QLatin1String(CONFIGURATION_ID_KEY), m_id.toSetting());
  m_displayName.toMap(map, DISPLAY_NAME_KEY);
  m_aspects.toMap(map);
  return map;
}

auto ProjectConfiguration::target() const -> Target*
{
  return m_target;
}

auto ProjectConfiguration::fromMap(const QVariantMap &map) -> bool
{
  const auto id = Id::fromSetting(map.value(QLatin1String(CONFIGURATION_ID_KEY)));
  // Note: This is only "startsWith", not ==, as RunConfigurations currently still
  // mangle in their build keys.
  QTC_ASSERT(id.toString().startsWith(m_id.toString()), return false);

  m_displayName.fromMap(map, DISPLAY_NAME_KEY);
  m_aspects.fromMap(map);
  return true;
}

auto ProjectConfiguration::aspect(Id id) const -> BaseAspect*
{
  return m_aspects.aspect(id);
}

auto ProjectConfiguration::acquaintAspects() -> void
{
  for (const auto aspect : m_aspects)
    aspect->acquaintSiblings(m_aspects);
}

auto ProjectConfiguration::doPostInit() -> void
{
  for (const auto &postInit : qAsConst(m_postInit))
    postInit();
}

auto ProjectConfiguration::mapFromBuildDeviceToGlobalPath(const FilePath &path) const -> FilePath
{
  const auto dev = BuildDeviceKitAspect::device(kit());
  QTC_ASSERT(dev, return path);
  return dev->mapToGlobalPath(path);
}

auto ProjectExplorer::idFromMap(const QVariantMap &map) -> Id
{
  return Id::fromSetting(map.value(QLatin1String(CONFIGURATION_ID_KEY)));
}

auto ProjectConfiguration::expandedDisplayName() const -> QString
{
  return m_target->macroExpander()->expand(m_displayName.value());
}
