// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "texteditoroverlay.hpp"
#include "texteditor.hpp"

#include <QDebug>
#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>

#include <algorithm>

using namespace TextEditor;
using namespace Internal;

TextEditorOverlay::TextEditorOverlay(TextEditorWidget *editor) : QObject(editor), m_visible(false), m_alpha(true), m_borderWidth(1), m_dropShadowWidth(2), m_firstSelectionOriginalBegin(-1), m_editor(editor), m_viewport(editor->viewport()) {}

auto TextEditorOverlay::update() -> void
{
  if (m_visible)
    m_viewport->update();
}

auto TextEditorOverlay::setVisible(bool b) -> void
{
  if (m_visible == b)
    return;
  m_visible = b;
  if (!m_selections.isEmpty())
    m_viewport->update();
}

auto TextEditorOverlay::clear() -> void
{
  if (m_selections.isEmpty())
    return;
  m_selections.clear();
  m_firstSelectionOriginalBegin = -1;
  update();
}

auto TextEditorOverlay::addOverlaySelection(int begin, int end, const QColor &fg, const QColor &bg, uint overlaySelectionFlags) -> void
{
  if (end < begin)
    return;

  auto document = m_editor->document();

  OverlaySelection selection;
  selection.m_fg = fg;
  selection.m_bg = bg;

  selection.m_cursor_begin = QTextCursor(document);
  selection.m_cursor_begin.setPosition(begin);
  selection.m_cursor_end = QTextCursor(document);
  selection.m_cursor_end.setPosition(end);

  if (overlaySelectionFlags & ExpandBegin)
    selection.m_cursor_begin.setKeepPositionOnInsert(true);

  if (overlaySelectionFlags & LockSize)
    selection.m_fixedLength = end - begin;

  selection.m_dropShadow = overlaySelectionFlags & DropShadow;

  if (m_selections.isEmpty())
    m_firstSelectionOriginalBegin = begin;
  else if (begin < m_firstSelectionOriginalBegin)
    qWarning() << "overlay selections not in order";

  m_selections.append(selection);
  update();
}

auto TextEditorOverlay::addOverlaySelection(const QTextCursor &cursor, const QColor &fg, const QColor &bg, uint overlaySelectionFlags) -> void
{
  addOverlaySelection(cursor.selectionStart(), cursor.selectionEnd(), fg, bg, overlaySelectionFlags);
}

auto TextEditorOverlay::rect() const -> QRect
{
  return m_viewport->rect();
}

