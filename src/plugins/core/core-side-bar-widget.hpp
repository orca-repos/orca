// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QWidget>

QT_BEGIN_NAMESPACE
class QToolBar;
class QAction;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class SideBar;
class SideBarItem;
class Command;
class SideBarComboBox;

class SideBarWidget final : public QWidget {
  Q_OBJECT

public:
  explicit SideBarWidget(SideBar *side_bar, const QString &id);
  ~SideBarWidget() override;

  auto currentItemId() const -> QString;
  auto currentItemTitle() const -> QString;
  auto setCurrentItem(const QString &id) -> void;
  auto updateAvailableItems() const -> void;
  auto removeCurrentItem() -> void;
  auto command(const QString &title) const -> Command*;
  auto setCloseIcon(const QIcon &icon) const -> void;

signals:
  auto splitMe() -> void;
  auto closeMe() -> void;
  auto currentWidgetChanged() -> void;

private:
  auto setCurrentIndex(int) -> void;

  SideBarComboBox *m_combo_box = nullptr;
  SideBarItem *m_current_item = nullptr;
  QToolBar *m_toolbar = nullptr;
  QAction *m_split_action = nullptr;
  QAction *m_close_action = nullptr;
  QList<QAction*> m_added_tool_bar_actions;
  SideBar *m_side_bar = nullptr;
};

} // namespace Orca::Plugin::Core
