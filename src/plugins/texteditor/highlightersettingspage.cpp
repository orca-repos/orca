// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "highlightersettingspage.hpp"
#include "highlightersettings.hpp"
#include "highlighter.hpp"
#include "ui_highlightersettingspage.h"

#include <core/icore.hpp>

#include <QDir>
#include <QPointer>

using namespace TextEditor::Internal;
using namespace Utils;

namespace TextEditor {

class HighlighterSettingsPage::HighlighterSettingsPagePrivate {
  Q_DECLARE_TR_FUNCTIONS(TextEditor::Internal::HighlighterSettingsPage)

public:
  HighlighterSettingsPagePrivate() = default;

  auto ensureInitialized() -> void;
  auto migrateGenericHighlighterFiles() -> void;

  bool m_initialized = false;
  const QString m_settingsPrefix{"Text"};
  HighlighterSettings m_settings;
  QPointer<QWidget> m_widget;
  Ui::HighlighterSettingsPage *m_page = nullptr;
};

auto HighlighterSettingsPage::HighlighterSettingsPagePrivate::migrateGenericHighlighterFiles() -> void
{
  const QDir userDefinitionPath(m_settings.definitionFilesPath().toString());
  if (userDefinitionPath.mkdir("syntax")) {
    const auto link = HostOsInfo::isAnyUnixHost() ? static_cast<bool(*)(const QString &, const QString &)>(&QFile::link) : static_cast<bool(*)(const QString &, const QString &)>(&QFile::copy);
    for (const auto &file : userDefinitionPath.entryInfoList({"*.xml"}, QDir::Files))
      link(file.filePath(), file.absolutePath() + "/syntax/" + file.fileName());
  }
}

auto HighlighterSettingsPage::HighlighterSettingsPagePrivate::ensureInitialized() -> void
{
  if (m_initialized)
    return;
  m_initialized = true;
  m_settings.fromSettings(m_settingsPrefix, Core::ICore::settings());
  migrateGenericHighlighterFiles();
}

HighlighterSettingsPage::HighlighterSettingsPage() : d(new HighlighterSettingsPagePrivate)
{
  setId(Constants::TEXT_EDITOR_HIGHLIGHTER_SETTINGS);
  setDisplayName(HighlighterSettingsPagePrivate::tr("Generic Highlighter"));
  setCategory(Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
  setDisplayCategory(QCoreApplication::translate("TextEditor", "Text Editor"));
  setCategoryIconPath(Constants::TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH);
}

HighlighterSettingsPage::~HighlighterSettingsPage()
{
  delete d;
}

auto HighlighterSettingsPage::widget() -> QWidget*
{
  if (!d->m_widget) {
    d->m_widget = new QWidget;
    d->m_page = new Ui::HighlighterSettingsPage;
    d->m_page->setupUi(d->m_widget);
    d->m_page->definitionFilesPath->setExpectedKind(Utils::PathChooser::ExistingDirectory);
    d->m_page->definitionFilesPath->setHistoryCompleter(QLatin1String("TextEditor.Highlighter.History"));
    connect(d->m_page->downloadDefinitions, &QPushButton::pressed, [label = QPointer<QLabel>(d->m_page->updateStatus)]() {
      Highlighter::downloadDefinitions([label]() {
        if (label)
          label->setText(HighlighterSettingsPagePrivate::tr("Download finished"));
      });
    });
    connect(d->m_page->reloadDefinitions, &QPushButton::pressed, []() {
      Highlighter::reload();
    });
    connect(d->m_page->resetCache, &QPushButton::clicked, []() {
      Highlighter::clearDefinitionForDocumentCache();
    });

    settingsToUI();
  }
  return d->m_widget;
}

auto HighlighterSettingsPage::apply() -> void
{
  if (!d->m_page) // page was not shown
    return;
  if (settingsChanged())
    settingsFromUI();
}

auto HighlighterSettingsPage::finish() -> void
{
  delete d->m_widget;
  if (!d->m_page) // page was not shown
    return;
  delete d->m_page;
  d->m_page = nullptr;
}

auto HighlighterSettingsPage::highlighterSettings() const -> const HighlighterSettings&
{
  d->ensureInitialized();
  return d->m_settings;
}

auto HighlighterSettingsPage::settingsFromUI() -> void
{
  d->ensureInitialized();
  d->m_settings.setDefinitionFilesPath(d->m_page->definitionFilesPath->filePath());
  d->m_settings.setIgnoredFilesPatterns(d->m_page->ignoreEdit->text());
  d->m_settings.toSettings(d->m_settingsPrefix, Core::ICore::settings());
}

auto HighlighterSettingsPage::settingsToUI() -> void
{
  d->ensureInitialized();
  d->m_page->definitionFilesPath->setFilePath(d->m_settings.definitionFilesPath());
  d->m_page->ignoreEdit->setText(d->m_settings.ignoredFilesPatterns());
}

auto HighlighterSettingsPage::settingsChanged() const -> bool
{
  d->ensureInitialized();
  return d->m_settings.definitionFilesPath() != d->m_page->definitionFilesPath->filePath() || d->m_settings.ignoredFilesPatterns() != d->m_page->ignoreEdit->text();
}

} // TextEditor
