// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include "texteditor_global.hpp"

#include "indenter.hpp"
#include "tabsettings.hpp"

QT_BEGIN_NAMESPACE
class QTextDocument;
class QTextCursor;
class QChar;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT TextIndenter : public Indenter {
public:
  explicit TextIndenter(QTextDocument *doc);
  ~TextIndenter() override;

  auto indentFor(const QTextBlock &block, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> int override;
  auto indentationForBlocks(const QVector<QTextBlock> &blocks, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> IndentationForBlock override;
  auto indentBlock(const QTextBlock &block, const QChar &typedChar, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void override;
  auto indent(const QTextCursor &cursor, const QChar &typedChar, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void override;
  auto reindent(const QTextCursor &cursor, const TabSettings &tabSettings, int cursorPositionInEditor = -1) -> void override;
  auto tabSettings() const -> Utils::optional<TabSettings> override;
};

} // namespace TextEditor
