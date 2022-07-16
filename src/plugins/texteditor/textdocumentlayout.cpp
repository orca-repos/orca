// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textdocumentlayout.hpp"
#include "textdocument.hpp"

#include <QDebug>

namespace TextEditor {

CodeFormatterData::~CodeFormatterData() = default;

TextBlockUserData::~TextBlockUserData()
{
  for (const auto mrk : qAsConst(m_marks)) {
    mrk->baseTextDocument()->removeMarkFromMarksCache(mrk);
    mrk->setBaseTextDocument(nullptr);
    mrk->removedFromEditor();
  }

  delete m_codeFormatterData;
}

auto TextBlockUserData::braceDepthDelta() const -> int
{
  auto delta = 0;
  for (auto &parenthesis : m_parentheses) {
    switch (parenthesis.chr.unicode()) {
    case '{':
    case '+':
    case '[':
      ++delta;
      break;
    case '}':
    case '-':
    case ']':
      --delta;
      break;
    default:
      break;
    }
  }
  return delta;
}

auto TextBlockUserData::checkOpenParenthesis(QTextCursor *cursor, QChar c) -> MatchType
{
  const auto block = cursor->block();
  if (!TextDocumentLayout::hasParentheses(block) || TextDocumentLayout::ifdefedOut(block))
    return NoMatch;

  auto parenList = TextDocumentLayout::parentheses(block);
  Parenthesis openParen, closedParen;
  auto closedParenParag = block;

  const auto cursorPos = cursor->position() - closedParenParag.position();
  auto i = 0;
  auto ignore = 0;
  auto foundOpen = false;
  for (;;) {
    if (!foundOpen) {
      if (i >= parenList.count())
        return NoMatch;
      openParen = parenList.at(i);
      if (openParen.pos != cursorPos) {
        ++i;
        continue;
      }
      foundOpen = true;
      ++i;
    }

    if (i >= parenList.count()) {
      for (;;) {
        closedParenParag = closedParenParag.next();
        if (!closedParenParag.isValid())
          return NoMatch;
        if (TextDocumentLayout::hasParentheses(closedParenParag) && !TextDocumentLayout::ifdefedOut(closedParenParag)) {
          parenList = TextDocumentLayout::parentheses(closedParenParag);
          break;
        }
      }
      i = 0;
    }

    closedParen = parenList.at(i);
    if (closedParen.type == Parenthesis::Opened) {
      ignore++;
      ++i;
      continue;
    }
    if (ignore > 0) {
      ignore--;
      ++i;
      continue;
    }

    cursor->clearSelection();
    cursor->setPosition(closedParenParag.position() + closedParen.pos + 1, QTextCursor::KeepAnchor);

    if (c == QLatin1Char('{') && closedParen.chr != QLatin1Char('}') || c == QLatin1Char('(') && closedParen.chr != QLatin1Char(')') || c == QLatin1Char('[') && closedParen.chr != QLatin1Char(']') || c == QLatin1Char('+') && closedParen.chr != QLatin1Char('-'))
      return Mismatch;

    return Match;
  }
}

auto TextBlockUserData::checkClosedParenthesis(QTextCursor *cursor, QChar c) -> MatchType
{
  const auto block = cursor->block();
  if (!TextDocumentLayout::hasParentheses(block) || TextDocumentLayout::ifdefedOut(block))
    return NoMatch;

  auto parenList = TextDocumentLayout::parentheses(block);
  Parenthesis openParen, closedParen;
  auto openParenParag = block;

  const auto cursorPos = cursor->position() - openParenParag.position();
  int i = parenList.count() - 1;
  auto ignore = 0;
  auto foundClosed = false;
  for (;;) {
    if (!foundClosed) {
      if (i < 0)
        return NoMatch;
      closedParen = parenList.at(i);
      if (closedParen.pos != cursorPos - 1) {
        --i;
        continue;
      }
      foundClosed = true;
      --i;
    }

    if (i < 0) {
      for (;;) {
        openParenParag = openParenParag.previous();
        if (!openParenParag.isValid())
          return NoMatch;

        if (TextDocumentLayout::hasParentheses(openParenParag) && !TextDocumentLayout::ifdefedOut(openParenParag)) {
          parenList = TextDocumentLayout::parentheses(openParenParag);
          break;
        }
      }
      i = parenList.count() - 1;
    }

    openParen = parenList.at(i);
    if (openParen.type == Parenthesis::Closed) {
      ignore++;
      --i;
      continue;
    }
    if (ignore > 0) {
      ignore--;
      --i;
      continue;
    }

    cursor->clearSelection();
    cursor->setPosition(openParenParag.position() + openParen.pos, QTextCursor::KeepAnchor);

    if (c == QLatin1Char('}') && openParen.chr != QLatin1Char('{') || c == QLatin1Char(')') && openParen.chr != QLatin1Char('(') || c == QLatin1Char(']') && openParen.chr != QLatin1Char('[') || c == QLatin1Char('-') && openParen.chr != QLatin1Char('+'))
      return Mismatch;

    return Match;
  }
}

auto TextBlockUserData::findPreviousOpenParenthesis(QTextCursor *cursor, bool select, bool onlyInCurrentBlock) -> bool
{
  auto block = cursor->block();
  const auto position = cursor->position();
  auto ignore = 0;
  while (block.isValid()) {
    auto parenList = TextDocumentLayout::parentheses(block);
    if (!parenList.isEmpty() && !TextDocumentLayout::ifdefedOut(block)) {
      for (int i = parenList.count() - 1; i >= 0; --i) {
        const auto paren = parenList.at(i);
        if (block == cursor->block() && position - block.position() <= paren.pos + (paren.type == Parenthesis::Closed ? 1 : 0))
          continue;
        if (paren.type == Parenthesis::Closed) {
          ++ignore;
        } else if (ignore > 0) {
          --ignore;
        } else {
          cursor->setPosition(block.position() + paren.pos, select ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
          return true;
        }
      }
    }
    if (onlyInCurrentBlock)
      return false;
    block = block.previous();
  }
  return false;
}

auto TextBlockUserData::findPreviousBlockOpenParenthesis(QTextCursor *cursor, bool checkStartPosition) -> bool
{
  auto block = cursor->block();
  const auto position = cursor->position();
  auto ignore = 0;
  while (block.isValid()) {
    auto parenList = TextDocumentLayout::parentheses(block);
    if (!parenList.isEmpty() && !TextDocumentLayout::ifdefedOut(block)) {
      for (int i = parenList.count() - 1; i >= 0; --i) {
        const auto paren = parenList.at(i);
        if (paren.chr != QLatin1Char('+') && paren.chr != QLatin1Char('-'))
          continue;
        if (block == cursor->block()) {
          if (position - block.position() <= paren.pos + (paren.type == Parenthesis::Closed ? 1 : 0))
            continue;
          if (checkStartPosition && paren.type == Parenthesis::Opened && paren.pos == cursor->position())
            return true;
        }
        if (paren.type == Parenthesis::Closed) {
          ++ignore;
        } else if (ignore > 0) {
          --ignore;
        } else {
          cursor->setPosition(block.position() + paren.pos);
          return true;
        }
      }
    }
    block = block.previous();
  }
  return false;
}

auto TextBlockUserData::findNextClosingParenthesis(QTextCursor *cursor, bool select) -> bool
{
  auto block = cursor->block();
  const auto position = cursor->position();
  auto ignore = 0;
  while (block.isValid()) {
    auto parenList = TextDocumentLayout::parentheses(block);
    if (!parenList.isEmpty() && !TextDocumentLayout::ifdefedOut(block)) {
      for (auto i = 0; i < parenList.count(); ++i) {
        const auto paren = parenList.at(i);
        if (block == cursor->block() && position - block.position() > paren.pos - (paren.type == Parenthesis::Opened ? 1 : 0))
          continue;
        if (paren.type == Parenthesis::Opened) {
          ++ignore;
        } else if (ignore > 0) {
          --ignore;
        } else {
          cursor->setPosition(block.position() + paren.pos + 1, select ? QTextCursor::KeepAnchor : QTextCursor::MoveAnchor);
          return true;
        }
      }
    }
    block = block.next();
  }
  return false;
}

auto TextBlockUserData::findNextBlockClosingParenthesis(QTextCursor *cursor) -> bool
{
  auto block = cursor->block();
  const auto position = cursor->position();
  auto ignore = 0;
  while (block.isValid()) {
    auto parenList = TextDocumentLayout::parentheses(block);
    if (!parenList.isEmpty() && !TextDocumentLayout::ifdefedOut(block)) {
      for (auto i = 0; i < parenList.count(); ++i) {
        const auto paren = parenList.at(i);
        if (paren.chr != QLatin1Char('+') && paren.chr != QLatin1Char('-'))
          continue;
        if (block == cursor->block() && position - block.position() > paren.pos - (paren.type == Parenthesis::Opened ? 1 : 0))
          continue;
        if (paren.type == Parenthesis::Opened) {
          ++ignore;
        } else if (ignore > 0) {
          --ignore;
        } else {
          cursor->setPosition(block.position() + paren.pos + 1);
          return true;
        }
      }
    }
    block = block.next();
  }
  return false;
}

auto TextBlockUserData::matchCursorBackward(QTextCursor *cursor) -> MatchType
{
  cursor->clearSelection();
  const auto block = cursor->block();

  if (!TextDocumentLayout::hasParentheses(block) || TextDocumentLayout::ifdefedOut(block))
    return NoMatch;

  const auto relPos = cursor->position() - block.position();

  const auto parentheses = TextDocumentLayout::parentheses(block);
  const auto cend = parentheses.constEnd();
  for (auto it = parentheses.constBegin(); it != cend; ++it) {
    const auto &paren = *it;
    if (paren.pos == relPos - 1 && paren.type == Parenthesis::Closed)
      return checkClosedParenthesis(cursor, paren.chr);
  }
  return NoMatch;
}

auto TextBlockUserData::matchCursorForward(QTextCursor *cursor) -> MatchType
{
  cursor->clearSelection();
  const auto block = cursor->block();

  if (!TextDocumentLayout::hasParentheses(block) || TextDocumentLayout::ifdefedOut(block))
    return NoMatch;

  const auto relPos = cursor->position() - block.position();

  const auto parentheses = TextDocumentLayout::parentheses(block);
  const auto cend = parentheses.constEnd();
  for (auto it = parentheses.constBegin(); it != cend; ++it) {
    const auto &paren = *it;
    if (paren.pos == relPos && paren.type == Parenthesis::Opened) {
      return checkOpenParenthesis(cursor, paren.chr);
    }
  }
  return NoMatch;
}

auto TextBlockUserData::setCodeFormatterData(CodeFormatterData *data) -> void
{
  if (m_codeFormatterData)
    delete m_codeFormatterData;

  m_codeFormatterData = data;
}

auto TextBlockUserData::addMark(TextMark *mark) -> void
{
  auto i = 0;
  for (; i < m_marks.size(); ++i) {
    if (mark->priority() < m_marks.at(i)->priority())
      break;
  }
  m_marks.insert(i, mark);
}

TextDocumentLayout::TextDocumentLayout(QTextDocument *doc) : QPlainTextDocumentLayout(doc) {}

TextDocumentLayout::~TextDocumentLayout()
{
  documentClosing();
}

auto TextDocumentLayout::setParentheses(const QTextBlock &block, const Parentheses &parentheses) -> void
{
  if (TextDocumentLayout::parentheses(block) == parentheses)
    return;

  userData(block)->setParentheses(parentheses);
  if (const auto layout = qobject_cast<TextDocumentLayout*>(block.document()->documentLayout())) emit layout->parenthesesChanged(block);
}

auto TextDocumentLayout::parentheses(const QTextBlock &block) -> Parentheses
{
  if (const auto userData = textUserData(block))
    return userData->parentheses();
  return Parentheses();
}

auto TextDocumentLayout::hasParentheses(const QTextBlock &block) -> bool
{
  if (const auto userData = textUserData(block))
    return userData->hasParentheses();
  return false;
}

auto TextDocumentLayout::setIfdefedOut(const QTextBlock &block) -> bool
{
  return userData(block)->setIfdefedOut();
}

auto TextDocumentLayout::clearIfdefedOut(const QTextBlock &block) -> bool
{
  if (const auto userData = textUserData(block))
    return userData->clearIfdefedOut();
  return false;
}

auto TextDocumentLayout::ifdefedOut(const QTextBlock &block) -> bool
{
  if (const auto userData = textUserData(block))
    return userData->ifdefedOut();
  return false;
}

auto TextDocumentLayout::braceDepthDelta(const QTextBlock &block) -> int
{
  if (const auto userData = textUserData(block))
    return userData->braceDepthDelta();
  return 0;
}

auto TextDocumentLayout::braceDepth(const QTextBlock &block) -> int
{
  const auto state = block.userState();
  if (state == -1)
    return 0;
  return state >> 8;
}

auto TextDocumentLayout::setBraceDepth(QTextBlock &block, int depth) -> void
{
  auto state = block.userState();
  if (state == -1)
    state = 0;
  state = state & 0xff;
  block.setUserState(depth << 8 | state);
}

auto TextDocumentLayout::changeBraceDepth(QTextBlock &block, int delta) -> void
{
  if (delta)
    setBraceDepth(block, braceDepth(block) + delta);
}

auto TextDocumentLayout::setLexerState(const QTextBlock &block, int state) -> void
{
  if (state == 0) {
    if (const auto userData = textUserData(block))
      userData->setLexerState(0);
  } else {
    userData(block)->setLexerState(qMax(0, state));
  }
}

auto TextDocumentLayout::lexerState(const QTextBlock &block) -> int
{
  if (const auto userData = textUserData(block))
    return userData->lexerState();
  return 0;
}

auto TextDocumentLayout::setFoldingIndent(const QTextBlock &block, int indent) -> void
{
  if (indent == 0) {
    if (const auto userData = textUserData(block))
      userData->setFoldingIndent(0);
  } else {
    userData(block)->setFoldingIndent(indent);
  }
}

auto TextDocumentLayout::foldingIndent(const QTextBlock &block) -> int
{
  if (const auto userData = textUserData(block))
    return userData->foldingIndent();
  return 0;
}

auto TextDocumentLayout::changeFoldingIndent(QTextBlock &block, int delta) -> void
{
  if (delta)
    setFoldingIndent(block, foldingIndent(block) + delta);
}

auto TextDocumentLayout::canFold(const QTextBlock &block) -> bool
{
  return block.next().isValid() && foldingIndent(block.next()) > foldingIndent(block);
}

auto TextDocumentLayout::isFolded(const QTextBlock &block) -> bool
{
  if (const auto userData = textUserData(block))
    return userData->folded();
  return false;
}

auto TextDocumentLayout::setFolded(const QTextBlock &block, bool folded) -> void
{
  if (folded)
    userData(block)->setFolded(true);
  else if (const auto userData = textUserData(block))
    userData->setFolded(false);
  else
    return;

  if (const auto layout = qobject_cast<TextDocumentLayout*>(block.document()->documentLayout())) emit layout->foldChanged(block.blockNumber(), folded);
}

auto TextDocumentLayout::setExpectedRawStringSuffix(const QTextBlock &block, const QByteArray &suffix) -> void
{
  if (const auto data = textUserData(block))
    data->setExpectedRawStringSuffix(suffix);
  else if (!suffix.isEmpty())
    userData(block)->setExpectedRawStringSuffix(suffix);
}

auto TextDocumentLayout::expectedRawStringSuffix(const QTextBlock &block) -> QByteArray
{
  if (const auto userData = textUserData(block))
    return userData->expectedRawStringSuffix();
  return {};
}

auto TextDocumentLayout::requestExtraAreaUpdate() -> void
{
  emit updateExtraArea();
}

auto TextDocumentLayout::doFoldOrUnfold(const QTextBlock &block, bool unfold) -> void
{
  if (!canFold(block))
    return;
  auto b = block.next();

  const auto indent = foldingIndent(block);
  while (b.isValid() && foldingIndent(b) > indent && (unfold || b.next().isValid())) {
    b.setVisible(unfold);
    b.setLineCount(unfold ? qMax(1, b.layout()->lineCount()) : 0);
    if (unfold) {
      // do not unfold folded sub-blocks
      if (isFolded(b) && b.next().isValid()) {
        const auto jndent = foldingIndent(b);
        b = b.next();
        while (b.isValid() && foldingIndent(b) > jndent)
          b = b.next();
        continue;
      }
    }
    b = b.next();
  }
  setFolded(block, !unfold);
}

auto TextDocumentLayout::setRequiredWidth(int width) -> void
{
  const auto oldw = m_requiredWidth;
  m_requiredWidth = width;
  const auto dw = int(QPlainTextDocumentLayout::documentSize().width());
  if (oldw > dw || width > dw)
    emitDocumentSizeChanged();
}

auto TextDocumentLayout::documentSize() const -> QSizeF
{
  auto size = QPlainTextDocumentLayout::documentSize();
  size.setWidth(qMax(qreal(m_requiredWidth), size.width()));
  return size;
}

auto TextDocumentLayout::documentClosing() -> TextMarks
{
  TextMarks marks;
  for (auto block = document()->begin(); block.isValid(); block = block.next()) {
    if (const auto data = static_cast<TextBlockUserData*>(block.userData()))
      marks.append(data->documentClosing());
  }
  return marks;
}

auto TextDocumentLayout::documentReloaded(TextMarks marks, TextDocument *baseTextDocument) -> void
{
  for (const auto mark : qAsConst(marks)) {
    const auto blockNumber = mark->lineNumber() - 1;
    auto block = document()->findBlockByNumber(blockNumber);
    if (block.isValid()) {
      const auto userData = TextDocumentLayout::userData(block);
      userData->addMark(mark);
      mark->setBaseTextDocument(baseTextDocument);
      mark->updateBlock(block);
    } else {
      baseTextDocument->removeMarkFromMarksCache(mark);
      mark->setBaseTextDocument(nullptr);
      mark->removedFromEditor();
    }
  }
  requestUpdate();
}

auto TextDocumentLayout::updateMarksLineNumber() -> void
{
  // Note: the breakpointmanger deletes breakpoint marks and readds them
  // if it doesn't agree with our updating
  auto block = document()->begin();
  auto blockNumber = 0;
  while (block.isValid()) {
    if (const TextBlockUserData *userData = textUserData(block)) {
      for (const auto mrk : userData->marks())
        mrk->updateLineNumber(blockNumber + 1);
    }
    block = block.next();
    ++blockNumber;
  }
}

auto TextDocumentLayout::updateMarksBlock(const QTextBlock &block) -> void
{
  if (const TextBlockUserData *userData = textUserData(block)) {
    for (const auto mrk : userData->marks())
      mrk->updateBlock(block);
  }
}

auto TextDocumentLayout::blockBoundingRect(const QTextBlock &block) const -> QRectF
{
  auto boundingRect = QPlainTextDocumentLayout::blockBoundingRect(block);
  if (const auto userData = textUserData(block))
    boundingRect.adjust(0, 0, 0, userData->additionalAnnotationHeight());
  return boundingRect;
}

auto TextDocumentLayout::FoldValidator::setup(TextDocumentLayout *layout) -> void
{
  m_layout = layout;
}

auto TextDocumentLayout::FoldValidator::reset() -> void
{
  m_insideFold = 0;
  m_requestDocUpdate = false;
}

auto TextDocumentLayout::FoldValidator::process(QTextBlock block) -> void
{
  if (!m_layout)
    return;

  const auto &previous = block.previous();
  if (!previous.isValid())
    return;

  const auto preIsFolded = isFolded(previous);
  const auto preCanFold = canFold(previous);
  const auto isVisible = block.isVisible();

  if (preIsFolded && !preCanFold)
    setFolded(previous, false);
  else if (!preIsFolded && preCanFold && previous.isVisible() && !isVisible)
    setFolded(previous, true);

  if (isFolded(previous) && !m_insideFold)
    m_insideFold = foldingIndent(block);

  auto shouldBeVisible = m_insideFold == 0;
  if (!shouldBeVisible) {
    shouldBeVisible = foldingIndent(block) < m_insideFold;
    if (shouldBeVisible)
      m_insideFold = 0;
  }

  if (shouldBeVisible != isVisible) {
    block.setVisible(shouldBeVisible);
    block.setLineCount(block.isVisible() ? qMax(1, block.layout()->lineCount()) : 0);
    m_requestDocUpdate = true;
  }
}

auto TextDocumentLayout::FoldValidator::finalize() -> void
{
  if (m_requestDocUpdate && m_layout) {
    m_layout->requestUpdate();
    m_layout->emitDocumentSizeChanged();
  }
}

auto operator<<(QDebug debug, const Parenthesis &parenthesis) -> QDebug
{
  QDebugStateSaver saver(debug);
  debug << (parenthesis.type == Parenthesis::Opened ? "Opening " : "Closing ") << parenthesis.chr << " at " << parenthesis.pos;

  return debug;
}

auto Parenthesis::operator==(const Parenthesis &other) const -> bool
{
  return pos == other.pos && chr == other.chr && source == other.source && type == other.type;
}

auto insertSorted(Parentheses &list, const Parenthesis &elem) -> void
{
  const auto it = std::lower_bound(list.begin(), list.end(), elem, [](const auto &p1, const auto &p2) { return p1.pos < p2.pos; });
  list.insert(it, elem);
}

} // namespace TextEditor
