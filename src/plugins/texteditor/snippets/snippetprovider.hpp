// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>

#include <QString>

#include <functional>

namespace TextEditor {

class TextEditorWidget;

class TEXTEDITOR_EXPORT SnippetProvider {
public:
  SnippetProvider() = default;

  using EditorDecorator = std::function<void(TextEditorWidget *)>;

  static auto snippetProviders() -> const QList<SnippetProvider>&;
  static auto registerGroup(const QString &groupId, const QString &displayName, EditorDecorator editorDecorator = EditorDecorator()) -> void;
  auto groupId() const -> QString;
  auto displayName() const -> QString;
  static auto decorateEditor(TextEditorWidget *editor, const QString &groupId) -> void;

private:
  QString m_groupId;
  QString m_displayName;
  EditorDecorator m_editorDecorator;
};

} // TextEditor
