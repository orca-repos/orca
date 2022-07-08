// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "icodestylepreferences.hpp"
#include "codestylepool.hpp"
#include "tabsettings.hpp"

#include <utils/settingsutils.hpp>

#include <QSettings>

using namespace TextEditor;

static const char currentPreferencesKey[] = "CurrentPreferences";

namespace TextEditor {
namespace Internal {

class ICodeStylePreferencesPrivate {
public:
  CodeStylePool *m_pool = nullptr;
  ICodeStylePreferences *m_currentDelegate = nullptr;
  TabSettings m_tabSettings;
  QByteArray m_id;
  QString m_displayName;
  bool m_readOnly = false;
  QString m_settingsSuffix;
};

}

ICodeStylePreferences::ICodeStylePreferences(QObject *parent) : QObject(parent), d(new Internal::ICodeStylePreferencesPrivate) {}

ICodeStylePreferences::~ICodeStylePreferences()
{
  delete d;
}

auto ICodeStylePreferences::id() const -> QByteArray
{
  return d->m_id;
}

auto ICodeStylePreferences::setId(const QByteArray &name) -> void
{
  d->m_id = name;
}

auto ICodeStylePreferences::displayName() const -> QString
{
  return d->m_displayName;
}

auto ICodeStylePreferences::setDisplayName(const QString &name) -> void
{
  d->m_displayName = name;
  emit displayNameChanged(name);
}

auto ICodeStylePreferences::isReadOnly() const -> bool
{
  return d->m_readOnly;
}

auto ICodeStylePreferences::setReadOnly(bool on) -> void
{
  d->m_readOnly = on;
}

auto ICodeStylePreferences::setTabSettings(const TabSettings &settings) -> void
{
  if (d->m_tabSettings == settings)
    return;

  d->m_tabSettings = settings;

  emit tabSettingsChanged(d->m_tabSettings);
  if (!currentDelegate()) emit currentTabSettingsChanged(d->m_tabSettings);
}

auto ICodeStylePreferences::tabSettings() const -> TabSettings
{
  return d->m_tabSettings;
}

auto ICodeStylePreferences::currentTabSettings() const -> TabSettings
{
  return currentPreferences()->tabSettings();
}

auto ICodeStylePreferences::currentValue() const -> QVariant
{
  return currentPreferences()->value();
}

auto ICodeStylePreferences::currentPreferences() const -> ICodeStylePreferences*
{
  auto prefs = (ICodeStylePreferences*)this;
  while (prefs->currentDelegate())
    prefs = prefs->currentDelegate();
  return prefs;
}

auto ICodeStylePreferences::delegatingPool() const -> CodeStylePool*
{
  return d->m_pool;
}

auto ICodeStylePreferences::setDelegatingPool(CodeStylePool *pool) -> void
{
  if (pool == d->m_pool)
    return;

  setCurrentDelegate(nullptr);
  if (d->m_pool) {
    disconnect(d->m_pool, &CodeStylePool::codeStyleRemoved, this, &ICodeStylePreferences::codeStyleRemoved);
  }
  d->m_pool = pool;
  if (d->m_pool) {
    connect(d->m_pool, &CodeStylePool::codeStyleRemoved, this, &ICodeStylePreferences::codeStyleRemoved);
  }
}

auto ICodeStylePreferences::currentDelegate() const -> ICodeStylePreferences*
{
  return d->m_currentDelegate;
}

auto ICodeStylePreferences::setCurrentDelegate(ICodeStylePreferences *delegate) -> void
{
  if (delegate && d->m_pool && !d->m_pool->codeStyles().contains(delegate)) {
    // warning
    return;
  }

  if (delegate == this || delegate && delegate->id() == id()) {
    // warning
    return;
  }

  if (d->m_currentDelegate == delegate)
    return; // nothing changes

  if (d->m_currentDelegate) {
    disconnect(d->m_currentDelegate, &ICodeStylePreferences::currentTabSettingsChanged, this, &ICodeStylePreferences::currentTabSettingsChanged);
    disconnect(d->m_currentDelegate, &ICodeStylePreferences::currentValueChanged, this, &ICodeStylePreferences::currentValueChanged);
    disconnect(d->m_currentDelegate, &ICodeStylePreferences::currentPreferencesChanged, this, &ICodeStylePreferences::currentPreferencesChanged);
  }
  d->m_currentDelegate = delegate;
  if (d->m_currentDelegate) {
    connect(d->m_currentDelegate, &ICodeStylePreferences::currentTabSettingsChanged, this, &ICodeStylePreferences::currentTabSettingsChanged);
    connect(d->m_currentDelegate, &ICodeStylePreferences::currentValueChanged, this, &ICodeStylePreferences::currentValueChanged);
    connect(d->m_currentDelegate, &ICodeStylePreferences::currentPreferencesChanged, this, &ICodeStylePreferences::currentPreferencesChanged);
  }
  emit currentDelegateChanged(d->m_currentDelegate);
  emit currentPreferencesChanged(currentPreferences());
  emit currentTabSettingsChanged(currentTabSettings());
  emit currentValueChanged(currentValue());
}

auto ICodeStylePreferences::currentDelegateId() const -> QByteArray
{
  if (currentDelegate())
    return currentDelegate()->id();
  return id(); // or 0?
}

auto ICodeStylePreferences::setCurrentDelegate(const QByteArray &id) -> void
{
  if (d->m_pool)
    setCurrentDelegate(d->m_pool->codeStyle(id));
}

auto ICodeStylePreferences::setSettingsSuffix(const QString &suffix) -> void
{
  d->m_settingsSuffix = suffix;
}

auto ICodeStylePreferences::toSettings(const QString &category, QSettings *s) const -> void
{
  Utils::toSettings(d->m_settingsSuffix, category, s, this);
}

auto ICodeStylePreferences::fromSettings(const QString &category, QSettings *s) -> void
{
  Utils::fromSettings(d->m_settingsSuffix, category, s, this);
}

auto ICodeStylePreferences::toMap() const -> QVariantMap
{
  QVariantMap map;
  if (!currentDelegate())
    return d->m_tabSettings.toMap();
  return {{currentPreferencesKey, currentDelegateId()}};
}

auto ICodeStylePreferences::fromMap(const QVariantMap &map) -> void
{
  d->m_tabSettings.fromMap(map);
  const auto delegateId = map.value(currentPreferencesKey).toByteArray();
  if (delegatingPool()) {
    const auto delegate = delegatingPool()->codeStyle(delegateId);
    if (!delegateId.isEmpty() && delegate)
      setCurrentDelegate(delegate);
  }
}

auto ICodeStylePreferences::codeStyleRemoved(ICodeStylePreferences *preferences) -> void
{
  if (currentDelegate() == preferences) {
    const auto pool = delegatingPool();
    const auto codeStyles = pool->codeStyles();
    const int idx = codeStyles.indexOf(preferences);
    ICodeStylePreferences *newCurrentPreferences = nullptr;
    auto i = idx + 1;
    // go forward
    while (i < codeStyles.count()) {
      const auto prefs = codeStyles.at(i);
      if (prefs->id() != id()) {
        newCurrentPreferences = prefs;
        break;
      }
      i++;
    }
    // go backward if still empty
    if (!newCurrentPreferences) {
      i = idx - 1;
      while (i >= 0) {
        const auto prefs = codeStyles.at(i);
        if (prefs->id() != id()) {
          newCurrentPreferences = prefs;
          break;
        }
        i--;
      }
    }
    setCurrentDelegate(newCurrentPreferences);
  }
}

} // TextEditor