auto TextEditorOverlay::createSelectionPath(const QTextCursor &begin, const QTextCursor &end, const QRect &clip) -> QPainterPath
{
  if (begin.isNull() || end.isNull() || begin.position() > end.position())
    return QPainterPath();

  auto offset = m_editor->contentOffset();
  auto viewportRect = rect();
  auto document = m_editor->document();

  if (m_editor->blockBoundingGeometry(begin.block()).translated(offset).top() > clip.bottom() + 10 || m_editor->blockBoundingGeometry(end.block()).translated(offset).bottom() < clip.top() - 10)
    return QPainterPath(); // nothing of the selection is visible

  auto block = begin.block();

  if (block.blockNumber() < m_editor->firstVisibleBlock().blockNumber() - 4)
    block = m_editor->document()->findBlockByNumber(m_editor->firstVisibleBlock().blockNumber() - 4);

  auto inSelection = false;

  QVector<QRectF> selection;

  if (begin.position() == end.position()) {
    // special case empty selections
    const auto blockGeometry = m_editor->blockBoundingGeometry(block);
    auto blockLayout = block.layout();
    auto pos = begin.position() - begin.block().position();
    auto line = blockLayout->lineForTextPosition(pos);
    auto lineRect = line.naturalTextRect();
    int x = line.cursorToX(pos);
    lineRect.setLeft(x - m_borderWidth);
    lineRect.setRight(x + m_borderWidth);
    selection += lineRect.translated(blockGeometry.topLeft());
  } else {
    for (; block.isValid() && block.blockNumber() <= end.blockNumber(); block = block.next()) {
      if (! block.isVisible())
        continue;

      const auto blockGeometry = m_editor->blockBoundingGeometry(block);
      auto blockLayout = block.layout();

      auto line = blockLayout->lineAt(0);
      auto firstOrLastBlock = false;

      auto beginChar = 0;
      if (!inSelection) {
        if (block == begin.block()) {
          beginChar = begin.positionInBlock();
          line = blockLayout->lineForTextPosition(beginChar);
          firstOrLastBlock = true;
        }
        inSelection = true;
      } else {
        //                while (beginChar < block.length() && document->characterAt(block.position() + beginChar).isSpace())
        //                    ++beginChar;
        //                if (beginChar == block.length())
        //                    beginChar = 0;
      }

      auto lastLine = blockLayout->lineCount() - 1;
      auto endChar = -1;
      if (block == end.block()) {
        endChar = end.positionInBlock();
        lastLine = blockLayout->lineForTextPosition(endChar).lineNumber();
        inSelection = false;
        firstOrLastBlock = true;
      } else {
        endChar = block.length();
        while (endChar > beginChar && document->characterAt(block.position() + endChar - 1).isSpace())
          --endChar;
      }

      auto lineRect = line.naturalTextRect();
      if (beginChar < endChar) {
        lineRect.setLeft(line.cursorToX(beginChar));
        if (line.lineNumber() == lastLine)
          lineRect.setRight(line.cursorToX(endChar));
        selection += lineRect.translated(blockGeometry.topLeft());

        for (auto lineIndex = line.lineNumber() + 1; lineIndex <= lastLine; ++lineIndex) {
          line = blockLayout->lineAt(lineIndex);
          lineRect = line.naturalTextRect();
          if (lineIndex == lastLine)
            lineRect.setRight(line.cursorToX(endChar));
          selection += lineRect.translated(blockGeometry.topLeft());
        }
      } else {
        // empty lines
        const auto emptyLineSelectionSize = 16;
        if (!firstOrLastBlock && !selection.isEmpty()) {
          // middle
          lineRect.setLeft(selection.last().left());
        } else if (inSelection) {
          // first line
          lineRect.setLeft(line.cursorToX(beginChar));
        } else {
          // last line
          if (endChar == 0)
            break;
          lineRect.setLeft(line.cursorToX(endChar) - emptyLineSelectionSize);
        }
        lineRect.setRight(lineRect.left() + emptyLineSelectionSize);
        selection += lineRect.translated(blockGeometry.topLeft());
      }

      if (!inSelection)
        break;

      if (blockGeometry.translated(offset).y() > 2 * viewportRect.height())
        break;
    }
  }

  if (selection.isEmpty())
    return QPainterPath();

  QVector<QPointF> points;

  const auto margin = m_borderWidth / 2;
  const auto extra = 0;

  const auto &firstSelection = selection.at(0);
  points += (firstSelection.topLeft() + firstSelection.topRight()) / 2 + QPointF(0, -margin);
  points += firstSelection.topRight() + QPointF(margin + 1, -margin);
  points += firstSelection.bottomRight() + QPointF(margin + 1, 0);

  const int count = selection.count();
  for (auto i = 1; i < count - 1; ++i) {
    auto x = std::max({selection.at(i - 1).right(), selection.at(i).right(), selection.at(i + 1).right()}) + margin;

    points += QPointF(x + 1, selection.at(i).top());
    points += QPointF(x + 1, selection.at(i).bottom());
  }

  const auto &lastSelection = selection.at(count - 1);
  points += lastSelection.topRight() + QPointF(margin + 1, 0);
  points += lastSelection.bottomRight() + QPointF(margin + 1, margin + extra);
  points += lastSelection.bottomLeft() + QPointF(-margin, margin + extra);
  points += lastSelection.topLeft() + QPointF(-margin, 0);

  for (auto i = count - 2; i > 0; --i) {
    auto x = std::min({selection.at(i - 1).left(), selection.at(i).left(), selection.at(i + 1).left()}) - margin;

    points += QPointF(x, selection.at(i).bottom() + extra);
    points += QPointF(x, selection.at(i).top());
  }

  points += firstSelection.bottomLeft() + QPointF(-margin, extra);
  points += firstSelection.topLeft() + QPointF(-margin, -margin);

  QPainterPath path;
  const auto corner = 4;
  path.moveTo(points.at(0));
  points += points.at(0);
  auto previous = points.at(0);
  for (auto i = 1; i < points.size(); ++i) {
    auto point = points.at(i);
    if (point.y() == previous.y() && qAbs(point.x() - previous.x()) > 2 * corner) {
      auto tmp = QPointF(previous.x() + corner * (point.x() > previous.x() ? 1 : -1), previous.y());
      path.quadTo(previous, tmp);
      previous = tmp;
      i--;
      continue;
    }
    if (point.x() == previous.x() && qAbs(point.y() - previous.y()) > 2 * corner) {
      auto tmp = QPointF(previous.x(), previous.y() + corner * (point.y() > previous.y() ? 1 : -1));
      path.quadTo(previous, tmp);
      previous = tmp;
      i--;
      continue;
    }

    auto target = (previous + point) / 2;
    path.quadTo(previous, target);
    previous = points.at(i);
  }
  path.closeSubpath();
  path.translate(offset);
  return path.simplified();
}

