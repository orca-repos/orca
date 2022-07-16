// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "refactoroverlay.hpp"
#include "textdocumentlayout.hpp"
#include "texteditor.hpp"

#include <utils/algorithm.hpp>
#include <utils/utilsicons.hpp>

#include <QPainter>

namespace TextEditor {

RefactorOverlay::RefactorOverlay(TextEditorWidget *editor) : QObject(editor), m_editor(editor), m_maxWidth(0), m_icon(Utils::Icons::CODEMODEL_FIXIT.icon()) {}

auto RefactorOverlay::paint(QPainter *painter, const QRect &clip) -> void
{
  m_maxWidth = 0;
  for (auto &marker : qAsConst(m_markers)) {
    paintMarker(marker, painter, clip);
  }

  if (const auto documentLayout = qobject_cast<TextDocumentLayout*>(m_editor->document()->documentLayout()))
    documentLayout->setRequiredWidth(m_maxWidth);
}

auto RefactorOverlay::markerAt(const QPoint &pos) const -> RefactorMarker
{
  for (const auto &marker : m_markers) {
    if (marker.rect.contains(pos))
      return marker;
  }
  return RefactorMarker();
}

auto RefactorOverlay::paintMarker(const RefactorMarker &marker, QPainter *painter, const QRect &clip) -> void
{
  if (!marker.cursor.block().isVisible())
    return; // block containing marker not visible

  const auto offset = m_editor->contentOffset();
  const auto geometry = m_editor->blockBoundingGeometry(marker.cursor.block()).translated(offset);

  if (geometry.top() > clip.bottom() + 10 || geometry.bottom() < clip.top() - 10)
    return; // marker not visible

  const auto cursor = marker.cursor;
  const auto cursorRect = m_editor->cursorRect(cursor);

  auto icon = marker.icon;
  if (icon.isNull())
    icon = m_icon;

  const auto devicePixelRatio = painter->device()->devicePixelRatio();
  const auto proposedIconSize = QSize(m_editor->fontMetrics().horizontalAdvance(QLatin1Char(' ')) + 3, cursorRect.height()) * devicePixelRatio;
  auto actualIconSize = icon.actualSize(proposedIconSize);
  #if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    actualIconSize /= devicePixelRatio;
  #endif // Qt < 6.0

  const auto y = cursorRect.top() + (cursorRect.height() - actualIconSize.height()) / 2;
  const auto x = cursorRect.right();
  marker.rect = QRect(x, y, actualIconSize.width(), actualIconSize.height());

  icon.paint(painter, marker.rect);
  m_maxWidth = qMax(m_maxWidth, x + actualIconSize.width() - int(offset.x()));
}

auto RefactorMarker::filterOutType(const RefactorMarkers &markers, const Utils::Id &type) -> RefactorMarkers
{
  return Utils::filtered(markers, [type](const RefactorMarker &marker) {
    return marker.type != type;
  });
}

} // namespace TextEditor
