// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/infobar.hpp>
#include <utils/porting.hpp>

#include <QIcon>
#include <QWidget>

#include <QPropertyAnimation>

QT_BEGIN_NAMESPACE
class QPainter;
class QStackedLayout;
class QStatusBar;
QT_END_NAMESPACE

namespace Core {
namespace Internal {

class FancyTab final : public QObject {
  Q_OBJECT
  Q_PROPERTY(qreal fader READ fader WRITE setFader)

public:
  explicit FancyTab(QWidget *parent_tab_bar) : QObject(parent_tab_bar), m_tabbar(parent_tab_bar)
  {
    m_animator.setPropertyName("fader");
    m_animator.setTargetObject(this);
  }

  auto fader() const -> qreal { return m_fader; }
  auto setFader(qreal value) -> void;
  auto fadeIn() -> void;
  auto fadeOut() -> void;

  QIcon icon;
  QString text;
  QString tool_tip;
  bool enabled = false;
  bool has_menu = false;

private:
  QPropertyAnimation m_animator;
  QWidget *m_tabbar;
  qreal m_fader = 0;
};

class FancyTabBar final : public QWidget {
  Q_OBJECT

public:
  explicit FancyTabBar(QWidget *parent = nullptr);

  auto event(QEvent *event) -> bool override;
  auto paintTab(QPainter *painter, int tab_index) const -> void;
  auto mousePressEvent(QMouseEvent *event) -> void override;
  auto mouseMoveEvent(QMouseEvent *event) -> void override;
  auto enterEvent(Utils::EnterEvent *event) -> void override;
  auto leaveEvent(QEvent *event) -> void override;
  auto validIndex(const int index) const -> bool { return index >= 0 && index < m_tabs.count(); }
  auto sizeHint() const -> QSize override;
  auto minimumSizeHint() const -> QSize override;
  auto setTabEnabled(int index, bool enable) -> void;
  auto isTabEnabled(int index) const -> bool;

  auto insertTab(const int index, const QIcon &icon, const QString &label, const bool has_menu) -> void
  {
    const auto tab = new FancyTab(this);

    tab->icon = icon;
    tab->text = label;
    tab->has_menu = has_menu;

    m_tabs.insert(index, tab);
    if (m_current_index >= index)
      ++m_current_index;

    updateGeometry();
  }

  auto setEnabled(int index, bool enabled) -> void;

  auto removeTab(const int index) -> void
  {
    const auto tab = m_tabs.takeAt(index);
    delete tab;
    updateGeometry();
  }

  auto setCurrentIndex(int index) -> void;
  auto currentIndex() const -> int { return m_current_index; }
  auto setTabToolTip(const int index, const QString &tool_tip) -> void { m_tabs[index]->tool_tip = tool_tip; }
  auto tabToolTip(const int index) const -> QString { return m_tabs.at(index)->tool_tip; }
  auto setIconsOnly(bool icons_only) -> void;
  auto count() const -> int { return static_cast<int>(m_tabs.count()); }
  auto tabRect(int index) const -> QRect;

signals:
  auto currentAboutToChange(int index) -> void;
  auto currentChanged(int index) -> void;
  auto menuTriggered(int index, QMouseEvent *event) -> void;

private:
  QRect m_hover_rect;
  int m_hover_index = -1;
  int m_current_index = -1;
  bool m_icons_only = false;
  QList<FancyTab*> m_tabs;
  auto tabSizeHint(bool minimum = false) const -> QSize;
};

class FancyTabWidget final : public QWidget {
  Q_OBJECT

public:
  explicit FancyTabWidget(QWidget *parent = nullptr);

  auto insertTab(int index, QWidget *tab, const QIcon &icon, const QString &label, bool has_menu) const -> void;
  auto removeTab(int index) const -> void;
  auto setBackgroundBrush(const QBrush &brush) const -> void;
  auto addCornerWidget(QWidget *widget) const -> void;
  auto insertCornerWidget(int pos, QWidget *widget) const -> void;
  auto cornerWidgetCount() const -> int;
  auto setTabToolTip(int index, const QString &tool_tip) const -> void;
  auto paintEvent(QPaintEvent *event) -> void override;
  auto currentIndex() const -> int;
  auto statusBar() const -> QStatusBar*;
  auto infoBar() -> Utils::InfoBar*;
  auto setTabEnabled(int index, bool enable) const -> void;
  auto isTabEnabled(int index) const -> bool;
  auto setIconsOnly(bool icons_only) const -> void;
  auto isSelectionWidgetVisible() const -> bool;

signals:
  auto currentAboutToShow(int index) -> void;
  auto currentChanged(int index) -> void;
  auto menuTriggered(int index, QMouseEvent *event) -> void;
  auto topAreaClicked(Qt::MouseButton button, Qt::KeyboardModifiers modifiers) -> void;

public slots:
  auto setCurrentIndex(int index) const -> void;
  auto setSelectionWidgetVisible(bool visible) const -> void;

private:
  auto showWidget(int index) -> void;

  FancyTabBar *m_tab_bar;
  QWidget *m_corner_widget_container;
  QStackedLayout *m_modes_stack;
  QWidget *m_selection_widget;
  QStatusBar *m_status_bar;
  Utils::InfoBarDisplay m_info_bar_display;
  Utils::InfoBar m_info_bar;
};

} // namespace Internal
} // namespace Core
