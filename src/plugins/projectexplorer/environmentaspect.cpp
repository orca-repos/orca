// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "environmentaspect.hpp"

#include "environmentaspectwidget.hpp"
#include "target.hpp"

#include <utils/qtcassert.hpp>

using namespace Utils;

static constexpr char BASE_KEY[] = "PE.EnvironmentAspect.Base";
static constexpr char CHANGES_KEY[] = "PE.EnvironmentAspect.Changes";

namespace ProjectExplorer {

// --------------------------------------------------------------------
// EnvironmentAspect:
// --------------------------------------------------------------------

EnvironmentAspect::EnvironmentAspect()
{
  setDisplayName(tr("Environment"));
  setId("EnvironmentAspect");
  setConfigWidgetCreator([this] { return new EnvironmentAspectWidget(this); });
}

auto EnvironmentAspect::baseEnvironmentBase() const -> int
{
  return m_base;
}

auto EnvironmentAspect::setBaseEnvironmentBase(int base) -> void
{
  QTC_ASSERT(base >= 0 && base < m_baseEnvironments.size(), return);
  if (m_base != base) {
    m_base = base;
    emit baseEnvironmentChanged();
  }
}

auto EnvironmentAspect::setUserEnvironmentChanges(const EnvironmentItems &diff) -> void
{
  if (m_userChanges != diff) {
    m_userChanges = diff;
    emit userEnvironmentChangesChanged(m_userChanges);
    emit environmentChanged();
  }
}

auto EnvironmentAspect::environment() const -> Environment
{
  auto env = modifiedBaseEnvironment();
  env.modify(m_userChanges);
  return env;
}

auto EnvironmentAspect::modifiedBaseEnvironment() const -> Environment
{
  QTC_ASSERT(m_base >= 0 && m_base < m_baseEnvironments.size(), return Environment());
  auto env = m_baseEnvironments.at(m_base).unmodifiedBaseEnvironment();
  for (const auto &modifier : m_modifiers)
    modifier(env);
  return env;
}

auto EnvironmentAspect::displayNames() const -> const QStringList
{
  return transform(m_baseEnvironments, &BaseEnvironment::displayName);
}

auto EnvironmentAspect::addModifier(const EnvironmentModifier &modifier) -> void
{
  m_modifiers.append(modifier);
}

auto EnvironmentAspect::addSupportedBaseEnvironment(const QString &displayName, const std::function<Environment()> &getter) -> void
{
  BaseEnvironment baseEnv;
  baseEnv.displayName = displayName;
  baseEnv.getter = getter;
  m_baseEnvironments.append(baseEnv);
  if (m_base == -1)
    setBaseEnvironmentBase(m_baseEnvironments.size() - 1);
}

auto EnvironmentAspect::addPreferredBaseEnvironment(const QString &displayName, const std::function<Environment()> &getter) -> void
{
  BaseEnvironment baseEnv;
  baseEnv.displayName = displayName;
  baseEnv.getter = getter;
  m_baseEnvironments.append(baseEnv);
  setBaseEnvironmentBase(m_baseEnvironments.size() - 1);
}

auto EnvironmentAspect::fromMap(const QVariantMap &map) -> void
{
  m_base = map.value(QLatin1String(BASE_KEY), -1).toInt();
  m_userChanges = EnvironmentItem::fromStringList(map.value(QLatin1String(CHANGES_KEY)).toStringList());
}

auto EnvironmentAspect::toMap(QVariantMap &data) const -> void
{
  data.insert(QLatin1String(BASE_KEY), m_base);
  data.insert(QLatin1String(CHANGES_KEY), EnvironmentItem::toStringList(m_userChanges));
}

auto EnvironmentAspect::currentDisplayName() const -> QString
{
  QTC_ASSERT(m_base >= 0 && m_base < m_baseEnvironments.size(), return {});
  return m_baseEnvironments[m_base].displayName;
}

auto EnvironmentAspect::BaseEnvironment::unmodifiedBaseEnvironment() const -> Environment
{
  return getter ? getter() : Environment();
}

} // namespace ProjectExplorer
