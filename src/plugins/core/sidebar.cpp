// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "sidebar.h"
#include "sidebarwidget.h"

#include <core/actionmanager/command.h>

#include <utils/algorithm.h>
#include <utils/qtcassert.h>
#include <utils/utilsicons.h>

#include <QSettings>
#include <QPointer>
#include <QToolButton>

namespace Core {

SideBarItem::SideBarItem(QWidget *widget, QString id) : m_id(std::move(id)), m_widget(widget) {}

SideBarItem::~SideBarItem()
{
  delete m_widget;
}

auto SideBarItem::widget() const -> QWidget*
{
  return m_widget;
}

auto SideBarItem::id() const -> QString
{
  return m_id;
}

auto SideBarItem::title() const -> QString
{
  return m_widget->windowTitle();
}

auto SideBarItem::createToolBarWidgets() -> QList<QToolButton*>
{
  return {};
}

struct SideBarPrivate {
  SideBarPrivate() = default;

  QList<Internal::SideBarWidget*> m_widgets;
  QMap<QString, QPointer<SideBarItem>> m_itemMap;
  QStringList m_availableItemIds;
  QStringList m_availableItemTitles;
  QStringList m_unavailableItemIds;
  QStringList m_defaultVisible;
  QMap<QString, Core::Command*> m_shortcutMap;
  bool m_closeWhenEmpty = false;
};

SideBar::SideBar(const QList<SideBarItem*> &item_list, const QList<SideBarItem*> &default_visible) : d(new SideBarPrivate)
{
  setOrientation(Qt::Vertical);
  for(const auto item: item_list) {
    d->m_itemMap.insert(item->id(), item);
    d->m_availableItemIds.append(item->id());
    d->m_availableItemTitles.append(item->title());
  }

  for(auto item: default_visible) {
    if (!item_list.contains(item))
      continue;
    d->m_defaultVisible.append(item->id());
  }
}

SideBar::~SideBar()
{
  for (const auto &i : d->m_itemMap)
    if (!i.isNull())
      delete i.data();
  delete d;
}

auto SideBar::idForTitle(const QString &title) const -> QString
{
  for (auto iter = d->m_itemMap.cbegin(), end = d->m_itemMap.cend(); iter != end; ++iter) {
    if (iter.value().data()->title() == title)
      return iter.key();
  }
  return {};
}

auto SideBar::availableItemIds() const -> QStringList
{
  return d->m_availableItemIds;
}

auto SideBar::availableItemTitles() const -> QStringList
{
  return d->m_availableItemTitles;
}

auto SideBar::unavailableItemIds() const -> QStringList
{
  return d->m_unavailableItemIds;
}

auto SideBar::closeWhenEmpty() const -> bool
{
  return d->m_closeWhenEmpty;
}

auto SideBar::setCloseWhenEmpty(const bool value) const -> void
{
  d->m_closeWhenEmpty = value;
}

auto SideBar::makeItemAvailable(const SideBarItem *item) -> void
{
  const auto cend = d->m_itemMap.constEnd();

  for (auto it = d->m_itemMap.constBegin(); it != cend; ++it) {
    if (it.value().data() == item) {
      d->m_availableItemIds.append(it.key());
      d->m_availableItemTitles.append(it.value().data()->title());
      d->m_unavailableItemIds.removeAll(it.key());
      Utils::sort(d->m_availableItemTitles);
      emit availableItemsChanged();
      //updateWidgets();
      break;
    }
  }
}

// sets a list of externally used, unavailable items. For example,
// another sidebar could set
auto SideBar::setUnavailableItemIds(const QStringList &item_ids) -> void
{
  // re-enable previous items
  for (const auto &id: d->m_unavailableItemIds) {
    d->m_availableItemIds.append(id);
    d->m_availableItemTitles.append(d->m_itemMap.value(id).data()->title());
  }

  d->m_unavailableItemIds.clear();

  for (const auto &id: item_ids) {
    if (!d->m_unavailableItemIds.contains(id))
      d->m_unavailableItemIds.append(id);
    d->m_availableItemIds.removeAll(id);
    d->m_availableItemTitles.removeAll(d->m_itemMap.value(id).data()->title());
  }
  Utils::sort(d->m_availableItemTitles);
  updateWidgets();
}

auto SideBar::item(const QString &id) -> SideBarItem*
{
  if (d->m_itemMap.contains(id)) {
    d->m_availableItemIds.removeAll(id);
    d->m_availableItemTitles.removeAll(d->m_itemMap.value(id).data()->title());

    if (!d->m_unavailableItemIds.contains(id))
      d->m_unavailableItemIds.append(id);

    emit availableItemsChanged();
    return d->m_itemMap.value(id).data();
  }
  return nullptr;
}

auto SideBar::insertSideBarWidget(const int position, const QString &id) -> Internal::SideBarWidget*
{
  if (!d->m_widgets.isEmpty())
    d->m_widgets.at(0)->setCloseIcon(Utils::Icons::CLOSE_SPLIT_BOTTOM.icon());

  const auto item = new Internal::SideBarWidget(this, id);
  connect(item, &Internal::SideBarWidget::splitMe, this, &SideBar::splitSubWidget);
  connect(item, &Internal::SideBarWidget::closeMe, this, &SideBar::closeSubWidget);
  connect(item, &Internal::SideBarWidget::currentWidgetChanged, this, &SideBar::updateWidgets);
  insertWidget(position, item);

  d->m_widgets.insert(position, item);
  if (d->m_widgets.size() == 1)
    d->m_widgets.at(0)->setCloseIcon(d->m_widgets.size() == 1 ? Utils::Icons::CLOSE_SPLIT_LEFT.icon() : Utils::Icons::CLOSE_SPLIT_TOP.icon());

  updateWidgets();
  return item;
}

auto SideBar::removeSideBarWidget(Internal::SideBarWidget *widget) const -> void
{
  widget->removeCurrentItem();
  d->m_widgets.removeOne(widget);
  widget->hide();
  widget->deleteLater();
}

auto SideBar::splitSubWidget() -> void
{
  const auto original = qobject_cast<Internal::SideBarWidget*>(sender());
  const auto pos = indexOf(original) + 1;
  insertSideBarWidget(pos);
  updateWidgets();
}

auto SideBar::closeSubWidget() -> void
{
  if (d->m_widgets.count() != 1) {
    const auto widget = qobject_cast<Internal::SideBarWidget*>(sender());
    if (!widget)
      return;
    removeSideBarWidget(widget);
    // update close button of top item
    if (d->m_widgets.size() == 1)
      d->m_widgets.at(0)->setCloseIcon(d->m_widgets.size() == 1 ? Utils::Icons::CLOSE_SPLIT_LEFT.icon() : Utils::Icons::CLOSE_SPLIT_TOP.icon());
    updateWidgets();
  } else {
    if (d->m_closeWhenEmpty) {
      setVisible(false);
      emit sideBarClosed();
    }
  }
}

auto SideBar::updateWidgets() const -> void
{
  foreach(Internal::SideBarWidget *i, d->m_widgets)
    i->updateAvailableItems();
}

auto SideBar::saveSettings(QSettings *settings, const QString &name) const -> void
{
  const auto prefix = name.isEmpty() ? name : (name + QLatin1Char('/'));

  QStringList views;

  for (auto i = 0; i < d->m_widgets.count(); ++i) {
    if (auto current_item_id = d->m_widgets.at(i)->currentItemId(); !current_item_id.isEmpty())
      views.append(current_item_id);
  }

  if (views.isEmpty() && !d->m_itemMap.isEmpty())
    views.append(d->m_itemMap.cbegin().key());

  settings->setValue(prefix + QLatin1String("Views"), views);
  settings->setValue(prefix + QLatin1String("Visible"), parentWidget() ? isVisibleTo(parentWidget()) : true);
  settings->setValue(prefix + QLatin1String("VerticalPosition"), saveState());
  settings->setValue(prefix + QLatin1String("Width"), width());
}

auto SideBar::closeAllWidgets() const -> void
{
  for(const auto &widget: d->m_widgets)
    removeSideBarWidget(widget);
}

auto SideBar::readSettings(const QSettings *settings, const QString &name) -> void
{
  const auto prefix = name.isEmpty() ? name : (name + QLatin1Char('/'));

  closeAllWidgets();

  if (const auto views_key = prefix + QLatin1String("Views"); settings->contains(views_key)) {
    if (auto views = settings->value(views_key).toStringList(); !views.isEmpty()) {
      for(const auto &id: views) if (availableItemIds().contains(id))
        insertSideBarWidget(static_cast<int>(d->m_widgets.count()), id);

    } else {
      insertSideBarWidget(0);
    }
  }
  if (d->m_widgets.empty()) {
    for(const auto &id: d->m_defaultVisible)
      insertSideBarWidget(static_cast<int>(d->m_widgets.count()), id);
  }

  if (const QString visible_key = prefix + QLatin1String("Visible"); settings->contains(visible_key))
    setVisible(settings->value(visible_key).toBool());

  if (const QString position_key = prefix + QLatin1String("VerticalPosition"); settings->contains(position_key))
    restoreState(settings->value(position_key).toByteArray());

  if (const QString width_key = prefix + QLatin1String("Width"); settings->contains(width_key)) {
    auto s = size();
    s.setWidth(settings->value(width_key).toInt());
    resize(s);
  }
}

auto SideBar::activateItem(const QString &id) const -> void
{
  QTC_ASSERT(d->m_itemMap.contains(id), return);

  for (auto i = 0; i < d->m_widgets.count(); ++i) {
    if (d->m_widgets.at(i)->currentItemId() == id) {
      d->m_itemMap.value(id)->widget()->setFocus();
      return;
    }
  }

  const auto widget = d->m_widgets.first();
  widget->setCurrentItem(id);
  updateWidgets();
  d->m_itemMap.value(id)->widget()->setFocus();
}

auto SideBar::setShortcutMap(const QMap<QString, Command*> &shortcut_map) const -> void
{
  d->m_shortcutMap = shortcut_map;
}

auto SideBar::shortcutMap() const -> QMap<QString, Command*>
{
  return d->m_shortcutMap;
}

} // namespace Core

