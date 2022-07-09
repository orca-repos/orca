// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "behaviorsettingspage.hpp"

#include "behaviorsettings.hpp"
#include "typingsettings.hpp"
#include "storagesettings.hpp"
#include "tabsettings.hpp"
#include "extraencodingsettings.hpp"
#include "ui_behaviorsettingspage.h"
#include "simplecodestylepreferences.hpp"
#include "texteditorconstants.hpp"
#include "codestylepool.hpp"
#include "texteditorsettings.hpp"

#include <core/icore.hpp>
#include <core/coreconstants.hpp>
#include <core/editormanager/editormanager.hpp>

#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>

// for opening the respective coding style preferences
#include <cppeditor/cppeditorconstants.hpp>
// #include <qmljseditor/qmljseditorconstants.hpp>
// #include <qmljstools/qmljstoolsconstants.hpp>

#include <QPointer>
#include <QSettings>

namespace TextEditor {

struct BehaviorSettingsPage::BehaviorSettingsPagePrivate : public QObject {
  BehaviorSettingsPagePrivate();

  const QString m_settingsPrefix{"text"};
  QPointer<QWidget> m_widget;
  Internal::Ui::BehaviorSettingsPage *m_page = nullptr;

  CodeStylePool *m_defaultCodeStylePool = nullptr;
  SimpleCodeStylePreferences *m_codeStyle = nullptr;
  SimpleCodeStylePreferences *m_pageCodeStyle = nullptr;
  TypingSettings m_typingSettings;
  StorageSettings m_storageSettings;
  BehaviorSettings m_behaviorSettings;
  ExtraEncodingSettings m_extraEncodingSettings;
};

BehaviorSettingsPage::BehaviorSettingsPagePrivate::BehaviorSettingsPagePrivate()
{
  // global tab preferences for all other languages
  m_codeStyle = new SimpleCodeStylePreferences(this);
  m_codeStyle->setDisplayName(tr("Global", "Settings"));
  m_codeStyle->setId(Constants::GLOBAL_SETTINGS_ID);

  // default pool for all other languages
  m_defaultCodeStylePool = new CodeStylePool(nullptr, this); // Any language
  m_defaultCodeStylePool->addCodeStyle(m_codeStyle);

  QSettings *const s = Core::ICore::settings();
  m_codeStyle->fromSettings(m_settingsPrefix, s);
  m_typingSettings.fromSettings(m_settingsPrefix, s);
  m_storageSettings.fromSettings(m_settingsPrefix, s);
  m_behaviorSettings.fromSettings(m_settingsPrefix, s);
  m_extraEncodingSettings.fromSettings(m_settingsPrefix, s);
}

BehaviorSettingsPage::BehaviorSettingsPage() : d(new BehaviorSettingsPagePrivate)
{
  // Add the GUI used to configure the tab, storage and interaction settings
  setId(Constants::TEXT_EDITOR_BEHAVIOR_SETTINGS);
  setDisplayName(tr("Behavior"));

  setCategory(Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("TextEditor", "Text Editor"));
  setCategoryIconPath(Constants::TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH);
}

BehaviorSettingsPage::~BehaviorSettingsPage()
{
  delete d;
}

auto BehaviorSettingsPage::widget() -> QWidget*
{
  if (!d->m_widget) {
    d->m_widget = new QWidget;
    d->m_page = new Internal::Ui::BehaviorSettingsPage;
    d->m_page->setupUi(d->m_widget);
    if (Utils::HostOsInfo::isMacHost())
      d->m_page->gridLayout->setContentsMargins(-1, 0, -1, 0); // don't ask.
    d->m_pageCodeStyle = new SimpleCodeStylePreferences(d->m_widget);
    d->m_pageCodeStyle->setDelegatingPool(d->m_codeStyle->delegatingPool());
    d->m_pageCodeStyle->setTabSettings(d->m_codeStyle->tabSettings());
    d->m_pageCodeStyle->setCurrentDelegate(d->m_codeStyle->currentDelegate());
    d->m_page->behaviorWidget->setCodeStyle(d->m_pageCodeStyle);

    TabSettingsWidget *tabSettingsWidget = d->m_page->behaviorWidget->tabSettingsWidget();
    tabSettingsWidget->setCodingStyleWarningVisible(true);
    connect(tabSettingsWidget, &TabSettingsWidget::codingStyleLinkClicked, this, &BehaviorSettingsPage::openCodingStylePreferences);

    settingsToUI();
  }
  return d->m_widget;
}

auto BehaviorSettingsPage::apply() -> void
{
  if (!d->m_page) // page was never shown
    return;

  TypingSettings newTypingSettings;
  StorageSettings newStorageSettings;
  BehaviorSettings newBehaviorSettings;
  ExtraEncodingSettings newExtraEncodingSettings;

  settingsFromUI(&newTypingSettings, &newStorageSettings, &newBehaviorSettings, &newExtraEncodingSettings);

  QSettings *s = Core::ICore::settings();
  QTC_ASSERT(s, return);

  if (d->m_codeStyle->tabSettings() != d->m_pageCodeStyle->tabSettings()) {
    d->m_codeStyle->setTabSettings(d->m_pageCodeStyle->tabSettings());
    d->m_codeStyle->toSettings(d->m_settingsPrefix, s);
  }

  if (d->m_codeStyle->currentDelegate() != d->m_pageCodeStyle->currentDelegate()) {
    d->m_codeStyle->setCurrentDelegate(d->m_pageCodeStyle->currentDelegate());
    d->m_codeStyle->toSettings(d->m_settingsPrefix, s);
  }

  if (newTypingSettings != d->m_typingSettings) {
    d->m_typingSettings = newTypingSettings;
    d->m_typingSettings.toSettings(d->m_settingsPrefix, s);

    emit TextEditorSettings::instance()->typingSettingsChanged(newTypingSettings);
  }

  if (newStorageSettings != d->m_storageSettings) {
    d->m_storageSettings = newStorageSettings;
    d->m_storageSettings.toSettings(d->m_settingsPrefix, s);

    emit TextEditorSettings::instance()->storageSettingsChanged(newStorageSettings);
  }

  if (newBehaviorSettings != d->m_behaviorSettings) {
    d->m_behaviorSettings = newBehaviorSettings;
    d->m_behaviorSettings.toSettings(d->m_settingsPrefix, s);

    emit TextEditorSettings::instance()->behaviorSettingsChanged(newBehaviorSettings);
  }

  if (newExtraEncodingSettings != d->m_extraEncodingSettings) {
    d->m_extraEncodingSettings = newExtraEncodingSettings;
    d->m_extraEncodingSettings.toSettings(d->m_settingsPrefix, s);

    emit TextEditorSettings::instance()->extraEncodingSettingsChanged(newExtraEncodingSettings);
  }

  s->setValue(QLatin1String(Core::Constants::SETTINGS_DEFAULTTEXTENCODING), d->m_page->behaviorWidget->assignedCodecName());
  s->setValue(QLatin1String(Core::Constants::SETTINGS_DEFAULT_LINE_TERMINATOR), d->m_page->behaviorWidget->assignedLineEnding());
}

auto BehaviorSettingsPage::settingsFromUI(TypingSettings *typingSettings, StorageSettings *storageSettings, BehaviorSettings *behaviorSettings, ExtraEncodingSettings *extraEncodingSettings) const -> void
{
  d->m_page->behaviorWidget->assignedTypingSettings(typingSettings);
  d->m_page->behaviorWidget->assignedStorageSettings(storageSettings);
  d->m_page->behaviorWidget->assignedBehaviorSettings(behaviorSettings);
  d->m_page->behaviorWidget->assignedExtraEncodingSettings(extraEncodingSettings);
}

auto BehaviorSettingsPage::settingsToUI() -> void
{
  d->m_page->behaviorWidget->setAssignedTypingSettings(d->m_typingSettings);
  d->m_page->behaviorWidget->setAssignedStorageSettings(d->m_storageSettings);
  d->m_page->behaviorWidget->setAssignedBehaviorSettings(d->m_behaviorSettings);
  d->m_page->behaviorWidget->setAssignedExtraEncodingSettings(d->m_extraEncodingSettings);
  d->m_page->behaviorWidget->setAssignedCodec(Core::EditorManager::defaultTextCodec());
  d->m_page->behaviorWidget->setAssignedLineEnding(Core::EditorManager::defaultLineEnding());
}

auto BehaviorSettingsPage::finish() -> void
{
  delete d->m_widget;
  if (!d->m_page) // page was never shown
    return;
  delete d->m_page;
  d->m_page = nullptr;
}

auto BehaviorSettingsPage::codeStyle() const -> ICodeStylePreferences*
{
  return d->m_codeStyle;
}

auto BehaviorSettingsPage::codeStylePool() const -> CodeStylePool*
{
  return d->m_defaultCodeStylePool;
}

auto BehaviorSettingsPage::typingSettings() const -> const TypingSettings&
{
  return d->m_typingSettings;
}

auto BehaviorSettingsPage::storageSettings() const -> const StorageSettings&
{
  return d->m_storageSettings;
}

auto BehaviorSettingsPage::behaviorSettings() const -> const BehaviorSettings&
{
  return d->m_behaviorSettings;
}

auto BehaviorSettingsPage::extraEncodingSettings() const -> const ExtraEncodingSettings&
{
  return d->m_extraEncodingSettings;
}

auto BehaviorSettingsPage::openCodingStylePreferences(TabSettingsWidget::CodingStyleLink link) -> void
{
  switch (link) {
  case TabSettingsWidget::CppLink:
    Core::ICore::showOptionsDialog(CppEditor::Constants::CPP_CODE_STYLE_SETTINGS_ID);
    break;
//case TabSettingsWidget::QtQuickLink:
//  Core::ICore::showOptionsDialog(QmlJSTools::Constants::QML_JS_CODE_STYLE_SETTINGS_ID);
//  break;
  }
}

} // namespace TextEditor
