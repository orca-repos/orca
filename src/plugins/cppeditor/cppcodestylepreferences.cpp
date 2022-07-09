// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppcodestylepreferences.hpp"

namespace CppEditor {

CppCodeStylePreferences::CppCodeStylePreferences(QObject *parent) : ICodeStylePreferences(parent)
{
  setSettingsSuffix("CodeStyleSettings");
  connect(this, &CppCodeStylePreferences::currentValueChanged, this, &CppCodeStylePreferences::slotCurrentValueChanged);
}

auto CppCodeStylePreferences::value() const -> QVariant
{
  QVariant v;
  v.setValue(codeStyleSettings());
  return v;
}

auto CppCodeStylePreferences::setValue(const QVariant &data) -> void
{
  if (!data.canConvert<CppCodeStyleSettings>())
    return;

  setCodeStyleSettings(data.value<CppCodeStyleSettings>());
}

auto CppCodeStylePreferences::codeStyleSettings() const -> CppCodeStyleSettings
{
  return m_data;
}

auto CppCodeStylePreferences::setCodeStyleSettings(const CppCodeStyleSettings &data) -> void
{
  if (m_data == data)
    return;

  m_data = data;

  QVariant v;
  v.setValue(data);
  emit valueChanged(v);
  emit codeStyleSettingsChanged(m_data);
  if (!currentDelegate()) emit currentValueChanged(v);
}

auto CppCodeStylePreferences::currentCodeStyleSettings() const -> CppCodeStyleSettings
{
  auto v = currentValue();
  if (!v.canConvert<CppCodeStyleSettings>()) {
    // warning
    return {};
  }
  return v.value<CppCodeStyleSettings>();
}

auto CppCodeStylePreferences::slotCurrentValueChanged(const QVariant &value) -> void
{
  if (!value.canConvert<CppCodeStyleSettings>())
    return;

  emit currentCodeStyleSettingsChanged(value.value<CppCodeStyleSettings>());
}

auto CppCodeStylePreferences::toMap() const -> QVariantMap
{
  auto map = ICodeStylePreferences::toMap();
  if (!currentDelegate()) {
    const auto dataMap = m_data.toMap();
    for (auto it = dataMap.begin(), end = dataMap.end(); it != end; ++it)
      map.insert(it.key(), it.value());
  }
  return map;
}

auto CppCodeStylePreferences::fromMap(const QVariantMap &map) -> void
{
  ICodeStylePreferences::fromMap(map);
  if (!currentDelegate())
    m_data.fromMap(map);
}

} // namespace CppEditor
