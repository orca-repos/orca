// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "extraencodingsettings.hpp"
#include "behaviorsettingswidget.hpp"

#include <utils/settingsutils.hpp>

#include <QLatin1String>
#include <QSettings>

// Keep this for compatibility reasons.
static constexpr char kGroupPostfix[] = "EditorManager";
static constexpr char kUtf8BomBehaviorKey[] = "Utf8BomBehavior";

using namespace TextEditor;

ExtraEncodingSettings::ExtraEncodingSettings() : m_utf8BomSetting(OnlyKeep) {}
ExtraEncodingSettings::~ExtraEncodingSettings() = default;

auto ExtraEncodingSettings::toSettings(const QString &category, QSettings *s) const -> void
{
  Q_UNUSED(category)

  Utils::toSettings(QLatin1String(kGroupPostfix), QString(), s, this);
}

auto ExtraEncodingSettings::fromSettings(const QString &category, QSettings *s) -> void
{
  Q_UNUSED(category)

  *this = ExtraEncodingSettings();
  Utils::fromSettings(QLatin1String(kGroupPostfix), QString(), s, this);
}

auto ExtraEncodingSettings::toMap() const -> QVariantMap
{
  return {{kUtf8BomBehaviorKey, m_utf8BomSetting}};
}

auto ExtraEncodingSettings::fromMap(const QVariantMap &map) -> void
{
  m_utf8BomSetting = (Utf8BomSetting)map.value(kUtf8BomBehaviorKey, m_utf8BomSetting).toInt();
}

auto ExtraEncodingSettings::equals(const ExtraEncodingSettings &s) const -> bool
{
  return m_utf8BomSetting == s.m_utf8BomSetting;
}

auto ExtraEncodingSettings::lineTerminationModeNames() -> QStringList
{
  return {BehaviorSettingsWidget::tr("Unix (LF)"), BehaviorSettingsWidget::tr("Windows (CRLF)")};
}
