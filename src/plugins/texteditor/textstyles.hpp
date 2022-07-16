// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditorconstants.hpp"

#include <utils/sizedarray.hpp>

#include <QList>

namespace TextEditor {

using MixinTextStyles = Utils::SizedArray<TextStyle, 6>;

struct TextStyles {
  TextStyle mainStyle = C_TEXT;
  MixinTextStyles mixinStyles;

  static auto mixinStyle(TextStyle main, const QList<TextStyle> &mixins) -> TextStyles
  {
    TextStyles res;
    res.mainStyle = main;
    res.mixinStyles.initializeElements();
    for (auto mixin : mixins)
      res.mixinStyles.push_back(mixin);
    return res;
  }

  static auto mixinStyle(TextStyle main, TextStyle mixin) -> TextStyles
  {
    return mixinStyle(main, QList{mixin});
  }
};

} // namespace TextEditor
