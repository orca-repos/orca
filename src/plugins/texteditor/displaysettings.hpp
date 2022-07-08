// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "QMetaType"

QT_BEGIN_NAMESPACE
class QSettings;
class QLabel;
QT_END_NAMESPACE

namespace TextEditor {

enum class AnnotationAlignment {
  NextToContent,
  NextToMargin,
  RightSide,
  BetweenLines
};

class TEXTEDITOR_EXPORT DisplaySettings {
public:
  DisplaySettings() = default;

  auto toSettings(const QString &category, QSettings *s) const -> void;
  auto fromSettings(const QString &category, const QSettings *s) -> void;
  auto equals(const DisplaySettings &ds) const -> bool;
  static auto createAnnotationSettingsLink() -> QLabel*;

  friend auto operator==(const DisplaySettings &t1, const DisplaySettings &t2) -> bool { return t1.equals(t2); }
  friend auto operator!=(const DisplaySettings &t1, const DisplaySettings &t2) -> bool { return !t1.equals(t2); }

  bool m_displayLineNumbers = true;
  bool m_textWrapping = false;
  bool m_visualizeWhitespace = false;
  bool m_displayFoldingMarkers = true;
  bool m_highlightCurrentLine = false;
  bool m_highlightBlocks = false;
  bool m_animateMatchingParentheses = true;
  bool m_highlightMatchingParentheses = true;
  bool m_markTextChanges = true;
  bool m_autoFoldFirstComment = true;
  bool m_centerCursorOnScroll = false;
  bool m_openLinksInNextSplit = false;
  bool m_forceOpenLinksInNextSplit = false;
  bool m_displayFileEncoding = false;
  bool m_scrollBarHighlights = true;
  bool m_animateNavigationWithinFile = false;
  int m_animateWithinFileTimeMax = 333; // read only setting
  bool m_displayAnnotations = true;
  AnnotationAlignment m_annotationAlignment = AnnotationAlignment::RightSide;
  int m_minimalAnnotationContent = 15;
};

} // namespace TextEditor

Q_DECLARE_METATYPE(TextEditor::AnnotationAlignment)
