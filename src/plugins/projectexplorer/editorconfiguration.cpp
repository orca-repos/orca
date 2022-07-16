// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "editorconfiguration.hpp"
#include "project.hpp"
#include "projectexplorer.hpp"
#include "session.hpp"

#include <utils/algorithm.hpp>

#include <core/core-interface.hpp>
#include <core/core-editor-manager.hpp>
#include <texteditor/texteditor.hpp>
#include <texteditor/textdocument.hpp>
#include <texteditor/texteditorsettings.hpp>
#include <texteditor/simplecodestylepreferences.hpp>
#include <texteditor/typingsettings.hpp>
#include <texteditor/storagesettings.hpp>
#include <texteditor/behaviorsettings.hpp>
#include <texteditor/extraencodingsettings.hpp>
#include <texteditor/tabsettings.hpp>
#include <texteditor/marginsettings.hpp>
#include <texteditor/icodestylepreferencesfactory.hpp>

#include <QLatin1String>
#include <QByteArray>
#include <QTextCodec>
#include <QDebug>

static const QLatin1String kPrefix("EditorConfiguration.");
static const QLatin1String kUseGlobal("EditorConfiguration.UseGlobal");
static const QLatin1String kCodec("EditorConfiguration.Codec");
static const QLatin1String kCodeStylePrefix("EditorConfiguration.CodeStyle.");
static const QLatin1String kCodeStyleCount("EditorConfiguration.CodeStyle.Count");

using namespace TextEditor;

namespace ProjectExplorer {

struct EditorConfigurationPrivate {
  EditorConfigurationPrivate() : m_typingSettings(TextEditorSettings::typingSettings()), m_storageSettings(TextEditorSettings::storageSettings()), m_behaviorSettings(TextEditorSettings::behaviorSettings()), m_extraEncodingSettings(TextEditorSettings::extraEncodingSettings()), m_textCodec(Orca::Plugin::Core::EditorManager::defaultTextCodec()) { }

  ICodeStylePreferences *m_defaultCodeStyle = nullptr;
  TypingSettings m_typingSettings;
  StorageSettings m_storageSettings;
  BehaviorSettings m_behaviorSettings;
  bool m_useGlobal = true;
  ExtraEncodingSettings m_extraEncodingSettings;
  MarginSettings m_marginSettings;
  QTextCodec *m_textCodec;

