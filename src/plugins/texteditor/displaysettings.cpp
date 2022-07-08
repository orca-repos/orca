// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "displaysettings.hpp"

#include "texteditorconstants.hpp"

#include <core/icore.hpp>
#include <utils/tooltip/tooltip.hpp>

#include <QLabel>
#include <QSettings>
#include <QString>

static constexpr char displayLineNumbersKey[] = "DisplayLineNumbers";
static constexpr char textWrappingKey[] = "TextWrapping";
static constexpr char visualizeWhitespaceKey[] = "VisualizeWhitespace";
static constexpr char displayFoldingMarkersKey[] = "DisplayFoldingMarkers";
static constexpr char highlightCurrentLineKey[] = "HighlightCurrentLine2Key";
static constexpr char highlightBlocksKey[] = "HighlightBlocksKey";
static constexpr char animateMatchingParenthesesKey[] = "AnimateMatchingParenthesesKey";
static constexpr char highlightMatchingParenthesesKey[] = "HightlightMatchingParenthesesKey";
static constexpr char markTextChangesKey[] = "MarkTextChanges";
static constexpr char autoFoldFirstCommentKey[] = "AutoFoldFirstComment";
static constexpr char centerCursorOnScrollKey[] = "CenterCursorOnScroll";
static constexpr char openLinksInNextSplitKey[] = "OpenLinksInNextSplitKey";
static constexpr char displayFileEncodingKey[] = "DisplayFileEncoding";
static constexpr char scrollBarHighlightsKey[] = "ScrollBarHighlights";
static constexpr char animateNavigationWithinFileKey[] = "AnimateNavigationWithinFile";
static constexpr char animateWithinFileTimeMaxKey[] = "AnimateWithinFileTimeMax";
static constexpr char displayAnnotationsKey[] = "DisplayAnnotations";
static constexpr char annotationAlignmentKey[] = "AnnotationAlignment";
static constexpr char minimalAnnotationContentKey[] = "MinimalAnnotationContent";
static constexpr char groupPostfix[] = "DisplaySettings";

