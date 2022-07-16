// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "textmark.hpp"
#include "textdocument.hpp"

#include <utils/id.hpp>

#include <State>

#include <QTextBlockUserData>
#include <QPlainTextDocumentLayout>

namespace TextEditor {

struct TEXTEDITOR_EXPORT Parenthesis {
  enum Type : char {
    Opened,
    Closed
  };

  Parenthesis() = default;
  Parenthesis(Type t, QChar c, int position) : pos(position), chr(c), type(t) {}

  friend TEXTEDITOR_EXPORT auto operator<<(QDebug debug, const Parenthesis &parenthesis) -> QDebug;

  int pos = -1;
  QChar chr;
  Utils::Id source;
  Type type = Opened;

  auto operator==(const Parenthesis &other) const -> bool;
};

using Parentheses = QVector<Parenthesis>;
TEXTEDITOR_EXPORT auto insertSorted(Parentheses &list, const Parenthesis &elem) -> void;

class TEXTEDITOR_EXPORT CodeFormatterData {
public:
  virtual ~CodeFormatterData();
};

class TEXTEDITOR_EXPORT TextBlockUserData : public QTextBlockUserData {
public:
  TextBlockUserData() : m_foldingIndent(0), m_lexerState(0), m_folded(false), m_ifdefedOut(false), m_foldingStartIncluded(false), m_foldingEndIncluded(false), m_codeFormatterData(nullptr) {}
  ~TextBlockUserData() override;

  auto marks() const -> TextMarks { return m_marks; }
  auto addMark(TextMark *mark) -> void;
  auto removeMark(TextMark *mark) -> bool { return m_marks.removeAll(mark); }

  auto documentClosing() -> TextMarks
  {
    const auto marks = m_marks;
    for (const auto mrk : marks)
      mrk->setBaseTextDocument(nullptr);
    m_marks.clear();
    return marks;
  }

  auto setFolded(bool b) -> void { m_folded = b; }
  auto folded() const -> bool { return m_folded; }
  auto setParentheses(const Parentheses &parentheses) -> void { m_parentheses = parentheses; }
  auto clearParentheses() -> void { m_parentheses.clear(); }
  auto parentheses() const -> const Parentheses& { return m_parentheses; }
  auto hasParentheses() const -> bool { return !m_parentheses.isEmpty(); }
  auto braceDepthDelta() const -> int;

  auto setIfdefedOut() -> bool
  {
    const bool result = m_ifdefedOut;
    m_ifdefedOut = true;
    return !result;
  }

  auto clearIfdefedOut() -> bool
  {
    const bool result = m_ifdefedOut;
    m_ifdefedOut = false;
    return result;
  }

  auto ifdefedOut() const -> bool
  {
    return m_ifdefedOut;
  }

  enum MatchType {
    NoMatch,
    Match,
    Mismatch
  };

  static auto checkOpenParenthesis(QTextCursor *cursor, QChar c) -> MatchType;
  static auto checkClosedParenthesis(QTextCursor *cursor, QChar c) -> MatchType;
  static auto matchCursorBackward(QTextCursor *cursor) -> MatchType;
  static auto matchCursorForward(QTextCursor *cursor) -> MatchType;
  static auto findPreviousOpenParenthesis(QTextCursor *cursor, bool select = false, bool onlyInCurrentBlock = false) -> bool;
  static auto findNextClosingParenthesis(QTextCursor *cursor, bool select = false) -> bool;
  static auto findPreviousBlockOpenParenthesis(QTextCursor *cursor, bool checkStartPosition = false) -> bool;
  static auto findNextBlockClosingParenthesis(QTextCursor *cursor) -> bool;

