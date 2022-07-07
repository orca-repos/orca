// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "navigationwidget.hpp"
#include "navigationsubwidget.hpp"
#include "icontext.hpp"
#include "icore.hpp"
#include "inavigationwidgetfactory.hpp"
#include "modemanager.hpp"
#include "imode.hpp"

#include <core/actionmanager/actionmanager.hpp>
#include <core/actionmanager/command.hpp>

#include <utils/qtcassert.hpp>
#include <utils/utilsicons.hpp>

#include <QCoreApplication>
#include <QSettings>
#include <QAction>
#include <QHBoxLayout>
#include <QResizeEvent>
#include <QStandardItemModel>

Q_DECLARE_METATYPE(Core::INavigationWidgetFactory *)

using namespace Utils;

namespace Core {

NavigationWidgetPlaceHolder *NavigationWidgetPlaceHolder::s_current_left = nullptr;
NavigationWidgetPlaceHolder *NavigationWidgetPlaceHolder::s_current_right = nullptr;

auto NavigationWidgetPlaceHolder::current(const Side side) -> NavigationWidgetPlaceHolder*
{
  return side == Side::Left ? s_current_left : s_current_right;
}

auto NavigationWidgetPlaceHolder::setCurrent(const Side side, NavigationWidgetPlaceHolder *nav_widget) -> void
{
  if (side == Side::Left)
    s_current_left = nav_widget;
  else
    s_current_right = nav_widget;
}

NavigationWidgetPlaceHolder::NavigationWidgetPlaceHolder(const Id mode, const Side side, QWidget *parent) : QWidget(parent), m_mode(mode), m_side(side)
{
  setLayout(new QVBoxLayout);
  layout()->setContentsMargins(0, 0, 0, 0);
  connect(ModeManager::instance(), &ModeManager::currentModeAboutToChange, this, &NavigationWidgetPlaceHolder::currentModeAboutToChange);
}

NavigationWidgetPlaceHolder::~NavigationWidgetPlaceHolder()
{
  if (current(m_side) == this) {
    if (const auto nw = NavigationWidget::instance(m_side)) {
      nw->setParent(nullptr);
      nw->hide();
    }
  }
}

auto NavigationWidgetPlaceHolder::applyStoredSize() -> void
{
  if (const auto splitter = qobject_cast<QSplitter*>(parentWidget())) {
    // A splitter we need to resize the splitter sizes
    auto sizes = splitter->sizes();
    auto diff = 0;
    auto count = static_cast<int>(sizes.count());

    for (auto i = 0; i < sizes.count(); ++i) {
      if (const auto ph = qobject_cast<NavigationWidgetPlaceHolder*>(splitter->widget(i))) {
        --count;
        const auto width = ph->storedWidth();
        diff += width - sizes.at(i);
        sizes[i] = width;
      }
    }

    const auto adjust = count > 1 ? diff / (count - 1) : 0;

    for (auto i = 0; i < sizes.count(); ++i) {
      if (!qobject_cast<NavigationWidgetPlaceHolder*>(splitter->widget(i)))
        sizes[i] += adjust;
    }

    splitter->setSizes(sizes);
  } else {
    auto s = size();
    s.setWidth(storedWidth());
    resize(s);
  }
}

// This function does work even though the order in which
// the placeHolder get the signal is undefined.
// It does ensure that after all PlaceHolders got the signal
// m_current points to the current PlaceHolder, or zero if there
// is no PlaceHolder in this mode
// And that the parent of the NavigationWidget gets the correct parent
auto NavigationWidgetPlaceHolder::currentModeAboutToChange(const Id mode) -> void
{
  const auto navigation_widget = NavigationWidget::instance(m_side);

  if (const auto current = NavigationWidgetPlaceHolder::current(m_side); current == this) {
    setCurrent(m_side, nullptr);
    navigation_widget->setParent(nullptr);
    navigation_widget->hide();
    navigation_widget->placeHolderChanged(nullptr);
  }

  if (m_mode == mode) {
    setCurrent(m_side, this);
    layout()->addWidget(navigation_widget);
    navigation_widget->show();
    applyStoredSize();
    setVisible(navigation_widget->isShown());
    navigation_widget->placeHolderChanged(this);
  }
}

auto NavigationWidgetPlaceHolder::storedWidth() const -> int
{
  return NavigationWidget::instance(m_side)->storedWidth();
}

struct ActivationInfo {
  Side side;
  int position;
};

using ActivationsMap = QHash<Id, ActivationInfo>;

struct NavigationWidgetPrivate {
  explicit NavigationWidgetPrivate(QAction *toggle_side_bar_action, Side side);
  ~NavigationWidgetPrivate() { delete m_factoryModel; }

