// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppcodestylesettings.hpp"
#include "cppcodeformatter.hpp"

#include <core/core-options-page-interface.hpp>
#include <texteditor/icodestylepreferencesfactory.hpp>

#include <QWidget>
#include <QPointer>

namespace TextEditor {
class FontSettings;
class TabSettings;
class SnippetEditorWidget;
class CodeStyleEditor;
class CodeStyleEditorWidget;
}

namespace CppEditor {

class CppCodeStylePreferences;

class CPPEDITOR_EXPORT CppCodeStyleWidget : public TextEditor::CodeStyleEditorWidget {
  Q_OBJECT public:
  CppCodeStyleWidget(QWidget *parent = nullptr) : CodeStyleEditorWidget(parent) {}

  virtual auto setCodeStyleSettings(const CppEditor::CppCodeStyleSettings &) -> void {}
  virtual auto setTabSettings(const TextEditor::TabSettings &) -> void {}
  virtual auto synchronize() -> void {}

signals:
  auto codeStyleSettingsChanged(const CppEditor::CppCodeStyleSettings &) -> void;
  auto tabSettingsChanged(const TextEditor::TabSettings &) -> void;
};

namespace Internal {
namespace Ui {

class CppCodeStyleSettingsPage;
}

class CppCodeStylePreferencesWidget : public QWidget {
  Q_OBJECT

public:
  explicit CppCodeStylePreferencesWidget(QWidget *parent = nullptr);
  ~CppCodeStylePreferencesWidget() override;

  auto setCodeStyle(CppCodeStylePreferences *codeStylePreferences) -> void;
  auto addTab(CppCodeStyleWidget *page, QString tabName) -> void;

private:
  auto decorateEditors(const TextEditor::FontSettings &fontSettings) -> void;
  auto setVisualizeWhitespace(bool on) -> void;
  auto slotTabSettingsChanged(const TextEditor::TabSettings &settings) -> void;
  auto slotCodeStyleSettingsChanged() -> void;
  auto updatePreview() -> void;
  auto setTabSettings(const TextEditor::TabSettings &settings) -> void;
  auto tabSettings() const -> TextEditor::TabSettings;
  auto setCodeStyleSettings(const CppCodeStyleSettings &settings, bool preview = true) -> void;
  auto slotCurrentPreferencesChanged(TextEditor::ICodeStylePreferences *, bool preview = true) -> void;
  auto cppCodeStyleSettings() const -> CppCodeStyleSettings;

  CppCodeStylePreferences *m_preferences = nullptr;
  Ui::CppCodeStyleSettingsPage *m_ui;
  QList<TextEditor::SnippetEditorWidget*> m_previews;
  bool m_blockUpdates = false;

signals:
  auto codeStyleSettingsChanged(const CppEditor::CppCodeStyleSettings &) -> void;
  auto tabSettingsChanged(const TextEditor::TabSettings &) -> void;
};

class CppCodeStyleSettingsPage : public Orca::Plugin::Core::IOptionsPage {
public:
  CppCodeStyleSettingsPage();

  auto widget() -> QWidget* override;
  auto apply() -> void override;
  auto finish() -> void override;

private:
  CppCodeStylePreferences *m_pageCppCodeStylePreferences = nullptr;
  QPointer<TextEditor::CodeStyleEditorWidget> m_widget;
};

} // namespace Internal
} // namespace CppEditor