namespace TextEditor {

auto DisplaySettings::toSettings(const QString &category, QSettings *s) const -> void
{
  QString group = QLatin1String(groupPostfix);
  if (!category.isEmpty())
    group.insert(0, category);
  s->beginGroup(group);
  s->setValue(QLatin1String(displayLineNumbersKey), m_displayLineNumbers);
  s->setValue(QLatin1String(textWrappingKey), m_textWrapping);
  s->setValue(QLatin1String(visualizeWhitespaceKey), m_visualizeWhitespace);
  s->setValue(QLatin1String(displayFoldingMarkersKey), m_displayFoldingMarkers);
  s->setValue(QLatin1String(highlightCurrentLineKey), m_highlightCurrentLine);
  s->setValue(QLatin1String(highlightBlocksKey), m_highlightBlocks);
  s->setValue(QLatin1String(animateMatchingParenthesesKey), m_animateMatchingParentheses);
  s->setValue(QLatin1String(highlightMatchingParenthesesKey), m_highlightMatchingParentheses);
  s->setValue(QLatin1String(markTextChangesKey), m_markTextChanges);
  s->setValue(QLatin1String(autoFoldFirstCommentKey), m_autoFoldFirstComment);
  s->setValue(QLatin1String(centerCursorOnScrollKey), m_centerCursorOnScroll);
  s->setValue(QLatin1String(openLinksInNextSplitKey), m_openLinksInNextSplit);
  s->setValue(QLatin1String(displayFileEncodingKey), m_displayFileEncoding);
  s->setValue(QLatin1String(scrollBarHighlightsKey), m_scrollBarHighlights);
  s->setValue(QLatin1String(animateNavigationWithinFileKey), m_animateNavigationWithinFile);
  s->setValue(QLatin1String(displayAnnotationsKey), m_displayAnnotations);
  s->setValue(QLatin1String(annotationAlignmentKey), static_cast<int>(m_annotationAlignment));
  s->endGroup();
}

auto DisplaySettings::fromSettings(const QString &category, const QSettings *s) -> void
{
  QString group = QLatin1String(groupPostfix);
  if (!category.isEmpty())
    group.insert(0, category);
  group += QLatin1Char('/');

  *this = DisplaySettings(); // Assign defaults

  m_displayLineNumbers = s->value(group + QLatin1String(displayLineNumbersKey), m_displayLineNumbers).toBool();
  m_textWrapping = s->value(group + QLatin1String(textWrappingKey), m_textWrapping).toBool();
  m_visualizeWhitespace = s->value(group + QLatin1String(visualizeWhitespaceKey), m_visualizeWhitespace).toBool();
  m_displayFoldingMarkers = s->value(group + QLatin1String(displayFoldingMarkersKey), m_displayFoldingMarkers).toBool();
  m_highlightCurrentLine = s->value(group + QLatin1String(highlightCurrentLineKey), m_highlightCurrentLine).toBool();
  m_highlightBlocks = s->value(group + QLatin1String(highlightBlocksKey), m_highlightBlocks).toBool();
  m_animateMatchingParentheses = s->value(group + QLatin1String(animateMatchingParenthesesKey), m_animateMatchingParentheses).toBool();
  m_highlightMatchingParentheses = s->value(group + QLatin1String(highlightMatchingParenthesesKey), m_highlightMatchingParentheses).toBool();
  m_markTextChanges = s->value(group + QLatin1String(markTextChangesKey), m_markTextChanges).toBool();
  m_autoFoldFirstComment = s->value(group + QLatin1String(autoFoldFirstCommentKey), m_autoFoldFirstComment).toBool();
  m_centerCursorOnScroll = s->value(group + QLatin1String(centerCursorOnScrollKey), m_centerCursorOnScroll).toBool();
  m_openLinksInNextSplit = s->value(group + QLatin1String(openLinksInNextSplitKey), m_openLinksInNextSplit).toBool();
  m_displayFileEncoding = s->value(group + QLatin1String(displayFileEncodingKey), m_displayFileEncoding).toBool();
  m_scrollBarHighlights = s->value(group + QLatin1String(scrollBarHighlightsKey), m_scrollBarHighlights).toBool();
  m_animateNavigationWithinFile = s->value(group + QLatin1String(animateNavigationWithinFileKey), m_animateNavigationWithinFile).toBool();
  m_animateWithinFileTimeMax = s->value(group + QLatin1String(animateWithinFileTimeMaxKey), m_animateWithinFileTimeMax).toInt();
  m_displayAnnotations = s->value(group + QLatin1String(displayAnnotationsKey), m_displayAnnotations).toBool();
  m_annotationAlignment = static_cast<AnnotationAlignment>(s->value(group + QLatin1String(annotationAlignmentKey), static_cast<int>(m_annotationAlignment)).toInt());
  m_minimalAnnotationContent = s->value(group + QLatin1String(minimalAnnotationContentKey), m_minimalAnnotationContent).toInt();
}

auto DisplaySettings::equals(const DisplaySettings &ds) const -> bool
{
  return m_displayLineNumbers == ds.m_displayLineNumbers && m_textWrapping == ds.m_textWrapping && m_visualizeWhitespace == ds.m_visualizeWhitespace && m_displayFoldingMarkers == ds.m_displayFoldingMarkers && m_highlightCurrentLine == ds.m_highlightCurrentLine && m_highlightBlocks == ds.m_highlightBlocks && m_animateMatchingParentheses == ds.m_animateMatchingParentheses && m_highlightMatchingParentheses == ds.m_highlightMatchingParentheses && m_markTextChanges == ds.m_markTextChanges && m_autoFoldFirstComment == ds.m_autoFoldFirstComment && m_centerCursorOnScroll == ds.m_centerCursorOnScroll && m_openLinksInNextSplit == ds.m_openLinksInNextSplit && m_forceOpenLinksInNextSplit == ds.m_forceOpenLinksInNextSplit && m_displayFileEncoding == ds.m_displayFileEncoding && m_scrollBarHighlights == ds.m_scrollBarHighlights && m_animateNavigationWithinFile == ds.m_animateNavigationWithinFile && m_animateWithinFileTimeMax == ds.m_animateWithinFileTimeMax && m_displayAnnotations == ds.m_displayAnnotations && m_annotationAlignment == ds.m_annotationAlignment && m_minimalAnnotationContent == ds.m_minimalAnnotationContent;
}

auto DisplaySettings::createAnnotationSettingsLink() -> QLabel*
{
  const auto label = new QLabel("<small><i><a href>Annotation Settings</a></i></small>");
  QObject::connect(label, &QLabel::linkActivated, []() {
    Utils::ToolTip::hideImmediately();
    Core::ICore::showOptionsDialog(Constants::TEXT_EDITOR_DISPLAY_SETTINGS);
  });
  return label;
}

} // namespace TextEditor
