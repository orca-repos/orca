// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "displaysettingspage.hpp"

#include "displaysettings.hpp"
#include "marginsettings.hpp"
#include "texteditorconstants.hpp"
#include "texteditorsettings.hpp"
#include "ui_displaysettingspage.h"

#include <core/core-interface.hpp>

namespace TextEditor {

class DisplaySettingsPagePrivate
{
public:
    DisplaySettingsPagePrivate();

    DisplaySettings m_displaySettings;
    MarginSettings m_marginSettings;
    QString m_settingsPrefix;
};

DisplaySettingsPagePrivate::DisplaySettingsPagePrivate()
{
    m_settingsPrefix = QLatin1String("text");
    m_displaySettings.fromSettings(m_settingsPrefix, Orca::Plugin::Core::ICore::settings());
    m_marginSettings.fromSettings(m_settingsPrefix, Orca::Plugin::Core::ICore::settings());
}

class DisplaySettingsWidget final : public Orca::Plugin::Core::IOptionsPageWidget
{
    Q_DECLARE_TR_FUNCTIONS(TextEditor::DisplaySettingsPage)

public:
    DisplaySettingsWidget(DisplaySettingsPagePrivate *data)
        : m_data(data)
    {
        m_ui.setupUi(this);
        settingsToUI();
    }

    auto apply() -> void final;

    auto settingsFromUI(DisplaySettings &displaySettings, MarginSettings &marginSettings) const -> void;
    auto settingsToUI() -> void;
    auto setDisplaySettings(const DisplaySettings &, const MarginSettings &newMarginSettings) -> void;

