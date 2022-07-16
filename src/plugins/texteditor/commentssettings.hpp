// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace TextEditor {

class TEXTEDITOR_EXPORT CommentsSettings {
public:
  CommentsSettings();

  auto toSettings(QSettings *s) const -> void;
  auto fromSettings(QSettings *s) -> void;
  auto equals(const CommentsSettings &other) const -> bool;

  friend auto operator==(const CommentsSettings &a, const CommentsSettings &b) -> bool { return a.equals(b); }
  friend auto operator!=(const CommentsSettings &a, const CommentsSettings &b) -> bool { return !(a == b); }

  bool m_enableDoxygen;
  bool m_generateBrief;
  bool m_leadingAsterisks;
};

} // namespace TextEditor
