// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QToolButton>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class FancyToolButton final : public QToolButton {
  Q_OBJECT
  Q_PROPERTY(qreal fader READ fader WRITE setFader)

public:
  explicit FancyToolButton(QAction *action, QWidget *parent = nullptr);

  auto paintEvent(QPaintEvent *event) -> void override;
  auto event(QEvent *e) -> bool override;
  auto sizeHint() const -> QSize override;
  auto minimumSizeHint() const -> QSize override;
  auto fader() const -> qreal { return m_fader; }

  auto setFader(const qreal value) -> void
  {
    m_fader = value;
    update();
  }

  auto setIconsOnly(bool icons_only) -> void;
  static auto hoverOverlay(QPainter *painter, const QRect &span_rect) -> void;

private:
  auto actionChanged() -> void;

  qreal m_fader = 0;
  bool m_icons_only = false;
};

class FancyActionBar final : public QWidget {
  Q_OBJECT

public:
  explicit FancyActionBar(QWidget *parent = nullptr);

  auto paintEvent(QPaintEvent *event) -> void override;
  auto insertAction(int index, QAction *action) -> void;
  auto addProjectSelector(QAction *action) -> void;
  auto actionsLayout() const -> QLayout*;
  auto minimumSizeHint() const -> QSize override;
  auto setIconsOnly(bool icons_only) -> void;

private:
  QVBoxLayout *m_actions_layout;
  bool m_icons_only = false;
};

} // namespace Orca::Plugin::Core
