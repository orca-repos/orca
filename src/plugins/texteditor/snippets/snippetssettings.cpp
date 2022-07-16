// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "snippetssettings.hpp"
#include "reuse.hpp"

#include <QSettings>

namespace {

const QLatin1String kGroupPostfix("SnippetsSettings");
const QLatin1String kLastUsedSnippetGroup("LastUsedSnippetGroup");

} // Anonymous

using namespace TextEditor;
using namespace Internal;

auto SnippetsSettings::toSettings(const QString &category, QSettings *s) const -> void
{
  const QString &group = category + kGroupPostfix;
  s->beginGroup(group);
  s->setValue(kLastUsedSnippetGroup, m_lastUsedSnippetGroup);
  s->endGroup();
}

auto SnippetsSettings::fromSettings(const QString &category, QSettings *s) -> void
{
  const QString &group = category + kGroupPostfix;
  s->beginGroup(group);
  m_lastUsedSnippetGroup = s->value(kLastUsedSnippetGroup, QString()).toString();
  s->endGroup();
}

auto SnippetsSettings::setLastUsedSnippetGroup(const QString &lastUsed) -> void
{
  m_lastUsedSnippetGroup = lastUsed;
}

auto SnippetsSettings::lastUsedSnippetGroup() const -> const QString&
{
  return m_lastUsedSnippetGroup;
}

auto SnippetsSettings::equals(const SnippetsSettings &snippetsSettings) const -> bool
{
  return m_lastUsedSnippetGroup == snippetsSettings.m_lastUsedSnippetGroup;
}
