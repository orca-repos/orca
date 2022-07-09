// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <QFutureWatcher>
#include <QScopedPointer>
#include <QTextCharFormat>

#include <functional>

namespace TextEditor {
class HighlightingResult;
class TextDocument;
}

namespace CppEditor {

class CPPEDITOR_EXPORT SemanticHighlighter : public QObject {
  Q_OBJECT

public:
  enum Kind {
    Unknown = 0,
    TypeUse,
    NamespaceUse,
    LocalUse,
    FieldUse,
    EnumerationUse,
    VirtualMethodUse,
    LabelUse,
    MacroUse,
    FunctionUse,
    PseudoKeywordUse,
    FunctionDeclarationUse,
    VirtualFunctionDeclarationUse,
    StaticFieldUse,
    StaticMethodUse,
    StaticMethodDeclarationUse,
    AngleBracketOpen,
    AngleBracketClose,
    DoubleAngleBracketClose,
    TernaryIf,
    TernaryElse,
  };

  using HighlightingRunner = std::function<QFuture<TextEditor::HighlightingResult> ()>;
  
  explicit SemanticHighlighter(TextEditor::TextDocument *baseTextDocument);
  ~SemanticHighlighter() override;

  auto setHighlightingRunner(HighlightingRunner highlightingRunner) -> void;
  auto updateFormatMapFromFontSettings() -> void;
  auto run() -> void;

private:
  auto onHighlighterResultAvailable(int from, int to) -> void;
  auto onHighlighterFinished() -> void;
  auto connectWatcher() -> void;
  auto disconnectWatcher() -> void;
  auto documentRevision() const -> unsigned;
  
  TextEditor::TextDocument *m_baseTextDocument;
  unsigned m_revision = 0;
  QScopedPointer<QFutureWatcher<TextEditor::HighlightingResult>> m_watcher;
  QHash<int, QTextCharFormat> m_formatMap;
  HighlightingRunner m_highlightingRunner;
};

} // namespace CppEditor
