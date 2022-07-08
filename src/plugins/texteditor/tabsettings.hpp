// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <QTextBlock>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

// Tab settings: Data type the GeneralSettingsPage acts on
// with some convenience functions for formatting.
class TEXTEDITOR_EXPORT TabSettings {
public:
  enum TabPolicy {
    SpacesOnlyTabPolicy = 0,
    TabsOnlyTabPolicy = 1,
    MixedTabPolicy = 2
  };

  // This enum must match the indexes of continuationAlignBehavior widget
  enum ContinuationAlignBehavior {
    NoContinuationAlign = 0,
    ContinuationAlignWithSpaces = 1,
    ContinuationAlignWithIndent = 2
  };

  TabSettings() = default;
  TabSettings(TabPolicy tabPolicy, int tabSize, int indentSize, ContinuationAlignBehavior continuationAlignBehavior);

  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, QSettings *s) -> void;
  auto toMap() const -> QVariantMap;
  auto fromMap(const QVariantMap &map) -> void;
  auto lineIndentPosition(const QString &text) const -> int;
  auto columnAt(const QString &text, int position) const -> int;
  auto columnAtCursorPosition(const QTextCursor &cursor) const -> int;
  auto positionAtColumn(const QString &text, int column, int *offset = nullptr, bool allowOverstep = false) const -> int;
  auto columnCountForText(const QString &text, int startColumn = 0) const -> int;
  auto indentedColumn(int column, bool doIndent = true) const -> int;
  auto indentationString(int startColumn, int targetColumn, int padding, const QTextBlock &currentBlock = QTextBlock()) const -> QString;
  auto indentationString(const QString &text) const -> QString;
  auto indentationColumn(const QString &text) const -> int;

  static auto maximumPadding(const QString &text) -> int;

  auto indentLine(const QTextBlock &block, int newIndent, int padding = 0) const -> void;
  auto reindentLine(QTextBlock block, int delta) const -> void;
  auto isIndentationClean(const QTextBlock &block, const int indent) const -> bool;
  auto guessSpacesForTabs(const QTextBlock &block) const -> bool;

  friend auto operator==(const TabSettings &t1, const TabSettings &t2) -> bool { return t1.equals(t2); }
  friend auto operator!=(const TabSettings &t1, const TabSettings &t2) -> bool { return !t1.equals(t2); }

  static auto firstNonSpace(const QString &text) -> int;
  static auto onlySpace(const QString &text) -> bool { return firstNonSpace(text) == text.length(); }
  static auto spacesLeftFromPosition(const QString &text, int position) -> int;
  static auto cursorIsAtBeginningOfLine(const QTextCursor &cursor) -> bool;
  static auto trailingWhitespaces(const QString &text) -> int;
  static auto removeTrailingWhitespace(QTextCursor cursor, QTextBlock &block) -> void;

  TabPolicy m_tabPolicy = SpacesOnlyTabPolicy;
  int m_tabSize = 8;
  int m_indentSize = 4;
  ContinuationAlignBehavior m_continuationAlignBehavior = ContinuationAlignWithSpaces;

  auto equals(const TabSettings &ts) const -> bool;
};

} // namespace TextEditor

Q_DECLARE_METATYPE(TextEditor::TabSettings)
Q_DECLARE_METATYPE(TextEditor::TabSettings::TabPolicy)
Q_DECLARE_METATYPE(TextEditor::TabSettings::ContinuationAlignBehavior)
