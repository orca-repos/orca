// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>
#include <texteditor/texteditor.hpp>

namespace TextEditor {

class TEXTEDITOR_EXPORT SnippetEditorWidget : public TextEditorWidget {
  Q_OBJECT

public:
  SnippetEditorWidget(QWidget *parent = nullptr);

signals:
  auto snippetContentChanged() -> void;

protected:
  auto focusOutEvent(QFocusEvent *event) -> void override;
  auto contextMenuEvent(QContextMenuEvent *e) -> void override;
  auto extraAreaWidth(int * /* markWidthPtr */ = nullptr) const -> int override { return 0; }
};

} // namespace TextEditor
