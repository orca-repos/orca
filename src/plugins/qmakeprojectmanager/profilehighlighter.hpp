// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/syntaxhighlighter.hpp>
#include <texteditor/codeassist/keywordscompletionassist.hpp>

namespace QmakeProjectManager {
namespace Internal {

class ProFileHighlighter : public TextEditor::SyntaxHighlighter {
public:
  enum ProfileFormats {
    ProfileVariableFormat,
    ProfileFunctionFormat,
    ProfileCommentFormat,
    ProfileVisualWhitespaceFormat,
    NumProfileFormats
  };

  ProFileHighlighter();

  auto highlightBlock(const QString &text) -> void override;

private:
  const TextEditor::Keywords m_keywords;
};

} // namespace Internal
} // namespace QmakeProjectManager
