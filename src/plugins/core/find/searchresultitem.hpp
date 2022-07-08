// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "searchresultcolor.hpp"

#include <utils/fileutils.hpp>

#include <QIcon>
#include <QStringList>
#include <QVariant>

namespace Core {

namespace Search {

class TextPosition {
public:
  TextPosition() = default;
  TextPosition(const int line, const int column) : line(line), column(column) {}

  int line = -1;   // (0 or -1 for no line number)
  int column = -1; // 0-based starting position for a mark (-1 for no mark)

  auto operator<(const TextPosition &other) const -> bool { return line < other.line || (line == other.line && column < other.column); }
};

class TextRange {
public:
  TextRange() = default;
  TextRange(const TextPosition begin, const TextPosition end) : begin(begin), end(end) {}

  auto mid(const QString &text) const -> QString { return text.mid(begin.column, length(text)); }

  auto length(const QString &text) const -> int
  {
    if (begin.line == end.line)
      return end.column - begin.column;

    const auto line_count = end.line - begin.line;
    int index = text.indexOf(QChar::LineFeed);
    auto current_line = 1;

    while (index > 0 && current_line < line_count) {
      ++index;
      index = text.indexOf(QChar::LineFeed, index);
      ++current_line;
    }

    if (index < 0)
      return 0;

    return index - begin.column + end.column;
  }

  TextPosition begin;
  TextPosition end;

  auto operator<(const TextRange &other) const -> bool { return begin < other.begin; }
};

} // namespace Search

class CORE_EXPORT SearchResultItem {
public:
  auto path() const -> QStringList { return m_path; }
  auto setPath(const QStringList &path) -> void { m_path = path; }

  auto setFilePath(const Utils::FilePath &file_path) -> void
  {
    m_path = QStringList{file_path.toUserOutput()};
  }

  auto lineText() const -> QString { return m_line_text; }
  auto setLineText(const QString &text) -> void { m_line_text = text; }
  auto icon() const -> QIcon { return m_icon; }
  auto setIcon(const QIcon &icon) -> void { m_icon = icon; }
  auto userData() const -> QVariant { return m_user_data; }
  auto setUserData(const QVariant &user_data) -> void { m_user_data = user_data; }
  auto mainRange() const -> Search::TextRange { return m_main_range; }
  auto setMainRange(const Search::TextRange &main_range) -> void { m_main_range = main_range; }

  auto setMainRange(const int line, const int column, const int length) -> void
  {
    m_main_range = {};
    m_main_range.begin.line = line;
    m_main_range.begin.column = column;
    m_main_range.end.line = m_main_range.begin.line;
    m_main_range.end.column = m_main_range.begin.column + length;
  }

  auto useTextEditorFont() const -> bool { return m_use_text_editor_font; }
  auto setUseTextEditorFont(const bool use_text_editor_font) -> void { m_use_text_editor_font = use_text_editor_font; }
  auto style() const -> SearchResultColor::Style { return m_style; }
  auto setStyle(const SearchResultColor::Style style) -> void { m_style = style; }
  auto selectForReplacement() const -> bool { return m_select_for_replacement; }
  auto setSelectForReplacement(const bool select) -> void { m_select_for_replacement = select; }

private:
  QStringList m_path;  // hierarchy to the parent item of this item
  QString m_line_text;  // text to Show for the item itself
  QIcon m_icon;        // icon to Show in front of the item (by be null icon to Hide)
  QVariant m_user_data; // user data for identification of the item
  Search::TextRange m_main_range;
  bool m_use_text_editor_font = false;
  bool m_select_for_replacement = true;
  SearchResultColor::Style m_style = SearchResultColor::Style::Default;
};

} // namespace Core

Q_DECLARE_METATYPE(Core::SearchResultItem)
Q_DECLARE_METATYPE(Core::Search::TextPosition)
