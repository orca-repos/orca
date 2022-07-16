// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor.hpp>

namespace TextEditor {

class TEXTEDITOR_EXPORT PlainTextEditorFactory : public TextEditorFactory {
public:
  PlainTextEditorFactory();
  static auto instance() -> PlainTextEditorFactory*;
  static auto createPlainTextEditor() -> BaseTextEditor*;
};

} // namespace TextEditor
