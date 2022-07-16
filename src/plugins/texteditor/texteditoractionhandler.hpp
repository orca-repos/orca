// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <utils/id.hpp>

#include <functional>

namespace Orca::Plugin::Core {
class IEditor;
}

namespace TextEditor {
class TextEditorWidget;

namespace Internal {
class TextEditorActionHandlerPrivate;
}

// Redirects slots from global actions to the respective editor.

class TEXTEDITOR_EXPORT TextEditorActionHandler final {
  TextEditorActionHandler(const TextEditorActionHandler &) = delete;
  auto operator=(const TextEditorActionHandler &) -> TextEditorActionHandler& = delete;

public:
  enum OptionalActionsMask {
    None = 0,
    Format = 1,
    UnCommentSelection = 2,
    UnCollapseAll = 4,
    FollowSymbolUnderCursor = 8,
    JumpToFileUnderCursor = 16,
    RenameSymbol = 32,
  };

  using TextEditorWidgetResolver = std::function<TextEditorWidget *(Orca::Plugin::Core::IEditor *)>;

  TextEditorActionHandler(Utils::Id editorId, Utils::Id contextId, uint optionalActions = None, const TextEditorWidgetResolver &resolver = {});
  auto optionalActions() const -> uint;
  ~TextEditorActionHandler();

private:
  Internal::TextEditorActionHandlerPrivate *d;
};

} // namespace TextEditor
