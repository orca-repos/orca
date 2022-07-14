// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-navigation-sub-widget.hpp"

#include "core-action-manager.hpp"
#include "core-command.hpp"
#include "core-interface.hpp"
#include "core-navigation-widget-factory-interface.hpp"
#include "core-navigation-widget.hpp"

#include <utils/styledbar.hpp>
#include <utils/utilsicons.hpp>

#include <QHBoxLayout>
#include <QMenu>
#include <QResizeEvent>
#include <QToolButton>

Q_DECLARE_METATYPE(Orca::Plugin::Core::INavigationWidgetFactory *)

using namespace Utils;

namespace Orca::Plugin::Core {

////
// NavigationSubWidget
////

NavigationSubWidget::NavigationSubWidget(NavigationWidget *parent_widget, const int position, const int factory_index) : QWidget(parent_widget), m_parent_widget(parent_widget), m_position(position)
{
  m_navigation_combo_box = new NavComboBox(this);
  m_navigation_combo_box->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  m_navigation_combo_box->setFocusPolicy(Qt::TabFocus);
  m_navigation_combo_box->setMinimumContentsLength(0);
  m_navigation_combo_box->setModel(parent_widget->factoryModel());
  m_navigation_widget = nullptr;
  m_navigation_widget_factory = nullptr;

  m_tool_bar = new StyledBar(this);
  const auto tool_bar_layout = new QHBoxLayout;
  tool_bar_layout->setContentsMargins(0, 0, 0, 0);
  tool_bar_layout->setSpacing(0);
  m_tool_bar->setLayout(tool_bar_layout);
  tool_bar_layout->addWidget(m_navigation_combo_box);

  const auto split_action = new QToolButton();
  split_action->setIcon(Icons::SPLIT_HORIZONTAL_TOOLBAR.icon());
  split_action->setToolTip(tr("Split"));
  split_action->setPopupMode(QToolButton::InstantPopup);
  split_action->setProperty("noArrow", true);
  m_split_menu = new QMenu(split_action);
  split_action->setMenu(m_split_menu);
  connect(m_split_menu, &QMenu::aboutToShow, this, &NavigationSubWidget::populateSplitMenu);

  m_close_button = new QToolButton();
  m_close_button->setIcon(Icons::CLOSE_SPLIT_BOTTOM.icon());
  m_close_button->setToolTip(tr("Close"));

  tool_bar_layout->addWidget(split_action);
  tool_bar_layout->addWidget(m_close_button);

  const auto lay = new QVBoxLayout();
  lay->setContentsMargins(0, 0, 0, 0);
  lay->setSpacing(0);
  setLayout(lay);
  lay->addWidget(m_tool_bar);

  connect(m_close_button, &QAbstractButton::clicked, this, &NavigationSubWidget::closeMe);
  setFactoryIndex(factory_index);
  connect(m_navigation_combo_box, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &NavigationSubWidget::comboBoxIndexChanged);
  comboBoxIndexChanged(factory_index);
}

NavigationSubWidget::~NavigationSubWidget() = default;

auto NavigationSubWidget::comboBoxIndexChanged(const int factory_index) -> void
{
  saveSettings();

  // Remove toolbutton
  for(const auto &w: m_additional_tool_bar_widgets)
    delete w;

  m_additional_tool_bar_widgets.clear();

  // Remove old Widget
  delete m_navigation_widget;
  m_navigation_widget = nullptr;
  m_navigation_widget_factory = nullptr;

  if (factory_index == -1)
    return;

  // Get new stuff
  m_navigation_widget_factory = m_navigation_combo_box->itemData(factory_index, NavigationWidget::FactoryObjectRole).value<INavigationWidgetFactory*>();
  const auto [widget, dock_tool_bar_widgets] = m_navigation_widget_factory->createWidget();
  m_navigation_widget = widget;
  layout()->addWidget(m_navigation_widget);

  // Add Toolbutton
  m_additional_tool_bar_widgets = dock_tool_bar_widgets;
  const auto layout = qobject_cast<QHBoxLayout*>(m_tool_bar->layout());
  for(const auto &w: m_additional_tool_bar_widgets) {
    layout->insertWidget(layout->count() - 2, w);
  }

  restoreSettings();
  emit factoryIndexChanged(factory_index);
}

auto NavigationSubWidget::populateSplitMenu() -> void
{
  m_split_menu->clear();
  const auto factory_model = m_parent_widget->factoryModel();
  const auto count = factory_model->rowCount();

  for (auto i = 0; i < count; ++i) {
    auto index = factory_model->index(i, 0);
    const auto id = factory_model->data(index, NavigationWidget::FactoryActionIdRole).value<Id>();
    const auto command = ActionManager::command(id);
    const auto factory_name = factory_model->data(index).toString();
    const auto display_name = command->keySequence().isEmpty() ? factory_name : QString("%1 (%2)").arg(factory_name, command->keySequence().toString(QKeySequence::NativeText));
    const auto action = m_split_menu->addAction(display_name);
    connect(action, &QAction::triggered, this, [this, i] { emit splitMe(i); });
  }
}

auto NavigationSubWidget::setFocusWidget() const -> void
{
  if (m_navigation_widget)
    m_navigation_widget->setFocus();
}

auto NavigationSubWidget::factory() const -> INavigationWidgetFactory*
{
  return m_navigation_widget_factory;
}

auto NavigationSubWidget::saveSettings() const -> void
{
  if (!m_navigation_widget || !factory())
    return;

  const auto settings = ICore::settings();
  settings->beginGroup(m_parent_widget->settingsGroup());
  factory()->saveSettings(settings, position(), m_navigation_widget);
  settings->endGroup();
}

auto NavigationSubWidget::restoreSettings() const -> void
{
  if (!m_navigation_widget || !factory())
    return;

  QSettings *settings = ICore::settings();
  settings->beginGroup(m_parent_widget->settingsGroup());
  factory()->restoreSettings(settings, position(), m_navigation_widget);
  settings->endGroup();
}

auto NavigationSubWidget::command(const QString &title) const -> Command*
{
  const auto command_map = m_parent_widget->commandMap();

  if (const auto r = command_map.find(Id::fromString(title)); r != command_map.end())
    return r.value();

  return nullptr;
}

auto NavigationSubWidget::setCloseIcon(const QIcon &icon) const -> void
{
  m_close_button->setIcon(icon);
}

auto NavigationSubWidget::widget() const -> QWidget*
{
  return m_navigation_widget;
}

auto NavigationSubWidget::factoryIndex() const -> int
{
  return m_navigation_combo_box->currentIndex();
}

auto NavigationSubWidget::setFactoryIndex(const int i) const -> void
{
  m_navigation_combo_box->setCurrentIndex(i);
}

auto NavigationSubWidget::position() const -> int
{
  return m_position;
}

auto NavigationSubWidget::setPosition(const int position) -> void
{
  m_position = position;
}

CommandComboBox::CommandComboBox(QWidget *parent) : QComboBox(parent) {}

auto CommandComboBox::event(QEvent *event) -> bool
{
  if (event->type() == QEvent::ToolTip) {
    const auto text = currentText();
    if (const auto cmd = command(text)) {
      const auto tooltip = tr("Activate %1 View").arg(text);
      setToolTip(cmd->stringWithAppendedShortcut(tooltip));
    } else {
      setToolTip(text);
    }
  }
  return QComboBox::event(event);
}

} // namespace Orca::Plugin::Core