  QMap<Utils::Id, ICodeStylePreferences*> m_languageCodeStylePreferences;
  QList<BaseTextEditor*> m_editors;
};

EditorConfiguration::EditorConfiguration() : d(std::make_unique<EditorConfigurationPrivate>())
{
  const auto languageCodeStylePreferences = TextEditorSettings::codeStyles();
  for (auto itCodeStyle = languageCodeStylePreferences.cbegin(), end = languageCodeStylePreferences.cend(); itCodeStyle != end; ++itCodeStyle) {
    auto languageId = itCodeStyle.key();
    // global prefs for language
    const auto originalPreferences = itCodeStyle.value();
    const auto factory = TextEditorSettings::codeStyleFactory(languageId);
    // clone of global prefs for language - it will became project prefs for language
    auto preferences = factory->createCodeStyle();
    // project prefs can point to the global language pool, which contains also the global language prefs
    preferences->setDelegatingPool(TextEditorSettings::codeStylePool(languageId));
    preferences->setId(languageId.name() + "Project");
    preferences->setDisplayName(tr("Project %1", "Settings, %1 is a language (C++ or QML)").arg(factory->displayName()));
    // project prefs by default point to global prefs (which in turn can delegate to anything else or not)
    preferences->setCurrentDelegate(originalPreferences);
    d->m_languageCodeStylePreferences.insert(languageId, preferences);
  }

  // clone of global prefs (not language specific), for project scope
  d->m_defaultCodeStyle = new SimpleCodeStylePreferences(this);
  d->m_defaultCodeStyle->setDelegatingPool(TextEditorSettings::codeStylePool());
  d->m_defaultCodeStyle->setDisplayName(tr("Project", "Settings"));
  d->m_defaultCodeStyle->setId("Project");
  // if setCurrentDelegate is 0 values are read from *this prefs
  d->m_defaultCodeStyle->setCurrentDelegate(TextEditorSettings::codeStyle());

  connect(SessionManager::instance(), &SessionManager::aboutToRemoveProject, this, &EditorConfiguration::slotAboutToRemoveProject);
}

EditorConfiguration::~EditorConfiguration()
{
  qDeleteAll(d->m_languageCodeStylePreferences);
}

auto EditorConfiguration::useGlobalSettings() const -> bool
{
  return d->m_useGlobal;
}

auto EditorConfiguration::cloneGlobalSettings() -> void
{
  d->m_defaultCodeStyle->setTabSettings(TextEditorSettings::codeStyle()->tabSettings());
  setTypingSettings(TextEditorSettings::typingSettings());
  setStorageSettings(TextEditorSettings::storageSettings());
  setBehaviorSettings(TextEditorSettings::behaviorSettings());
  setExtraEncodingSettings(TextEditorSettings::extraEncodingSettings());
  setMarginSettings(TextEditorSettings::marginSettings());
  d->m_textCodec = Orca::Plugin::Core::EditorManager::defaultTextCodec();
}

auto EditorConfiguration::textCodec() const -> QTextCodec*
{
  return d->m_textCodec;
}

auto EditorConfiguration::typingSettings() const -> const TypingSettings&
{
  return d->m_typingSettings;
}

auto EditorConfiguration::storageSettings() const -> const StorageSettings&
{
  return d->m_storageSettings;
}

auto EditorConfiguration::behaviorSettings() const -> const BehaviorSettings&
{
  return d->m_behaviorSettings;
}

auto EditorConfiguration::extraEncodingSettings() const -> const ExtraEncodingSettings&
{
  return d->m_extraEncodingSettings;
}

auto EditorConfiguration::marginSettings() const -> const MarginSettings&
{
  return d->m_marginSettings;
}

auto EditorConfiguration::codeStyle() const -> ICodeStylePreferences*
{
  return d->m_defaultCodeStyle;
}

auto EditorConfiguration::codeStyle(Utils::Id languageId) const -> ICodeStylePreferences*
{
  return d->m_languageCodeStylePreferences.value(languageId, codeStyle());
}

auto EditorConfiguration::codeStyles() const -> QMap<Utils::Id, ICodeStylePreferences*>
{
  return d->m_languageCodeStylePreferences;
}

static auto toMapWithPrefix(QVariantMap *map, const QVariantMap &source) -> void
{
  for (auto it = source.constBegin(), end = source.constEnd(); it != end; ++it)
    map->insert(kPrefix + it.key(), it.value());
}

auto EditorConfiguration::toMap() const -> QVariantMap
{
  QVariantMap map = {{kUseGlobal, d->m_useGlobal}, {kCodec, d->m_textCodec->name()}, {kCodeStyleCount, d->m_languageCodeStylePreferences.count()}};

  auto i = 0;
  for (auto itCodeStyle = d->m_languageCodeStylePreferences.cbegin(), end = d->m_languageCodeStylePreferences.cend(); itCodeStyle != end; ++itCodeStyle) {
    const QVariantMap settingsIdMap = {{"language", itCodeStyle.key().toSetting()}, {"value", itCodeStyle.value()->toMap()}};
    map.insert(kCodeStylePrefix + QString::number(i), settingsIdMap);
    i++;
  }

  toMapWithPrefix(&map, d->m_defaultCodeStyle->tabSettings().toMap());
  toMapWithPrefix(&map, d->m_typingSettings.toMap());
  toMapWithPrefix(&map, d->m_storageSettings.toMap());
  toMapWithPrefix(&map, d->m_behaviorSettings.toMap());
  toMapWithPrefix(&map, d->m_extraEncodingSettings.toMap());
  toMapWithPrefix(&map, d->m_marginSettings.toMap());

  return map;
}

auto EditorConfiguration::fromMap(const QVariantMap &map) -> void
{
  const auto &codecName = map.value(kCodec, d->m_textCodec->name()).toByteArray();
  d->m_textCodec = QTextCodec::codecForName(codecName);
  if (!d->m_textCodec)
    d->m_textCodec = Orca::Plugin::Core::EditorManager::defaultTextCodec();

  const auto codeStyleCount = map.value(kCodeStyleCount, 0).toInt();
  for (auto i = 0; i < codeStyleCount; ++i) {
    auto settingsIdMap = map.value(kCodeStylePrefix + QString::number(i)).toMap();
    if (settingsIdMap.isEmpty()) {
      qWarning() << "No data for code style settings list" << i << "found!";
      continue;
    }
    auto languageId = Utils::Id::fromSetting(settingsIdMap.value(QLatin1String("language")));
    auto value = settingsIdMap.value(QLatin1String("value")).toMap();
    const auto preferences = d->m_languageCodeStylePreferences.value(languageId);
    if (preferences)
      preferences->fromMap(value);
  }

  QVariantMap submap;
  for (auto it = map.constBegin(), end = map.constEnd(); it != end; ++it) {
    if (it.key().startsWith(kPrefix))
      submap.insert(it.key().mid(kPrefix.size()), it.value());
  }
  d->m_defaultCodeStyle->fromMap(submap);
  d->m_typingSettings.fromMap(submap);
  d->m_storageSettings.fromMap(submap);
  d->m_behaviorSettings.fromMap(submap);
  d->m_extraEncodingSettings.fromMap(submap);
  d->m_marginSettings.fromMap(submap);
  setUseGlobalSettings(map.value(kUseGlobal, d->m_useGlobal).toBool());
}

auto EditorConfiguration::configureEditor(BaseTextEditor *textEditor) const -> void
{
  const auto widget = textEditor->editorWidget();
  if (widget)
    widget->setCodeStyle(codeStyle(widget->languageSettingsId()));
  if (!d->m_useGlobal) {
    textEditor->textDocument()->setCodec(d->m_textCodec);
    if (widget)
      switchSettings(widget);
  }
  d->m_editors.append(textEditor);
  connect(textEditor, &BaseTextEditor::destroyed, this, [this, textEditor]() {
    d->m_editors.removeOne(textEditor);
  });
}

auto EditorConfiguration::deconfigureEditor(BaseTextEditor *textEditor) const -> void
{
  const auto widget = textEditor->editorWidget();
  if (widget)
    widget->setCodeStyle(TextEditorSettings::codeStyle(widget->languageSettingsId()));

  d->m_editors.removeOne(textEditor);

  // TODO: what about text codec and switching settings?
}

auto EditorConfiguration::setUseGlobalSettings(bool use) -> void
{
  d->m_useGlobal = use;
  d->m_defaultCodeStyle->setCurrentDelegate(use ? TextEditorSettings::codeStyle() : nullptr);
  foreach(Orca::Plugin::Core::IEditor *editor, Orca::Plugin::Core::DocumentModel::editorsForOpenedDocuments()) {
    if (const auto widget = TextEditorWidget::fromEditor(editor)) {
      const auto project = SessionManager::projectForFile(editor->document()->filePath());
      if (project && project->editorConfiguration() == this)
        switchSettings(widget);
    }
  }
}

template <typename New, typename Old>
static auto switchSettings_helper(const New *newSender, const Old *oldSender, TextEditorWidget *widget) -> void
{
  QObject::disconnect(oldSender, &Old::marginSettingsChanged, widget, &TextEditorWidget::setMarginSettings);
  QObject::disconnect(oldSender, &Old::typingSettingsChanged, widget, &TextEditorWidget::setTypingSettings);
  QObject::disconnect(oldSender, &Old::storageSettingsChanged, widget, &TextEditorWidget::setStorageSettings);
  QObject::disconnect(oldSender, &Old::behaviorSettingsChanged, widget, &TextEditorWidget::setBehaviorSettings);
  QObject::disconnect(oldSender, &Old::extraEncodingSettingsChanged, widget, &TextEditorWidget::setExtraEncodingSettings);

  QObject::connect(newSender, &New::marginSettingsChanged, widget, &TextEditorWidget::setMarginSettings);
  QObject::connect(newSender, &New::typingSettingsChanged, widget, &TextEditorWidget::setTypingSettings);
  QObject::connect(newSender, &New::storageSettingsChanged, widget, &TextEditorWidget::setStorageSettings);
  QObject::connect(newSender, &New::behaviorSettingsChanged, widget, &TextEditorWidget::setBehaviorSettings);
  QObject::connect(newSender, &New::extraEncodingSettingsChanged, widget, &TextEditorWidget::setExtraEncodingSettings);
}

auto EditorConfiguration::switchSettings(TextEditorWidget *widget) const -> void
{
  if (d->m_useGlobal) {
    widget->setMarginSettings(TextEditorSettings::marginSettings());
    widget->setTypingSettings(TextEditorSettings::typingSettings());
    widget->setStorageSettings(TextEditorSettings::storageSettings());
    widget->setBehaviorSettings(TextEditorSettings::behaviorSettings());
    widget->setExtraEncodingSettings(TextEditorSettings::extraEncodingSettings());
    switchSettings_helper(TextEditorSettings::instance(), this, widget);
  } else {
    widget->setMarginSettings(marginSettings());
    widget->setTypingSettings(typingSettings());
    widget->setStorageSettings(storageSettings());
    widget->setBehaviorSettings(behaviorSettings());
    widget->setExtraEncodingSettings(extraEncodingSettings());
    switchSettings_helper(this, TextEditorSettings::instance(), widget);
  }
}

auto EditorConfiguration::setTypingSettings(const TypingSettings &settings) -> void
{
  d->m_typingSettings = settings;
  emit typingSettingsChanged(d->m_typingSettings);
}

auto EditorConfiguration::setStorageSettings(const StorageSettings &settings) -> void
{
  d->m_storageSettings = settings;
  emit storageSettingsChanged(d->m_storageSettings);
}

auto EditorConfiguration::setBehaviorSettings(const BehaviorSettings &settings) -> void
{
  d->m_behaviorSettings = settings;
  emit behaviorSettingsChanged(d->m_behaviorSettings);
}

auto EditorConfiguration::setExtraEncodingSettings(const ExtraEncodingSettings &settings) -> void
{
  d->m_extraEncodingSettings = settings;
  emit extraEncodingSettingsChanged(d->m_extraEncodingSettings);
}

auto EditorConfiguration::setMarginSettings(const MarginSettings &settings) -> void
{
  if (d->m_marginSettings != settings) {
    d->m_marginSettings = settings;
    emit marginSettingsChanged(d->m_marginSettings);
  }
}

auto EditorConfiguration::setTextCodec(QTextCodec *textCodec) -> void
{
  d->m_textCodec = textCodec;
}

auto EditorConfiguration::setShowWrapColumn(bool onoff) -> void
{
  if (d->m_marginSettings.m_showMargin != onoff) {
    d->m_marginSettings.m_showMargin = onoff;
    emit marginSettingsChanged(d->m_marginSettings);
  }
}

auto EditorConfiguration::setUseIndenter(bool onoff) -> void
{
  if (d->m_marginSettings.m_useIndenter != onoff) {
    d->m_marginSettings.m_useIndenter = onoff;
    emit marginSettingsChanged(d->m_marginSettings);
  }
}

auto EditorConfiguration::setWrapColumn(int column) -> void
{
  if (d->m_marginSettings.m_marginColumn != column) {
    d->m_marginSettings.m_marginColumn = column;
    emit marginSettingsChanged(d->m_marginSettings);
  }
}

auto EditorConfiguration::slotAboutToRemoveProject(Project *project) -> void
{
  if (project->editorConfiguration() != this)
    return;

  foreach(BaseTextEditor *editor, d->m_editors)
    deconfigureEditor(editor);
}

auto actualTabSettings(const QString &fileName, const TextDocument *baseTextdocument) -> TabSettings
{
  if (baseTextdocument)
    return baseTextdocument->tabSettings();
  if (const auto project = SessionManager::projectForFile(Utils::FilePath::fromString(fileName)))
    return project->editorConfiguration()->codeStyle()->tabSettings();
  return TextEditorSettings::codeStyle()->tabSettings();
}

} // ProjectExplorer
