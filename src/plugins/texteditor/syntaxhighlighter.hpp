// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <texteditor/texteditorconstants.hpp>

#include <QObject>
#include <QTextLayout>

#include <functional>
#include <climits>

QT_BEGIN_NAMESPACE
class QTextDocument;
class QSyntaxHighlighterPrivate;
class QTextCharFormat;
class QFont;
class QColor;
class QTextBlockUserData;
class QTextEdit;
QT_END_NAMESPACE namespace TextEditor {

class FontSettings;
class SyntaxHighlighterPrivate;

class TEXTEDITOR_EXPORT SyntaxHighlighter : public QObject {
  Q_OBJECT Q_DECLARE_PRIVATE(SyntaxHighlighter)

public:
  SyntaxHighlighter(QObject *parent = nullptr);
  SyntaxHighlighter(QTextDocument *parent);
  SyntaxHighlighter(QTextEdit *parent);
  ~SyntaxHighlighter() override;

  auto setDocument(QTextDocument *doc) -> void;
  auto document() const -> QTextDocument*;
  auto setExtraFormats(const QTextBlock &block, QVector<QTextLayout::FormatRange> &&formats) -> void;
  auto clearExtraFormats(const QTextBlock &block) -> void;
  auto clearAllExtraFormats() -> void;
  static auto generateColors(int n, const QColor &background) -> QList<QColor>;

  // Don't call in constructors of derived classes
  virtual auto setFontSettings(const FontSettings &fontSettings) -> void;
  auto fontSettings() const -> FontSettings;
  auto setNoAutomaticHighlighting(bool noAutomatic) -> void;

public slots:
  auto rehighlight() -> void;
  auto rehighlightBlock(const QTextBlock &block) -> void;

protected:
  auto setDefaultTextFormatCategories() -> void;
  auto setTextFormatCategories(int count, std::function<TextStyle(int)> formatMapping) -> void;
  auto formatForCategory(int categoryIndex) const -> QTextCharFormat;

  // implement in subclasses
  // default implementation highlights whitespace
  virtual auto highlightBlock(const QString &text) -> void;

  auto setFormat(int start, int count, const QTextCharFormat &format) -> void;
  auto setFormat(int start, int count, const QColor &color) -> void;
  auto setFormat(int start, int count, const QFont &font) -> void;
  auto format(int pos) const -> QTextCharFormat;
  auto formatSpaces(const QString &text, int start = 0, int count = INT_MAX) -> void;
  auto setFormatWithSpaces(const QString &text, int start, int count, const QTextCharFormat &format) -> void;
  auto previousBlockState() const -> int;
  auto currentBlockState() const -> int;
  auto setCurrentBlockState(int newState) -> void;
  auto setCurrentBlockUserData(QTextBlockUserData *data) -> void;
  auto currentBlockUserData() const -> QTextBlockUserData*;
  auto currentBlock() const -> QTextBlock;

private:
  auto setTextFormatCategories(const QVector<std::pair<int, TextStyle>> &categories) -> void;
  auto reformatBlocks(int from, int charsRemoved, int charsAdded) -> void;
  auto delayedRehighlight() -> void;

  QScopedPointer<SyntaxHighlighterPrivate> d_ptr;
};

} // namespace TextEditor
