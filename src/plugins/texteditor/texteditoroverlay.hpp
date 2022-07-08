// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0
#pragma once

#include <QObject>
#include <QList>
#include <QVector>
#include <QTextCursor>
#include <QColor>

QT_FORWARD_DECLARE_CLASS(QWidget)
QT_FORWARD_DECLARE_CLASS(QPainterPath)

namespace TextEditor {
class TextEditorWidget;

namespace Internal {

struct OverlaySelection {
  OverlaySelection() = default;

  QTextCursor m_cursor_begin;
  QTextCursor m_cursor_end;
  QColor m_fg;
  QColor m_bg;
  int m_fixedLength = -1;
  bool m_dropShadow = false;
};

class TextEditorOverlay : public QObject {
  Q_OBJECT

public:
  TextEditorOverlay(TextEditorWidget *editor);

  auto rect() const -> QRect;
  auto paint(QPainter *painter, const QRect &clip) -> void;
  auto fill(QPainter *painter, const QColor &color, const QRect &clip) -> void;
  auto isVisible() const -> bool { return m_visible; }
  auto setVisible(bool b) -> void;
  auto hide() -> void { setVisible(false); }
  auto show() -> void { setVisible(true); }

  auto setBorderWidth(int bw) -> void { m_borderWidth = bw; }

  auto update() -> void;

  auto setAlpha(bool enabled) -> void { m_alpha = enabled; }

  virtual auto clear() -> void;

  enum OverlaySelectionFlags {
    LockSize = 1,
    DropShadow = 2,
    ExpandBegin = 4
  };

  auto addOverlaySelection(const QTextCursor &cursor, const QColor &fg, const QColor &bg, uint overlaySelectionFlags = 0) -> void;
  auto addOverlaySelection(int begin, int end, const QColor &fg, const QColor &bg, uint overlaySelectionFlags = 0) -> void;
  auto selections() const -> const QList<OverlaySelection>& { return m_selections; }
  auto isEmpty() const -> bool { return m_selections.isEmpty(); }
  auto dropShadowWidth() const -> int { return m_dropShadowWidth; }
  auto hasFirstSelectionBeginMoved() const -> bool;

protected:
  auto cursorForSelection(const OverlaySelection &selection) const -> QTextCursor;
  auto cursorForIndex(int selectionIndex) const -> QTextCursor;

private:
  auto createSelectionPath(const QTextCursor &begin, const QTextCursor &end, const QRect &clip) -> QPainterPath;
  auto paintSelection(QPainter *painter, const OverlaySelection &selection) -> void;
  auto fillSelection(QPainter *painter, const OverlaySelection &selection, const QColor &color) -> void;

  bool m_visible;
  bool m_alpha;
  int m_borderWidth;
  int m_dropShadowWidth;
  int m_firstSelectionOriginalBegin;

  TextEditorWidget *m_editor;
  QWidget *m_viewport;
  QList<OverlaySelection> m_selections;
};

} // namespace Internal
} // namespace TextEditor
