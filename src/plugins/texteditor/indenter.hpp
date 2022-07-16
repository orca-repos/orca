// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/fileutils.hpp>
#include <utils/optional.hpp>
#include <utils/textutils.hpp>

#include <QMap>
#include <QTextBlock>
#include <vector>

namespace Utils {
class FilePath;
}

namespace TextEditor {

class ICodeStylePreferences;
class TabSettings;

using IndentationForBlock = QMap<int, int>;

class RangeInLines {
public:
  int startLine;
  int endLine;
};

using RangesInLines = std::vector<RangeInLines>;

class Indenter {
public:
  explicit Indenter(QTextDocument *doc) : m_doc(doc) {}
  virtual ~Indenter() = default;

  auto setFileName(const Utils::FilePath &fileName) -> void { m_fileName = fileName; }
  // Returns true if key triggers an indent.
  virtual auto isElectricCharacter(const QChar & /*ch*/) const -> bool { return false; }
  virtual auto setCodeStylePreferences(ICodeStylePreferences * /*preferences*/) -> void {}
  virtual auto invalidateCache() -> void {}

  virtual auto indentFor(const QTextBlock & /*block*/, const TabSettings & /*tabSettings*/, int /*cursorPositionInEditor*/  = -1) -> int
  {
    return -1;
  }

  virtual auto autoIndent(const QTextCursor &cursor, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void
  {
    indent(cursor, QChar::Null, tabSettings, cursorPositionInEditor);
  }

  virtual auto format(const RangesInLines & /*rangesInLines*/) -> Utils::Text::Replacements
  {
    return Utils::Text::Replacements();
  }

  virtual auto formatOnSave() const -> bool { return false; }

  // Expects a list of blocks in order of occurrence in the document.
  virtual auto indentationForBlocks(const QVector<QTextBlock> &blocks, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> IndentationForBlock = 0;
  virtual auto tabSettings() const -> Utils::optional<TabSettings> = 0;
  // Indent a text block based on previous line. Default does nothing
  virtual auto indentBlock(const QTextBlock &block, const QChar &typedChar, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void = 0;
  // Indent at cursor. Calls indentBlock for selection or current line.
  virtual auto indent(const QTextCursor &cursor, const QChar &typedChar, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void = 0;
  // Reindent at cursor. Selection will be adjusted according to the indentation
  // change of the first block.
  virtual auto reindent(const QTextCursor &cursor, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void = 0;
  virtual auto margin() const -> Utils::optional<int> { return Utils::nullopt; }

protected:
  QTextDocument *m_doc;
  Utils::FilePath m_fileName;
};

} // namespace TextEditor
