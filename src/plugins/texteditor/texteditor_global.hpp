// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#if defined(TEXTEDITOR_LIBRARY)
#  define TEXTEDITOR_EXPORT Q_DECL_EXPORT
#else
#  define TEXTEDITOR_EXPORT Q_DECL_IMPORT
#endif

namespace TextEditor {

enum TextPositionOperation {
  CurrentPosition = 1,
  EndOfLinePosition = 2,
  StartOfLinePosition = 3,
  AnchorPosition = 4,
  EndOfDocPosition = 5
};

} // namespace TextEditor
