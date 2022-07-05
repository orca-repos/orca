// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "minisplitter.h"

#include <utils/theme/theme.h>

#include <QPaintEvent>
#include <QPainter>
#include <QSplitterHandle>

namespace Core {
namespace Internal {

class MiniSplitterHandle final : public QSplitterHandle {
public:
  MiniSplitterHandle(const Qt::Orientation orientation, QSplitter *parent, const bool light_colored = false) : QSplitterHandle(orientation, parent), m_light_colored(light_colored)
  {
    setMask(QRegion(contentsRect()));
    setAttribute(Qt::WA_MouseNoMask, true);
  }

protected:
  auto resizeEvent(QResizeEvent *event) -> void override;
  auto paintEvent(QPaintEvent *event) -> void override;

private:
  bool m_light_colored;
};

} // namespace Internal
} // namespace Core

using namespace Core;
using namespace Core::Internal;

auto MiniSplitterHandle::resizeEvent(QResizeEvent *event) -> void
{
  if (orientation() == Qt::Horizontal)
    setContentsMargins(2, 0, 2, 0);
  else
    setContentsMargins(0, 2, 0, 2);

  setMask(QRegion(contentsRect()));
  QSplitterHandle::resizeEvent(event);
}

auto MiniSplitterHandle::paintEvent(QPaintEvent *event) -> void
{
  QPainter painter(this);
  const auto color = Utils::orcaTheme()->color(m_light_colored ? Utils::Theme::FancyToolBarSeparatorColor : Utils::Theme::SplitterColor);
  painter.fillRect(event->rect(), color);
}

/*!
    \class Core::MiniSplitter
    \inheaderfile coreplugin/minisplitter.h
    \inmodule Orca

    \brief The MiniSplitter class is a simple helper-class to obtain
    \macos style 1-pixel wide splitters.
*/

/*!
    \enum Core::MiniSplitter::SplitterStyle
    This enum value holds the splitter style.

    \value Dark  Dark style.
    \value Light Light style.
*/

auto MiniSplitter::createHandle() -> QSplitterHandle*
{
  return new MiniSplitterHandle(orientation(), this, m_style == Light);
}

MiniSplitter::MiniSplitter(QWidget *parent, const SplitterStyle style) : QSplitter(parent), m_style(style)
{
  setHandleWidth(1);
  setChildrenCollapsible(false);
  setProperty("minisplitter", true);
}

MiniSplitter::MiniSplitter(const Qt::Orientation orientation, QWidget *parent, const SplitterStyle style) : QSplitter(orientation, parent), m_style(style)
{
  setHandleWidth(1);
  setChildrenCollapsible(false);
  setProperty("minisplitter", true);
}

/*!
    \class Core::NonResizingSplitter
    \inheaderfile coreplugin/minisplitter.h
    \inmodule Orca

    \brief The NonResizingSplitter class is a MiniSplitter that keeps its
    first widget's size fixed when it is resized.
*/

/*!
    Constructs a non-resizing splitter with \a parent and \a style.

    The default style is MiniSplitter::Light.
*/
NonResizingSplitter::NonResizingSplitter(QWidget *parent, const SplitterStyle style) : MiniSplitter(parent, style) {}

/*!
    \internal
*/
auto NonResizingSplitter::resizeEvent(QResizeEvent *ev) -> void
{
  // bypass QSplitter magic
  const auto left_split_width = qMin(sizes().at(0), ev->size().width());
  const auto right_split_width = qMax(0, ev->size().width() - left_split_width);
  setSizes(QList<int>() << left_split_width << right_split_width);
  QSplitter::resizeEvent(ev);
}