    DisplaySettingsPagePrivate *m_data = nullptr;
    Internal::Ui::DisplaySettingsPage m_ui;
};

auto DisplaySettingsWidget::apply() -> void
{
    DisplaySettings newDisplaySettings;
    MarginSettings newMarginSettings;

    settingsFromUI(newDisplaySettings, newMarginSettings);
    setDisplaySettings(newDisplaySettings, newMarginSettings);
}

auto DisplaySettingsWidget::settingsFromUI(DisplaySettings &displaySettings, MarginSettings &marginSettings) const -> void
{
    displaySettings.m_displayLineNumbers = m_ui.displayLineNumbers->isChecked();
    displaySettings.m_textWrapping = m_ui.enableTextWrapping->isChecked();
    marginSettings.m_showMargin = m_ui.showWrapColumn->isChecked();
    marginSettings.m_useIndenter = m_ui.useIndenter->isChecked();
    marginSettings.m_marginColumn = m_ui.wrapColumn->value();
    displaySettings.m_visualizeWhitespace = m_ui.visualizeWhitespace->isChecked();
    displaySettings.m_displayFoldingMarkers = m_ui.displayFoldingMarkers->isChecked();
    displaySettings.m_highlightCurrentLine = m_ui.highlightCurrentLine->isChecked();
    displaySettings.m_highlightBlocks = m_ui.highlightBlocks->isChecked();
    displaySettings.m_animateMatchingParentheses = m_ui.animateMatchingParentheses->isChecked();
    displaySettings.m_highlightMatchingParentheses = m_ui.highlightMatchingParentheses->isChecked();
    displaySettings.m_markTextChanges = m_ui.markTextChanges->isChecked();
    displaySettings.m_autoFoldFirstComment = m_ui.autoFoldFirstComment->isChecked();
    displaySettings.m_centerCursorOnScroll = m_ui.centerOnScroll->isChecked();
    displaySettings.m_openLinksInNextSplit = m_ui.openLinksInNextSplit->isChecked();
    displaySettings.m_displayFileEncoding = m_ui.displayFileEncoding->isChecked();
    displaySettings.m_scrollBarHighlights = m_ui.scrollBarHighlights->isChecked();
    displaySettings.m_animateNavigationWithinFile = m_ui.animateNavigationWithinFile->isChecked();
    displaySettings.m_displayAnnotations = m_ui.displayAnnotations->isChecked();
    if (m_ui.leftAligned->isChecked())
        displaySettings.m_annotationAlignment = AnnotationAlignment::NextToContent;
    else if (m_ui.atMargin->isChecked())
        displaySettings.m_annotationAlignment = AnnotationAlignment::NextToMargin;
    else if (m_ui.rightAligned->isChecked())
        displaySettings.m_annotationAlignment = AnnotationAlignment::RightSide;
    else if (m_ui.betweenLines->isChecked())
        displaySettings.m_annotationAlignment = AnnotationAlignment::BetweenLines;
}

auto DisplaySettingsWidget::settingsToUI() -> void
{
    const auto &displaySettings = m_data->m_displaySettings;
    const auto &marginSettings = m_data->m_marginSettings;
    m_ui.displayLineNumbers->setChecked(displaySettings.m_displayLineNumbers);
    m_ui.enableTextWrapping->setChecked(displaySettings.m_textWrapping);
    m_ui.showWrapColumn->setChecked(marginSettings.m_showMargin);
    m_ui.useIndenter->setChecked(marginSettings.m_useIndenter);
    m_ui.wrapColumn->setValue(marginSettings.m_marginColumn);
    m_ui.visualizeWhitespace->setChecked(displaySettings.m_visualizeWhitespace);
    m_ui.displayFoldingMarkers->setChecked(displaySettings.m_displayFoldingMarkers);
    m_ui.highlightCurrentLine->setChecked(displaySettings.m_highlightCurrentLine);
    m_ui.highlightBlocks->setChecked(displaySettings.m_highlightBlocks);
    m_ui.animateMatchingParentheses->setChecked(displaySettings.m_animateMatchingParentheses);
    m_ui.highlightMatchingParentheses->setChecked(displaySettings.m_highlightMatchingParentheses);
    m_ui.markTextChanges->setChecked(displaySettings.m_markTextChanges);
    m_ui.autoFoldFirstComment->setChecked(displaySettings.m_autoFoldFirstComment);
    m_ui.centerOnScroll->setChecked(displaySettings.m_centerCursorOnScroll);
    m_ui.openLinksInNextSplit->setChecked(displaySettings.m_openLinksInNextSplit);
    m_ui.displayFileEncoding->setChecked(displaySettings.m_displayFileEncoding);
    m_ui.scrollBarHighlights->setChecked(displaySettings.m_scrollBarHighlights);
    m_ui.animateNavigationWithinFile->setChecked(displaySettings.m_animateNavigationWithinFile);
    m_ui.displayAnnotations->setChecked(displaySettings.m_displayAnnotations);
    switch (displaySettings.m_annotationAlignment) {
    case AnnotationAlignment::NextToContent: m_ui.leftAligned->setChecked(true); break;
    case AnnotationAlignment::NextToMargin: m_ui.atMargin->setChecked(true); break;
    case AnnotationAlignment::RightSide: m_ui.rightAligned->setChecked(true); break;
    case AnnotationAlignment::BetweenLines: m_ui.betweenLines->setChecked(true); break;
    }
}

auto DisplaySettingsPage::displaySettings() const -> const DisplaySettings&
{
    return d->m_displaySettings;
}

auto DisplaySettingsPage::marginSettings() const -> const MarginSettings&
{
    return d->m_marginSettings;
}

auto DisplaySettingsWidget::setDisplaySettings(const DisplaySettings &newDisplaySettings, const MarginSettings &newMarginSettings) -> void
{
    if (newDisplaySettings != m_data->m_displaySettings) {
        m_data->m_displaySettings = newDisplaySettings;
        m_data->m_displaySettings.toSettings(m_data->m_settingsPrefix, Orca::Plugin::Core::ICore::settings());

        emit TextEditorSettings::instance()->displaySettingsChanged(newDisplaySettings);
    }

    if (newMarginSettings != m_data->m_marginSettings) {
        m_data->m_marginSettings = newMarginSettings;
        m_data->m_marginSettings.toSettings(m_data->m_settingsPrefix, Orca::Plugin::Core::ICore::settings());

        emit TextEditorSettings::instance()->marginSettingsChanged(newMarginSettings);
    }
}

DisplaySettingsPage::DisplaySettingsPage()
  : d(new DisplaySettingsPagePrivate)
{
    setId(Constants::TEXT_EDITOR_DISPLAY_SETTINGS);
    setDisplayName(DisplaySettingsWidget::tr("Display"));
    setCategory(Constants::TEXT_EDITOR_SETTINGS_CATEGORY);
    setDisplayCategory(QCoreApplication::translate("TextEditor", "Text Editor"));
    setCategoryIconPath(Constants::TEXT_EDITOR_SETTINGS_CATEGORY_ICON_PATH);
    setWidgetCreator([this] { return new DisplaySettingsWidget(d); });
}

DisplaySettingsPage::~DisplaySettingsPage()
{
    delete d;
}

} // TextEditor
