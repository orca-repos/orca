// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "syntaxhighlighter.hpp"

#include <utils/fileutils.hpp>

#include <AbstractHighlighter>
#include <Definition>

namespace TextEditor {

class TextDocument;

class Highlighter : public SyntaxHighlighter, public KSyntaxHighlighting::AbstractHighlighter {
  Q_OBJECT
  Q_INTERFACES(KSyntaxHighlighting::AbstractHighlighter)

public:
  using Definition = KSyntaxHighlighting::Definition;
  using Definitions = QList<Definition>;

  Highlighter();

  static auto definitionForName(const QString &name) -> Definition;
  static auto definitionsForDocument(const TextDocument *document) -> Definitions;
  static auto definitionsForMimeType(const QString &mimeType) -> Definitions;
  static auto definitionsForFileName(const Utils::FilePath &fileName) -> Definitions;
  static auto rememberDefinitionForDocument(const Definition &definition, const TextDocument *document) -> void;
  static auto clearDefinitionForDocumentCache() -> void;
  static auto addCustomHighlighterPath(const Utils::FilePath &path) -> void;
  static auto downloadDefinitions(std::function<void()> callback = nullptr) -> void;
  static auto reload() -> void;
  static auto handleShutdown() -> void;

protected:
  auto highlightBlock(const QString &text) -> void override;
  auto applyFormat(int offset, int length, const KSyntaxHighlighting::Format &format) -> void override;
  auto applyFolding(int offset, int length, KSyntaxHighlighting::FoldingRegion region) -> void override;
};

} // namespace TextEditor
