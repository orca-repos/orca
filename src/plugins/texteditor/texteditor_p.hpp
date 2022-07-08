// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QList>

namespace TextEditor {

class TextDocument;

namespace Internal {

//
// TextEditorPrivate
//

struct TextEditorPrivateHighlightBlocks {
  auto count() const -> int { return visualIndent.size(); }
  auto isEmpty() const -> bool { return open.isEmpty() || close.isEmpty() || visualIndent.isEmpty(); }

  auto operator==(const TextEditorPrivateHighlightBlocks &o) const -> bool
  {
    return open == o.open && close == o.close && visualIndent == o.visualIndent;
  }

  auto operator!=(const TextEditorPrivateHighlightBlocks &o) const -> bool { return !(*this == o); }

  QList<int> open;
  QList<int> close;
  QList<int> visualIndent;
};

} // namespace Internal
} // namespace TextEditor
