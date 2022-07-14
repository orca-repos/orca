// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"
#include "core-mini-splitter.hpp"

#include <QList>
#include <QMap>

QT_BEGIN_NAMESPACE
class QSettings;
class QToolButton;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class  Command;
struct SideBarPrivate;
class  SideBarWidget;

/*
 * An item in the sidebar. Has a widget that is displayed in the sidebar and
 * optionally a list of tool buttons that are added to the toolbar above it.
 * The window title of the widget is displayed in the combo box.
 *
 * The SideBarItem takes ownership over the widget.
 */
class CORE_EXPORT SideBarItem final : public QObject {
  Q_OBJECT

public:
  // id is non-localized string of the item that's used to store the settings.
  explicit SideBarItem(QWidget *widget, QString id);
  ~SideBarItem() override;

  auto widget() const -> QWidget*;
  auto id() const -> QString;
  auto title() const -> QString;

  /* Should always return a new set of tool buttons.
   *
   * Workaround since there doesn't seem to be a nice way to remove widgets
   * that have been added to a QToolBar without either not deleting the
   * associated QAction or causing the QToolButton to be deleted.
   */
  auto createToolBarWidgets() -> QList<QToolButton*>;

private:
  const QString m_id;
  QWidget *m_widget;
};

class CORE_EXPORT SideBar final : public MiniSplitter {
  Q_OBJECT

public:
  /*
   * The SideBar takes explicit ownership of the SideBarItems
   * if you have one SideBar, or shared ownership in case
   * of multiple SideBars.
   */
  SideBar(const QList<SideBarItem*> &item_list, const QList<SideBarItem*> &default_visible);
  ~SideBar() override;

  auto availableItemIds() const -> QStringList;
  auto availableItemTitles() const -> QStringList;
  auto unavailableItemIds() const -> QStringList;
  auto makeItemAvailable(const SideBarItem *item) -> void;
  auto setUnavailableItemIds(const QStringList &item_ids) -> void;
  auto idForTitle(const QString &title) const -> QString;
  auto item(const QString &id) -> SideBarItem*;
  auto closeWhenEmpty() const -> bool;
  auto setCloseWhenEmpty(bool value) const -> void;
  auto saveSettings(QSettings *settings, const QString &name) const -> void;
  auto readSettings(const QSettings *settings, const QString &name) -> void;
  auto closeAllWidgets() const -> void;
  auto activateItem(const QString &id) const -> void;
  auto setShortcutMap(const QMap<QString, Command*> &shortcut_map) const -> void;
  auto shortcutMap() const -> QMap<QString, Command*>;

signals:
  auto sideBarClosed() -> void;
  auto availableItemsChanged() -> void;

private:
  auto splitSubWidget() -> void;
  auto closeSubWidget() -> void;
  auto updateWidgets() const -> void;
  auto insertSideBarWidget(int position, const QString &id = QString()) -> SideBarWidget*;
  auto removeSideBarWidget(SideBarWidget *widget) const -> void;

  SideBarPrivate *d;
};

} // namespace Orca::Plugin::Core
