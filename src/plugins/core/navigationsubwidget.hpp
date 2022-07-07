// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QComboBox>

#include <QList>

QT_BEGIN_NAMESPACE
class QMenu;
class QToolButton;
QT_END_NAMESPACE

namespace Utils {
class StyledBar;
}

namespace Core {
class INavigationWidgetFactory;
class Command;
class NavigationWidget;

namespace Internal {
class NavigationSubWidget final : public QWidget {
  Q_OBJECT

public:
  NavigationSubWidget(NavigationWidget *parent_widget, int position, int factory_index);
  ~NavigationSubWidget() override;

  auto factory() const -> INavigationWidgetFactory*;
  auto factoryIndex() const -> int;
  auto setFactoryIndex(int i) const -> void;
  auto setFocusWidget() const -> void;
  auto position() const -> int;
  auto setPosition(int position) -> void;
  auto saveSettings() const -> void;
  auto restoreSettings() const -> void;
  auto command(const QString &title) const -> Command*;
  auto setCloseIcon(const QIcon &icon) const -> void;
  auto widget() const -> QWidget*;

signals:
  auto splitMe(int factory_index) -> void;
  auto closeMe() -> void;
  auto factoryIndexChanged(int factory_index) -> void;

private:
  auto comboBoxIndexChanged(int) -> void;
  auto populateSplitMenu() -> void;

  NavigationWidget *m_parent_widget;
  QComboBox *m_navigation_combo_box;
  QMenu *m_split_menu;
  QToolButton *m_close_button;
  QWidget *m_navigation_widget;
  INavigationWidgetFactory *m_navigation_widget_factory;
  Utils::StyledBar *m_tool_bar;
  QList<QToolButton*> m_additional_tool_bar_widgets;
  int m_position;
};

// A combo associated with a command. Shows the command text
// and shortcut in the tooltip.
class CommandComboBox : public QComboBox {
  Q_OBJECT

public:
  explicit CommandComboBox(QWidget *parent = nullptr);

protected:
  auto event(QEvent *event) -> bool override;

private:
  virtual auto command(const QString &text) const -> const Command* = 0;
};

class NavComboBox final : public CommandComboBox {
  Q_OBJECT

public:
  explicit NavComboBox(NavigationSubWidget *nav_sub_widget) : m_nav_sub_widget(nav_sub_widget) {}

private:
  auto command(const QString &text) const -> const Command* override
  {
    return m_nav_sub_widget->command(text);
  }

  NavigationSubWidget *m_nav_sub_widget;
};

} // namespace Internal
} // namespace Core
