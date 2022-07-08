// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "indenter.hpp"

#include <utils/id.hpp>

#include <QWidget>

namespace  ProjectExplorer { class Project; }

namespace TextEditor {

class ICodeStylePreferences;

class TEXTEDITOR_EXPORT CodeStyleEditorWidget : public QWidget {
  Q_OBJECT

public:
  CodeStyleEditorWidget(QWidget *parent = nullptr) : QWidget(parent) {}
  virtual auto apply() -> void {}
};

class TEXTEDITOR_EXPORT ICodeStylePreferencesFactory {
  ICodeStylePreferencesFactory(const ICodeStylePreferencesFactory &) = delete;
  auto operator=(const ICodeStylePreferencesFactory &) -> ICodeStylePreferencesFactory& = delete;

public:
  ICodeStylePreferencesFactory();
  virtual ~ICodeStylePreferencesFactory() = default;

  virtual auto createCodeStyleEditor(ICodeStylePreferences *codeStyle, ProjectExplorer::Project *project = nullptr, QWidget *parent = nullptr) -> CodeStyleEditorWidget*;
  virtual auto languageId() -> Utils::Id = 0;
  virtual auto displayName() -> QString = 0;
  virtual auto createCodeStyle() const -> ICodeStylePreferences* = 0;
  virtual auto createEditor(ICodeStylePreferences *preferences, ProjectExplorer::Project *project = nullptr, QWidget *parent = nullptr) const -> QWidget* = 0;
  virtual auto createIndenter(QTextDocument *doc) const -> Indenter* = 0;
  virtual auto snippetProviderGroupId() const -> QString = 0;
  virtual auto previewText() const -> QString = 0;
};

} // namespace TextEditor
