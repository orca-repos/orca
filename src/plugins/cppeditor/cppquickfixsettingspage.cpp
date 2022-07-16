// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppquickfixsettingspage.hpp"

#include "cppeditorconstants.hpp"
#include "cppquickfixsettings.hpp"
#include "cppquickfixsettingswidget.hpp"

#include <QCoreApplication>
#include <QtDebug>

using namespace CppEditor::Internal;

CppQuickFixSettingsPage::CppQuickFixSettingsPage()
{
  setId(Constants::QUICK_FIX_SETTINGS_ID);
  setDisplayName(QCoreApplication::translate("CppEditor", Constants::QUICK_FIX_SETTINGS_DISPLAY_NAME));
  setCategory(Constants::CPP_SETTINGS_CATEGORY);
}

auto CppQuickFixSettingsPage::widget() -> QWidget*
{
  if (!m_widget) {
    m_widget = new CppQuickFixSettingsWidget;
    m_widget->loadSettings(CppQuickFixSettings::instance());
  }
  return m_widget;
}

auto CppQuickFixSettingsPage::apply() -> void
{
  const auto s = CppQuickFixSettings::instance();
  m_widget->saveSettings(s);
  s->saveAsGlobalSettings();
}

auto CppEditor::Internal::CppQuickFixSettingsPage::finish() -> void
{
  delete m_widget;
}