auto TextEditorOverlay::paintSelection(QPainter *painter, const OverlaySelection &selection) -> void
{
  auto begin = selection.m_cursor_begin;

  const auto &end = selection.m_cursor_end;
  const auto &fg = selection.m_fg;
  const auto &bg = selection.m_bg;

  if (begin.isNull() || end.isNull() || begin.position() > end.position() || !bg.isValid())
    return;

  auto path = createSelectionPath(begin, end, m_editor->viewport()->rect());

  painter->save();
  auto penColor = fg;
  if (m_alpha)
    penColor.setAlpha(220);
  QPen pen(penColor, m_borderWidth);
  painter->translate(-.5, -.5);

  auto pathRect = path.controlPointRect();

  if (!m_alpha || begin.blockNumber() != end.blockNumber()) {
    // gradients are too slow for larger selections :(
    auto col = bg;
    if (m_alpha)
      col.setAlpha(50);
    painter->setBrush(col);
  } else {
    QLinearGradient linearGrad(pathRect.topLeft(), pathRect.bottomLeft());
    auto col1 = fg.lighter(150);
    col1.setAlpha(20);
    auto col2 = fg;
    col2.setAlpha(80);
    linearGrad.setColorAt(0, col1);
    linearGrad.setColorAt(1, col2);
    painter->setBrush(QBrush(linearGrad));
  }

  painter->setRenderHint(QPainter::Antialiasing);

  if (selection.m_dropShadow) {
    painter->save();
    auto shadow = path;
    shadow.translate(m_dropShadowWidth, m_dropShadowWidth);
    QPainterPath clip;
    clip.addRect(m_editor->viewport()->rect());
    painter->setClipPath(clip - path);
    painter->fillPath(shadow, QColor(0, 0, 0, 100));
    painter->restore();
  }

  pen.setJoinStyle(Qt::RoundJoin);
  painter->setPen(pen);
  painter->drawPath(path);
  painter->restore();
}

auto TextEditorOverlay::fillSelection(QPainter *painter, const OverlaySelection &selection, const QColor &color) -> void
{
  const auto &begin = selection.m_cursor_begin;
  const auto &end = selection.m_cursor_end;
  if (begin.isNull() || end.isNull() || begin.position() > end.position())
    return;

  const auto path = createSelectionPath(begin, end, m_editor->viewport()->rect());

  painter->save();
  painter->translate(-.5, -.5);
  painter->setRenderHint(QPainter::Antialiasing);
  painter->fillPath(path, color);
  painter->restore();
}

auto TextEditorOverlay::paint(QPainter *painter, const QRect &clip) -> void
{
  Q_UNUSED(clip)
  for (int i = m_selections.size() - 1; i >= 0; --i) {
    const auto &selection = m_selections.at(i);
    if (selection.m_dropShadow)
      continue;
    if (selection.m_fixedLength >= 0 && selection.m_cursor_end.position() - selection.m_cursor_begin.position() != selection.m_fixedLength)
      continue;

    paintSelection(painter, selection);
  }
  for (int i = m_selections.size() - 1; i >= 0; --i) {
    const auto &selection = m_selections.at(i);
    if (!selection.m_dropShadow)
      continue;
    if (selection.m_fixedLength >= 0 && selection.m_cursor_end.position() - selection.m_cursor_begin.position() != selection.m_fixedLength)
      continue;

    paintSelection(painter, selection);
  }
}

auto TextEditorOverlay::cursorForSelection(const OverlaySelection &selection) const -> QTextCursor
{
  auto cursor = selection.m_cursor_begin;
  cursor.setPosition(selection.m_cursor_begin.position());
  cursor.setKeepPositionOnInsert(false);
  if (!cursor.isNull())
    cursor.setPosition(selection.m_cursor_end.position(), QTextCursor::KeepAnchor);
  return cursor;
}

auto TextEditorOverlay::cursorForIndex(int selectionIndex) const -> QTextCursor
{
  return cursorForSelection(m_selections.value(selectionIndex));
}

auto TextEditorOverlay::fill(QPainter *painter, const QColor &color, const QRect &clip) -> void
{
  Q_UNUSED(clip)
  for (int i = m_selections.size() - 1; i >= 0; --i) {
    const auto &selection = m_selections.at(i);
    if (selection.m_dropShadow)
      continue;
    if (selection.m_fixedLength >= 0 && selection.m_cursor_end.position() - selection.m_cursor_begin.position() != selection.m_fixedLength)
      continue;

    fillSelection(painter, selection, color);
  }
  for (int i = m_selections.size() - 1; i >= 0; --i) {
    const auto &selection = m_selections.at(i);
    if (!selection.m_dropShadow)
      continue;
    if (selection.m_fixedLength >= 0 && selection.m_cursor_end.position() - selection.m_cursor_begin.position() != selection.m_fixedLength)
      continue;

    fillSelection(painter, selection, color);
  }
}

auto TextEditorOverlay::hasFirstSelectionBeginMoved() const -> bool
{
  if (m_firstSelectionOriginalBegin == -1 || m_selections.isEmpty())
    return false;
  return m_selections.at(0).m_cursor_begin.position() != m_firstSelectionOriginalBegin;
}
