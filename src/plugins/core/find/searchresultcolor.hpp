// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>

#include <QColor>
#include <QHash>

namespace Core {

class CORE_EXPORT SearchResultColor {
public:
  enum class Style {
    Default,
    Alt1,
    Alt2
  };

  SearchResultColor() = default;

  SearchResultColor(const QColor &text_bg, const QColor &text_fg, const QColor &highlight_bg, const QColor &highlight_fg) : text_background(text_bg), text_foreground(text_fg), highlight_background(highlight_bg), highlight_foreground(highlight_fg)
  {
    if (!highlight_background.isValid())
      highlight_background = text_background;

    if (!highlight_foreground.isValid())
      highlight_foreground = text_foreground;
  }

  friend auto qHash(Style style)
  {
    return QT_PREPEND_NAMESPACE(qHash(static_cast<int>(style)));
  }

  QColor text_background;
  QColor text_foreground;
  QColor highlight_background;
  QColor highlight_foreground;
};

using search_result_colors = QHash<SearchResultColor::Style, SearchResultColor>;

} // namespace Core
