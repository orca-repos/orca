// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cmake_global.hpp"

#include <texteditor/textindenter.hpp>

namespace CMakeProjectManager {
namespace Internal {

class CMAKE_EXPORT CMakeIndenter : public TextEditor::TextIndenter {
public:
  explicit CMakeIndenter(QTextDocument *doc);

  auto isElectricCharacter(const QChar &ch) const -> bool override;
  auto indentFor(const QTextBlock &block, const TextEditor::TabSettings &tabSettings, int cursorPositionInEditor = -1) -> int override;
};

} // namespace Internal
} // namespace CMakeProjectManager
