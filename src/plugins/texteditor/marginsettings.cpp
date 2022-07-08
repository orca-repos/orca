// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "marginsettings.hpp"

#include <QSettings>
#include <QString>
#include <QVariantMap>

static constexpr char showWrapColumnKey[] = "ShowMargin";
static constexpr char wrapColumnKey[] = "MarginColumn";
static constexpr char groupPostfix[] = "MarginSettings";
static constexpr char useIndenterColumnKey[] = "UseIndenter";

using namespace TextEditor;

MarginSettings::MarginSettings() : m_showMargin(false), m_useIndenter(false), m_marginColumn(80) {}

auto MarginSettings::toSettings(const QString &category, QSettings *s) const -> void
{
  QString group = QLatin1String(groupPostfix);
  if (!category.isEmpty())
    group.insert(0, category);
  s->beginGroup(group);
  s->setValue(QLatin1String(showWrapColumnKey), m_showMargin);
  s->setValue(QLatin1String(useIndenterColumnKey), m_useIndenter);
  s->setValue(QLatin1String(wrapColumnKey), m_marginColumn);
  s->endGroup();
}

auto MarginSettings::fromSettings(const QString &category, const QSettings *s) -> void
{
  QString group = QLatin1String(groupPostfix);
  if (!category.isEmpty())
    group.insert(0, category);
  group += QLatin1Char('/');

  *this = MarginSettings(); // Assign defaults

  m_showMargin = s->value(group + QLatin1String(showWrapColumnKey), m_showMargin).toBool();
  m_useIndenter = s->value(group + QLatin1String(useIndenterColumnKey), m_useIndenter).toBool();
  m_marginColumn = s->value(group + QLatin1String(wrapColumnKey), m_marginColumn).toInt();
}

auto MarginSettings::toMap() const -> QVariantMap
{
  return {{showWrapColumnKey, m_showMargin}, {useIndenterColumnKey, m_useIndenter}, {wrapColumnKey, m_marginColumn}};
}

auto MarginSettings::fromMap(const QVariantMap &map) -> void
{
  m_showMargin = map.value(showWrapColumnKey, m_showMargin).toBool();
  m_useIndenter = map.value(useIndenterColumnKey, m_useIndenter).toBool();
  m_marginColumn = map.value(wrapColumnKey, m_marginColumn).toInt();
}

auto MarginSettings::equals(const MarginSettings &other) const -> bool
{
  return m_showMargin == other.m_showMargin && m_useIndenter == other.m_useIndenter && m_marginColumn == other.m_marginColumn;
}