  QList<Internal::NavigationSubWidget*> m_subWidgets;
  QHash<QAction*, Id> m_actionMap;
  QHash<Id, Command*> m_commandMap;
  QStandardItemModel *m_factoryModel;

  bool m_shown;
  int m_width;
  QAction *m_toggleSideBarAction; // does not take ownership
  Side m_side;

  static NavigationWidget *s_instance_left;
  static NavigationWidget *s_instance_right;

  static ActivationsMap s_activations_map;

  static auto updateActivationsMap(Id activated_id, const ActivationInfo &activation_info) -> void;
  static auto removeFromActivationsMap(const ActivationInfo &activation_info) -> void;
};

NavigationWidgetPrivate::NavigationWidgetPrivate(QAction *toggle_side_bar_action, const Side side) : m_factoryModel(new QStandardItemModel), m_shown(true), m_width(0), m_toggleSideBarAction(toggle_side_bar_action), m_side(side) {}

auto NavigationWidgetPrivate::updateActivationsMap(const Id activated_id, const ActivationInfo &activation_info) -> void
{
  s_activations_map.insert(activated_id, activation_info);
}

NavigationWidget *NavigationWidgetPrivate::s_instance_left = nullptr;
NavigationWidget *NavigationWidgetPrivate::s_instance_right = nullptr;
ActivationsMap NavigationWidgetPrivate::s_activations_map;

NavigationWidget::NavigationWidget(QAction *toggle_side_bar_action, const Side side) : d(new NavigationWidgetPrivate(toggle_side_bar_action, side))
{
  d->m_factoryModel->setSortRole(FactoryPriorityRole);
  setOrientation(Qt::Vertical);

  if (side == Side::Left)
    NavigationWidgetPrivate::s_instance_left = this;
  else
    NavigationWidgetPrivate::s_instance_right = this;
}

NavigationWidget::~NavigationWidget()
{
  if (d->m_side == Side::Left)
    NavigationWidgetPrivate::s_instance_left = nullptr;
  else
    NavigationWidgetPrivate::s_instance_right = nullptr;

  delete d;
}

auto NavigationWidget::instance(const Side side) -> NavigationWidget*
{
  return side == Side::Left ? NavigationWidgetPrivate::s_instance_left : NavigationWidgetPrivate::s_instance_right;
}

auto NavigationWidget::activateSubWidget(const Id factory_id, const Side fallback_side) -> QWidget*
{
  auto navigation_widget = instance(fallback_side);
  auto preferred_position = -1;

  if (NavigationWidgetPrivate::s_activations_map.contains(factory_id)) {
    const auto [side, position] = NavigationWidgetPrivate::s_activations_map.value(factory_id);
    navigation_widget = instance(side);
    preferred_position = position;
  }

  return navigation_widget->activateSubWidget(factory_id, preferred_position);
}

auto NavigationWidget::setFactories(const QList<INavigationWidgetFactory*> &factories) -> void
{
  const Context navicontext(Constants::C_NAVIGATION_PANE);

  for(auto &factory: factories) {
    const auto id = factory->id();
    const auto action_id = id.withPrefix("Orca.Sidebar.");
    if (!ActionManager::command(action_id)) {
      auto action = new QAction(tr("Activate %1 View").arg(factory->displayName()), this);
      d->m_actionMap.insert(action, id);
      connect(action, &QAction::triggered, this, [this, action]() {
        activateSubWidget(d->m_actionMap[action], Side::Left);
      });
      auto cmd = ActionManager::registerAction(action, action_id, navicontext);
      cmd->setDefaultKeySequence(factory->activationSequence());
      d->m_commandMap.insert(id, cmd);
    }
    const auto new_row = new QStandardItem(factory->displayName());
    new_row->setData(QVariant::fromValue(factory), FactoryObjectRole);
    new_row->setData(QVariant::fromValue(factory->id()), FactoryIdRole);
    new_row->setData(QVariant::fromValue(action_id), FactoryActionIdRole);
    new_row->setData(factory->priority(), FactoryPriorityRole);
    d->m_factoryModel->appendRow(new_row);
  }

  d->m_factoryModel->sort(0);
  updateToggleText();
}

auto NavigationWidget::settingsGroup() const -> QString
{
  const auto side(d->m_side == Side::Left ? QStringLiteral("Left") : QStringLiteral("Right"));
  return QStringLiteral("Navigation%1").arg(side);
}

auto NavigationWidget::storedWidth() const -> int
{
  return d->m_width;
}

auto NavigationWidget::factoryModel() const -> QAbstractItemModel*
{
  return d->m_factoryModel;
}

auto NavigationWidget::updateToggleText() const -> void
{
  const bool have_data = d->m_factoryModel->rowCount();

  d->m_toggleSideBarAction->setVisible(have_data);
  d->m_toggleSideBarAction->setEnabled(have_data && NavigationWidgetPlaceHolder::current(d->m_side));

  const auto tr_tool_tip = d->m_side == Side::Left ? (isShown() ? Constants::TR_HIDE_LEFT_SIDEBAR : Constants::TR_SHOW_LEFT_SIDEBAR) : isShown() ? Constants::TR_HIDE_RIGHT_SIDEBAR : Constants::TR_SHOW_RIGHT_SIDEBAR;

  d->m_toggleSideBarAction->setToolTip(QCoreApplication::translate("Core", tr_tool_tip));
}

auto NavigationWidget::placeHolderChanged(const NavigationWidgetPlaceHolder *holder) const -> void
{
  d->m_toggleSideBarAction->setChecked(holder && isShown());
  updateToggleText();
}

auto NavigationWidget::resizeEvent(QResizeEvent *re) -> void
{
  if (d->m_width && re->size().width())
    d->m_width = re->size().width();
  MiniSplitter::resizeEvent(re);
}

static auto closeIconForSide(const Side side, const int item_count) -> QIcon
{
  if (item_count > 1)
    return Icons::CLOSE_SPLIT_TOP.icon();
  return side == Side::Left ? Icons::CLOSE_SPLIT_LEFT.icon() : Icons::CLOSE_SPLIT_RIGHT.icon();
}

auto NavigationWidget::insertSubItem(const int position, const int factory_index) -> Internal::NavigationSubWidget*
{
  for (auto pos = position + 1; pos < d->m_subWidgets.size(); ++pos) {
    const auto nsw = d->m_subWidgets.at(pos);
    nsw->setPosition(pos + 1);
    NavigationWidgetPrivate::updateActivationsMap(nsw->factory()->id(), {d->m_side, pos + 1});
  }

  if (!d->m_subWidgets.isEmpty()) // Make all icons the bottom icon
    d->m_subWidgets.at(0)->setCloseIcon(Icons::CLOSE_SPLIT_BOTTOM.icon());

  const auto nsw = new Internal::NavigationSubWidget(this, position, factory_index);
  connect(nsw, &Internal::NavigationSubWidget::splitMe, this, &NavigationWidget::splitSubWidget);
  connect(nsw, &Internal::NavigationSubWidget::closeMe, this, &NavigationWidget::closeSubWidget);
  connect(nsw, &Internal::NavigationSubWidget::factoryIndexChanged, this, &NavigationWidget::onSubWidgetFactoryIndexChanged);
  insertWidget(position, nsw);

  d->m_subWidgets.insert(position, nsw);
  d->m_subWidgets.at(0)->setCloseIcon(closeIconForSide(d->m_side, static_cast<int>(d->m_subWidgets.size())));
  NavigationWidgetPrivate::updateActivationsMap(nsw->factory()->id(), {d->m_side, position});

  return nsw;
}

auto NavigationWidget::activateSubWidget(const Id factory_id, const int preferred_position) -> QWidget*
{
  setShown(true);

  for(const auto sub_widget: d->m_subWidgets) {
    if (sub_widget->factory()->id() == factory_id) {
      sub_widget->setFocusWidget();
      ICore::raiseWindow(this);
      return sub_widget->widget();
    }
  }

  if (const auto index = factoryIndex(factory_id); index >= 0) {
    const auto preferred_index_valid = 0 <= preferred_position && preferred_position < d->m_subWidgets.count();
    const auto activation_index = preferred_index_valid ? preferred_position : 0;
    const auto sub_widget = d->m_subWidgets.at(activation_index);
    sub_widget->setFactoryIndex(index);
    sub_widget->setFocusWidget();
    ICore::raiseWindow(this);
    return sub_widget->widget();
  }

  return nullptr;
}

auto NavigationWidget::splitSubWidget(const int factory_index) -> void
{
  const auto original = qobject_cast<Internal::NavigationSubWidget*>(sender());
  const auto pos = indexOf(original) + 1;
  insertSubItem(pos, factory_index);
}

auto NavigationWidget::closeSubWidget() -> void
{
  if (d->m_subWidgets.count() != 1) {
    const auto sub_widget = qobject_cast<Internal::NavigationSubWidget*>(sender());
    sub_widget->saveSettings();
    const auto position = static_cast<int>(d->m_subWidgets.indexOf(sub_widget));
    for (auto pos = position + 1; pos < d->m_subWidgets.size(); ++pos) {
      const auto nsw = d->m_subWidgets.at(pos);
      nsw->setPosition(pos - 1);
      NavigationWidgetPrivate::updateActivationsMap(nsw->factory()->id(), {d->m_side, pos - 1});
    }
    d->m_subWidgets.removeOne(sub_widget);
    sub_widget->hide();
    sub_widget->deleteLater();
    // update close button of top item
    if (!d->m_subWidgets.isEmpty())
      d->m_subWidgets.at(0)->setCloseIcon(closeIconForSide(d->m_side, d->m_subWidgets.size()));
  } else {
    setShown(false);
  }
}

static auto defaultFirstView(const Side side) -> QString
{
  return side == Side::Left ? QString("Projects") : QString("Outline");
}

static auto defaultVisible(const Side side) -> bool
{
  return side == Side::Left;
}

auto NavigationWidget::saveSettings(QtcSettings *settings) const -> void
{
  QStringList view_ids;

  for (auto i = 0; i < d->m_subWidgets.count(); ++i) {
    d->m_subWidgets.at(i)->saveSettings();
    view_ids.append(d->m_subWidgets.at(i)->factory()->id().toString());
  }

  settings->setValueWithDefault(settingsKey("Views"), view_ids, {defaultFirstView(d->m_side)});
  settings->setValueWithDefault(settingsKey("Visible"), isShown(), defaultVisible(d->m_side));
  settings->setValue(settingsKey("VerticalPosition"), saveState());
  settings->setValue(settingsKey("Width"), d->m_width);

  const auto activation_key = QStringLiteral("ActivationPosition.");
  for (const auto keys = NavigationWidgetPrivate::s_activations_map.keys(); const auto &factory_id : keys) {
    if (const auto & [side, position] = NavigationWidgetPrivate::s_activations_map[factory_id]; side == d->m_side)
      settings->setValue(settingsKey(activation_key + factory_id.toString()), position);
  }
}

auto NavigationWidget::restoreSettings(QSettings *settings) -> void
{
  if (!d->m_factoryModel->rowCount()) {
    // We have no widgets to show!
    setShown(false);
    return;
  }

  const auto is_left_side = d->m_side == Side::Left;
  auto view_ids = settings->value(settingsKey("Views"), QStringList(defaultFirstView(d->m_side))).toStringList();
  auto restore_splitter_state = true;

  if (const auto version = settings->value(settingsKey("Version"), 1).toInt(); version == 1) {
    if (const auto default_second_view = is_left_side ? QLatin1String("Open Documents") : QLatin1String("Bookmarks"); !view_ids.contains(default_second_view)) {
      view_ids += default_second_view;
      restore_splitter_state = false;
    }
    settings->setValue(settingsKey("Version"), 2);
  }

  auto position = 0;

  for (const auto &id : view_ids) {
    if (const auto index = factoryIndex(Id::fromString(id)); index >= 0) {
      // Only add if the id was actually found!
      insertSubItem(position, index);
      ++position;
    } else {
      restore_splitter_state = false;
    }
  }

  if (d->m_subWidgets.isEmpty())
    // Make sure we have at least the projects widget or outline widget
    insertSubItem(0, qMax(0, factoryIndex(Id::fromString(defaultFirstView(d->m_side)))));

  setShown(settings->value(settingsKey("Visible"), defaultVisible(d->m_side)).toBool());

  if (restore_splitter_state && settings->contains(settingsKey("VerticalPosition"))) {
    restoreState(settings->value(settingsKey("VerticalPosition")).toByteArray());
  } else {
    QList<int> sizes;
    sizes += 256;
    for (auto i = view_ids.size() - 1; i > 0; --i)
      sizes.prepend(512);
    setSizes(sizes);
  }

  d->m_width = settings->value(settingsKey("Width"), 240).toInt();
  if (d->m_width < 40)
    d->m_width = 40;

  // Apply
  if (NavigationWidgetPlaceHolder::current(d->m_side))
    NavigationWidgetPlaceHolder::current(d->m_side)->applyStoredSize();

  // Restore last activation positions
  settings->beginGroup(settingsGroup());
  const auto activation_key = QStringLiteral("ActivationPosition.");

  for (const auto keys = settings->allKeys(); const auto &key : keys) {
    if (!key.startsWith(activation_key))
      continue;
    const auto position = settings->value(key).toInt();
    const auto factory_id = Id::fromString(key.mid(activation_key.length()));
    NavigationWidgetPrivate::updateActivationsMap(factory_id, {d->m_side, position});
  }

  settings->endGroup();
}

auto NavigationWidget::closeSubWidgets() const -> void
{
  for(const auto sub_widget: d->m_subWidgets) {
    sub_widget->saveSettings();
    delete sub_widget;
  }
  d->m_subWidgets.clear();
}

auto NavigationWidget::setShown(const bool b) const -> void
{
  if (d->m_shown == b)
    return;

  const bool have_data = d->m_factoryModel->rowCount();
  d->m_shown = b;

  if (const auto current = NavigationWidgetPlaceHolder::current(d->m_side)) {
    const auto visible = d->m_shown && have_data;
    current->setVisible(visible);
    d->m_toggleSideBarAction->setChecked(visible);
  } else {
    d->m_toggleSideBarAction->setChecked(false);
  }

  updateToggleText();
}

auto NavigationWidget::isShown() const -> bool
{
  return d->m_shown;
}

auto NavigationWidget::factoryIndex(const Id id) const -> int
{
  for (auto row = 0; row < d->m_factoryModel->rowCount(); ++row) {
    if (d->m_factoryModel->data(d->m_factoryModel->index(row, 0), FactoryIdRole).value<Id>() == id)
      return row;
  }

  return -1;
}

auto NavigationWidget::settingsKey(const QString &key) const -> QString
{
  return QStringLiteral("%1/%2").arg(settingsGroup(), key);
}

auto NavigationWidget::onSubWidgetFactoryIndexChanged(int factory_index) const -> void
{
  Q_UNUSED(factory_index)
  const auto sub_widget = qobject_cast<Internal::NavigationSubWidget*>(sender());
  QTC_ASSERT(sub_widget, return);
  const auto factory_id = sub_widget->factory()->id();
  NavigationWidgetPrivate::updateActivationsMap(factory_id, {d->m_side, sub_widget->position()});
}

auto NavigationWidget::commandMap() const -> QHash<Id, Command*>
{
  return d->m_commandMap;
}

} // namespace Core
