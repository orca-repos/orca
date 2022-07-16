// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "tabsettings.hpp"

#include <utils/settingsutils.hpp>

#include <QDebug>
#include <QSettings>
#include <QString>
#include <QTextCursor>
#include <QTextDocument>

static constexpr char spacesForTabsKey[] = "SpacesForTabs";
static constexpr char autoSpacesForTabsKey[] = "AutoSpacesForTabs";
static constexpr char tabSizeKey[] = "TabSize";
static constexpr char indentSizeKey[] = "IndentSize";
static constexpr char groupPostfix[] = "TabSettings";
static constexpr char paddingModeKey[] = "PaddingMode";

namespace TextEditor {

TabSettings::TabSettings(TabPolicy tabPolicy, int tabSize, int indentSize, ContinuationAlignBehavior continuationAlignBehavior) : m_tabPolicy(tabPolicy), m_tabSize(tabSize), m_indentSize(indentSize), m_continuationAlignBehavior(continuationAlignBehavior) {}

auto TabSettings::toSettings(const QString &category, QSettings *s) const -> void
{
  Utils::toSettings(QLatin1String(groupPostfix), category, s, this);
}

auto TabSettings::fromSettings(const QString &category, QSettings *s) -> void
{
  *this = TabSettings(); // Assign defaults
  Utils::fromSettings(QLatin1String(groupPostfix), category, s, this);
}

auto TabSettings::toMap() const -> QVariantMap
{
  return {{spacesForTabsKey, m_tabPolicy != TabsOnlyTabPolicy}, {autoSpacesForTabsKey, m_tabPolicy == MixedTabPolicy}, {tabSizeKey, m_tabSize}, {indentSizeKey, m_indentSize}, {paddingModeKey, m_continuationAlignBehavior}};
}

auto TabSettings::fromMap(const QVariantMap &map) -> void
{
  const auto spacesForTabs = map.value(spacesForTabsKey, true).toBool();
  const auto autoSpacesForTabs = map.value(autoSpacesForTabsKey, false).toBool();
  m_tabPolicy = spacesForTabs ? (autoSpacesForTabs ? MixedTabPolicy : SpacesOnlyTabPolicy) : TabsOnlyTabPolicy;
  m_tabSize = map.value(tabSizeKey, m_tabSize).toInt();
  m_indentSize = map.value(indentSizeKey, m_indentSize).toInt();
  m_continuationAlignBehavior = (ContinuationAlignBehavior)map.value(paddingModeKey, m_continuationAlignBehavior).toInt();
}

auto TabSettings::cursorIsAtBeginningOfLine(const QTextCursor &cursor) -> bool
{
  const auto text = cursor.block().text();
  const auto fns = firstNonSpace(text);
  return cursor.position() - cursor.block().position() <= fns;
}

auto TabSettings::lineIndentPosition(const QString &text) const -> int
{
  auto i = 0;
  while (i < text.size()) {
    if (!text.at(i).isSpace())
      break;
    ++i;
  }
  const auto column = columnAt(text, i);
  return i - column % m_indentSize;
}

auto TabSettings::firstNonSpace(const QString &text) -> int
{
  auto i = 0;
  while (i < text.size()) {
    if (!text.at(i).isSpace())
      return i;
    ++i;
  }
  return i;
}

auto TabSettings::indentationString(const QString &text) const -> QString
{
  return text.left(firstNonSpace(text));
}

auto TabSettings::indentationColumn(const QString &text) const -> int
{
  return columnAt(text, firstNonSpace(text));
}

auto TabSettings::maximumPadding(const QString &text) -> int
{
  const auto fns = firstNonSpace(text);
  auto i = fns;
  while (i > 0) {
    if (text.at(i - 1) != QLatin1Char(' '))
      break;
    --i;
  }
  return fns - i;
}

auto TabSettings::trailingWhitespaces(const QString &text) -> int
{
  auto i = 0;
  while (i < text.size()) {
    if (!text.at(text.size() - 1 - i).isSpace())
      return i;
    ++i;
  }
  return i;
}

auto TabSettings::removeTrailingWhitespace(QTextCursor cursor, QTextBlock &block) -> void
{
  if (const auto trailing = trailingWhitespaces(block.text())) {
    cursor.setPosition(block.position() + block.length() - 1);
    cursor.movePosition(QTextCursor::PreviousCharacter, QTextCursor::KeepAnchor, trailing);
    cursor.removeSelectedText();
  }
}

auto TabSettings::isIndentationClean(const QTextBlock &block, const int indent) const -> bool
{
  auto i = 0;
  auto spaceCount = 0;
  const auto text = block.text();
  const auto spacesForTabs = guessSpacesForTabs(block);
  while (i < text.size()) {
    auto c = text.at(i);
    if (!c.isSpace())
      return true;

    if (c == QLatin1Char(' ')) {
      ++spaceCount;
      if (spaceCount == m_tabSize)
        if (!spacesForTabs)
          if (m_continuationAlignBehavior != ContinuationAlignWithSpaces || i < indent)
            return false;
      if (spaceCount > indent && m_continuationAlignBehavior == NoContinuationAlign)
        return false;
    } else if (c == QLatin1Char('\t')) {
      if (spacesForTabs || spaceCount != 0)
        return false;
      if (m_continuationAlignBehavior != ContinuationAlignWithIndent && (i + 1) * m_tabSize > indent)
        return false;
    }
    ++i;
  }
  return true;
}

auto TabSettings::columnAt(const QString &text, int position) const -> int
{
  auto column = 0;
  for (auto i = 0; i < position; ++i) {
    if (text.at(i) == QLatin1Char('\t'))
      column = column - column % m_tabSize + m_tabSize;
    else
      ++column;
  }
  return column;
}

auto TabSettings::columnAtCursorPosition(const QTextCursor &cursor) const -> int
{
  return columnAt(cursor.block().text(), cursor.positionInBlock());
}

auto TabSettings::positionAtColumn(const QString &text, int column, int *offset, bool allowOverstep) const -> int
{
  auto col = 0;
  auto i = 0;
  const int textSize = text.size();
  while ((i < textSize || allowOverstep) && col < column) {
    if (i < textSize && text.at(i) == QLatin1Char('\t'))
      col = col - col % m_tabSize + m_tabSize;
    else
      ++col;
    ++i;
  }
  if (offset)
    *offset = column - col;
  return i;
}

auto TabSettings::columnCountForText(const QString &text, int startColumn) const -> int
{
  auto column = startColumn;
  for (const auto c : text) {
    if (c == QLatin1Char('\t'))
      column = column - column % m_tabSize + m_tabSize;
    else
      ++column;
  }
  return column - startColumn;
}

auto TabSettings::spacesLeftFromPosition(const QString &text, int position) -> int
{
  if (position > text.size())
    return 0;
  auto i = position;
  while (i > 0) {
    if (!text.at(i - 1).isSpace())
      break;
    --i;
  }
  return position - i;
}

auto TabSettings::indentedColumn(int column, bool doIndent) const -> int
{
  const auto aligned = column / m_indentSize * m_indentSize;
  if (doIndent)
    return aligned + m_indentSize;
  if (aligned < column)
    return aligned;
  return qMax(0, aligned - m_indentSize);
}

auto TabSettings::guessSpacesForTabs(const QTextBlock &_block) const -> bool
{
  if (m_tabPolicy == MixedTabPolicy && _block.isValid()) {
    const auto doc = _block.document();
    QVector<QTextBlock> currentBlocks(2, _block); // [0] looks back; [1] looks forward
    auto maxLookAround = 100;
    while (maxLookAround-- > 0) {
      if (currentBlocks.at(0).isValid())
        currentBlocks[0] = currentBlocks.at(0).previous();
      if (currentBlocks.at(1).isValid())
        currentBlocks[1] = currentBlocks.at(1).next();
      auto done = true;
      foreach(const QTextBlock &block, currentBlocks) {
        if (block.isValid())
          done = false;
        if (!block.isValid() || block.length() == 0)
          continue;
        const auto firstChar = doc->characterAt(block.position());
        if (firstChar == QLatin1Char(' '))
          return true;
        if (firstChar == QLatin1Char('\t'))
          return false;
      }
      if (done)
        break;
    }
  }
  return m_tabPolicy != TabsOnlyTabPolicy;
}

auto TabSettings::indentationString(int startColumn, int targetColumn, int padding, const QTextBlock &block) const -> QString
{
  targetColumn = qMax(startColumn, targetColumn);
  if (guessSpacesForTabs(block))
    return QString(targetColumn - startColumn, QLatin1Char(' '));

  QString s;
  const auto alignedStart = startColumn == 0 ? 0 : startColumn - startColumn % m_tabSize + m_tabSize;
  if (alignedStart > startColumn && alignedStart <= targetColumn) {
    s += QLatin1Char('\t');
    startColumn = alignedStart;
  }
  if (m_continuationAlignBehavior == NoContinuationAlign) {
    targetColumn -= padding;
    padding = 0;
  } else if (m_continuationAlignBehavior == ContinuationAlignWithIndent) {
    padding = 0;
  }
  const auto columns = targetColumn - padding - startColumn;
  const auto tabs = columns / m_tabSize;
  s += QString(tabs, QLatin1Char('\t'));
  s += QString(targetColumn - startColumn - tabs * m_tabSize, QLatin1Char(' '));
  return s;
}

auto TabSettings::indentLine(const QTextBlock &block, int newIndent, int padding) const -> void
{
  const auto text = block.text();
  const int oldBlockLength = text.size();

  if (m_continuationAlignBehavior == NoContinuationAlign) {
    newIndent -= padding;
    padding = 0;
  } else if (m_continuationAlignBehavior == ContinuationAlignWithIndent) {
    padding = 0;
  }

  // Quickly check whether indenting is required.
  // fixme: after changing "use spaces for tabs" the change was not reflected
  // because of the following optimisation. Commenting it out for now.
  //    if (indentationColumn(text) == newIndent)
  //        return;

  const auto indentString = indentationString(0, newIndent, padding, block);

  if (oldBlockLength == indentString.length() && text == indentString)
    return;

  QTextCursor cursor(block);
  cursor.beginEditBlock();
  cursor.movePosition(QTextCursor::StartOfBlock);
  cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, firstNonSpace(text));
  cursor.removeSelectedText();
  cursor.insertText(indentString);
  cursor.endEditBlock();
}

auto TabSettings::reindentLine(QTextBlock block, int delta) const -> void
{
  const auto text = block.text();
  const int oldBlockLength = text.size();

  const auto oldIndent = indentationColumn(text);
  const auto newIndent = qMax(oldIndent + delta, 0);

  if (oldIndent == newIndent)
    return;

  auto padding = 0;
  // user likes tabs for spaces and uses tabs for indentation, preserve padding
  if (m_tabPolicy == TabsOnlyTabPolicy && m_tabSize == m_indentSize)
    padding = qMin(maximumPadding(text), newIndent);
  const auto indentString = indentationString(0, newIndent, padding, block);

  if (oldBlockLength == indentString.length() && text == indentString)
    return;

  QTextCursor cursor(block);
  cursor.beginEditBlock();
  cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, firstNonSpace(text));
  cursor.removeSelectedText();
  cursor.insertText(indentString);
  cursor.endEditBlock();
}

auto TabSettings::equals(const TabSettings &ts) const -> bool
{
  return m_tabPolicy == ts.m_tabPolicy && m_tabSize == ts.m_tabSize && m_indentSize == ts.m_indentSize && m_continuationAlignBehavior == ts.m_continuationAlignBehavior;
}

} // namespace TextEditor