  // Get the code folding level
  auto foldingIndent() const -> int { return m_foldingIndent; }
  /* Set the code folding level.
   *
   * A code folding marker will appear the line *before* the one where the indention
   * level increases. The code folding reagion will end in the last line that has the same
   * indention level (or higher).
   */
  auto setFoldingIndent(int indent) -> void { m_foldingIndent = indent; }
  // Set whether the first character of the folded region will show when the code is folded.
  auto setFoldingStartIncluded(bool included) -> void { m_foldingStartIncluded = included; }
  auto foldingStartIncluded() const -> bool { return m_foldingStartIncluded; }
  // Set whether the last character of the folded region will show when the code is folded.
  auto setFoldingEndIncluded(bool included) -> void { m_foldingEndIncluded = included; }
  auto foldingEndIncluded() const -> bool { return m_foldingEndIncluded; }
  auto lexerState() const -> int { return m_lexerState; }
  auto setLexerState(int state) -> void { m_lexerState = state; }
  auto setAdditionalAnnotationHeight(int annotationHeight) -> void { m_additionalAnnotationHeight = annotationHeight; }
  auto additionalAnnotationHeight() const -> int { return m_additionalAnnotationHeight; }
  auto codeFormatterData() const -> CodeFormatterData* { return m_codeFormatterData; }
  auto setCodeFormatterData(CodeFormatterData *data) -> void;
  auto syntaxState() -> KSyntaxHighlighting::State { return m_syntaxState; }
  auto setSyntaxState(KSyntaxHighlighting::State state) -> void { m_syntaxState = state; }
  auto expectedRawStringSuffix() -> QByteArray { return m_expectedRawStringSuffix; }
  auto setExpectedRawStringSuffix(const QByteArray &suffix) -> void { m_expectedRawStringSuffix = suffix; }

private:
  TextMarks m_marks;
  int m_foldingIndent : 16;
  int m_lexerState : 8;
  uint m_folded : 1;
  uint m_ifdefedOut : 1;
  uint m_foldingStartIncluded : 1;
  uint m_foldingEndIncluded : 1;
  int m_additionalAnnotationHeight = 0;
  Parentheses m_parentheses;
  CodeFormatterData *m_codeFormatterData;
  KSyntaxHighlighting::State m_syntaxState;
  QByteArray m_expectedRawStringSuffix; // A bit C++-specific, but let's be pragmatic.
};

class TEXTEDITOR_EXPORT TextDocumentLayout : public QPlainTextDocumentLayout {
  Q_OBJECT

public:
  TextDocumentLayout(QTextDocument *doc);
  ~TextDocumentLayout() override;

  static auto setParentheses(const QTextBlock &block, const Parentheses &parentheses) -> void;
  static auto clearParentheses(const QTextBlock &block) -> void { setParentheses(block, Parentheses()); }
  static auto parentheses(const QTextBlock &block) -> Parentheses;
  static auto hasParentheses(const QTextBlock &block) -> bool;
  static auto setIfdefedOut(const QTextBlock &block) -> bool;
  static auto clearIfdefedOut(const QTextBlock &block) -> bool;
  static auto ifdefedOut(const QTextBlock &block) -> bool;
  static auto braceDepthDelta(const QTextBlock &block) -> int;
  static auto braceDepth(const QTextBlock &block) -> int;
  static auto setBraceDepth(QTextBlock &block, int depth) -> void;
  static auto changeBraceDepth(QTextBlock &block, int delta) -> void;
  static auto setFoldingIndent(const QTextBlock &block, int indent) -> void;
  static auto foldingIndent(const QTextBlock &block) -> int;
  static auto setLexerState(const QTextBlock &block, int state) -> void;
  static auto lexerState(const QTextBlock &block) -> int;
  static auto changeFoldingIndent(QTextBlock &block, int delta) -> void;
  static auto canFold(const QTextBlock &block) -> bool;
  static auto doFoldOrUnfold(const QTextBlock &block, bool unfold) -> void;
  static auto isFolded(const QTextBlock &block) -> bool;
  static auto setFolded(const QTextBlock &block, bool folded) -> void;
  static auto setExpectedRawStringSuffix(const QTextBlock &block, const QByteArray &suffix) -> void;
  static auto expectedRawStringSuffix(const QTextBlock &block) -> QByteArray;

  class TEXTEDITOR_EXPORT FoldValidator {
  public:
    auto setup(TextDocumentLayout *layout) -> void;
    auto reset() -> void;
    auto process(QTextBlock block) -> void;
    auto finalize() -> void;

  private:
    TextDocumentLayout *m_layout = nullptr;
    bool m_requestDocUpdate = false;
    int m_insideFold = 0;
  };

  static auto textUserData(const QTextBlock &block) -> TextBlockUserData*
  {
    return static_cast<TextBlockUserData*>(block.userData());
  }

  static auto userData(const QTextBlock &block) -> TextBlockUserData*
  {
    auto data = static_cast<TextBlockUserData*>(block.userData());
    if (!data && block.isValid())
      const_cast<QTextBlock&>(block).setUserData(data = new TextBlockUserData);
    return data;
  }

  auto requestExtraAreaUpdate() -> void;

  auto emitDocumentSizeChanged() -> void
  {
    emit documentSizeChanged(documentSize());
  }

  int lastSaveRevision = 0;
  bool hasMarks = false;
  double maxMarkWidthFactor = 1.0;
  int m_requiredWidth = 0;

  auto setRequiredWidth(int width) -> void;
  auto documentSize() const -> QSizeF override;
  auto blockBoundingRect(const QTextBlock &block) const -> QRectF override;
  auto documentClosing() -> TextMarks;
  auto documentReloaded(TextMarks marks, TextDocument *baseextDocument) -> void;
  auto updateMarksLineNumber() -> void;
  auto updateMarksBlock(const QTextBlock &block) -> void;

signals:
  auto updateExtraArea() -> void;
  auto foldChanged(const int blockNumber, bool folded) -> void;
  auto parenthesesChanged(const QTextBlock block) -> void;
};

} // namespace TextEditor
