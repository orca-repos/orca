// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "behaviorsettings.hpp"

#include <utils/settingsutils.hpp>

#include <QSettings>
#include <QString>

static constexpr char mouseHidingKey[] = "MouseHiding";
static constexpr char mouseNavigationKey[] = "MouseNavigation";
static constexpr char scrollWheelZoomingKey[] = "ScrollWheelZooming";
static constexpr char constrainTooltips[] = "ConstrainTooltips";
static constexpr char camelCaseNavigationKey[] = "CamelCaseNavigation";
static constexpr char keyboardTooltips[] = "KeyboardTooltips";
static constexpr char groupPostfix[] = "BehaviorSettings";
static constexpr char smartSelectionChanging[] = "SmartSelectionChanging";

namespace TextEditor {

BehaviorSettings::BehaviorSettings() : m_mouseHiding(true), m_mouseNavigation(true), m_scrollWheelZooming(true), m_constrainHoverTooltips(false), m_camelCaseNavigation(true), m_keyboardTooltips(false), m_smartSelectionChanging(true) {}

auto BehaviorSettings::toSettings(const QString &category, QSettings *s) const -> void
{
  Utils::toSettings(QLatin1String(groupPostfix), category, s, this);
}

auto BehaviorSettings::fromSettings(const QString &category, QSettings *s) -> void
{
  *this = BehaviorSettings();
  Utils::fromSettings(QLatin1String(groupPostfix), category, s, this);
}

auto BehaviorSettings::toMap() const -> QVariantMap
{
  return {{mouseHidingKey, m_mouseHiding}, {mouseNavigationKey, m_mouseNavigation}, {scrollWheelZoomingKey, m_scrollWheelZooming}, {constrainTooltips, m_constrainHoverTooltips}, {camelCaseNavigationKey, m_camelCaseNavigation}, {keyboardTooltips, m_keyboardTooltips}, {smartSelectionChanging, m_smartSelectionChanging}};
}

auto BehaviorSettings::fromMap(const QVariantMap &map) -> void
{
  m_mouseHiding = map.value(mouseHidingKey, m_mouseHiding).toBool();
  m_mouseNavigation = map.value(mouseNavigationKey, m_mouseNavigation).toBool();
  m_scrollWheelZooming = map.value(scrollWheelZoomingKey, m_scrollWheelZooming).toBool();
  m_constrainHoverTooltips = map.value(constrainTooltips, m_constrainHoverTooltips).toBool();
  m_camelCaseNavigation = map.value(camelCaseNavigationKey, m_camelCaseNavigation).toBool();
  m_keyboardTooltips = map.value(keyboardTooltips, m_keyboardTooltips).toBool();
  m_smartSelectionChanging = map.value(smartSelectionChanging, m_smartSelectionChanging).toBool();
}

auto BehaviorSettings::equals(const BehaviorSettings &ds) const -> bool
{
  return m_mouseHiding == ds.m_mouseHiding && m_mouseNavigation == ds.m_mouseNavigation && m_scrollWheelZooming == ds.m_scrollWheelZooming && m_constrainHoverTooltips == ds.m_constrainHoverTooltips && m_camelCaseNavigation == ds.m_camelCaseNavigation && m_keyboardTooltips == ds.m_keyboardTooltips && m_smartSelectionChanging == ds.m_smartSelectionChanging;
}

} // namespace TextEditor

