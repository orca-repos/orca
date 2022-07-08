// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include <utils/id.hpp>

#include <QTextCursor>
#include <QIcon>

namespace TextEditor {
class TextEditorWidget;

struct TEXTEDITOR_EXPORT RefactorMarker;
using RefactorMarkers = QList<RefactorMarker>;

struct TEXTEDITOR_EXPORT RefactorMarker {
  auto isValid() const -> bool { return !cursor.isNull(); }
  QTextCursor cursor;
  QString tooltip;
  QIcon icon;
  mutable QRect rect; // used to cache last drawing positin in document coordinates
  std::function<void(TextEditorWidget *)> callback;
  Utils::Id type;
  QVariant data;

  static auto filterOutType(const RefactorMarkers &markers, const Utils::Id &type) -> RefactorMarkers;
};

class TEXTEDITOR_EXPORT RefactorOverlay : public QObject {
  Q_OBJECT

public:
  explicit RefactorOverlay(TextEditorWidget *editor);

  auto isEmpty() const -> bool { return m_markers.isEmpty(); }
  auto paint(QPainter *painter, const QRect &clip) -> void;
  auto setMarkers(const RefactorMarkers &markers) -> void { m_markers = markers; }
  auto markers() const -> RefactorMarkers { return m_markers; }
  auto clear() -> void { m_markers.clear(); }
  auto markerAt(const QPoint &pos) const -> RefactorMarker;

private:
  auto paintMarker(const RefactorMarker &marker, QPainter *painter, const QRect &clip) -> void;
  RefactorMarkers m_markers;
  TextEditorWidget *m_editor;
  int m_maxWidth;
  const QIcon m_icon;
};

} // namespace TextEditor
