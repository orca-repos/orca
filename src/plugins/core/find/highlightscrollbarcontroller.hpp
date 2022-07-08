// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.hpp>

#include <utils/id.hpp>
#include <utils/theme/theme.hpp>

#include <QHash>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QAbstractScrollArea;
class QScrollBar;
QT_END_NAMESPACE

namespace Core {

struct CORE_EXPORT Highlight {
  enum Priority {
    Invalid = -1,
    LowPriority = 0,
    NormalPriority = 1,
    HighPriority = 2,
    HighestPriority = 3
  };

  Highlight(Utils::Id category, int position, Utils::Theme::Color color, Priority priority);
  Highlight() = default;

  Utils::Id category;
  int position = -1;
  Utils::Theme::Color color = Utils::Theme::TextColorNormal;
  Priority priority = Invalid;
};

class HighlightScrollBarOverlay;

class CORE_EXPORT HighlightScrollBarController {
  Q_DISABLE_COPY_MOVE(HighlightScrollBarController)

public:
  HighlightScrollBarController() = default;
  ~HighlightScrollBarController();

  auto scrollBar() const -> QScrollBar*;
  auto scrollArea() const -> QAbstractScrollArea*;
  auto setScrollArea(QAbstractScrollArea *scroll_area) -> void;
  auto lineHeight() const -> double;
  auto setLineHeight(double line_height) -> void;
  auto visibleRange() const -> double;
  auto setVisibleRange(double visible_range) -> void;
  auto margin() const -> double;
  auto setMargin(double margin) -> void;
  auto highlights() const -> QHash<Utils::Id, QVector<Highlight>>;
  auto addHighlight(Highlight highlight) -> void;
  auto removeHighlights(Utils::Id category) -> void;
  auto removeAllHighlights() -> void;

private:
  QHash<Utils::Id, QVector<Highlight>> m_highlights;
  double m_line_height = 0.0;
  double m_visible_range = 0.0; // in pixels
  double m_margin = 0.0;       // in pixels
  QAbstractScrollArea *m_scroll_area = nullptr;
  QPointer<HighlightScrollBarOverlay> m_overlay;
};

} // namespace Core
