// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-mode-manager.hpp"

#include "core-action-manager.hpp"
#include "core-command.hpp"
#include "core-fancy-action-bar.hpp"
#include "core-fancy-tab-widget.hpp"
#include "core-interface.hpp"
#include "core-main-window.hpp"
#include "core-mode-interface.hpp"

#include <utils/algorithm.hpp>
#include <utils/qtcassert.hpp>

#include <QAction>
#include <QDebug>
#include <QMap>
#include <QMouseEvent>

using namespace Utils;

namespace Orca::Plugin::Core {

/*!
    \class Orca::Plugin::Core::ModeManager
    \inheaderfile coreplugin/modemanager.h
    \ingroup mainclasses
    \inmodule Orca

    \brief The ModeManager class manages the activation of modes and the
    actions in the mode selector's tool bar.

    Modes are implemented with the IMode class. Use the ModeManager to
    force activation of a mode, or to be notified when the active mode changed.

    The ModeManager also manages the actions that are visible in the mode
    selector's toolbar. Adding actions to the tool bar should be done very
    sparingly.
*/

/*!
    \enum ModeManager::Style
    \internal
*/

/*!
    \fn void ModeManager::currentModeAboutToChange(Utils::Id mode)

    Emitted before the current mode changes to \a mode.
*/

/*!
    \fn void ModeManager::currentModeChanged(Utils::Id mode, Utils::Id oldMode)

    Emitted after the current mode changed from \a oldMode to \a mode.
*/

struct ModeManagerPrivate {
  auto showMenu(int index, const QMouseEvent *event) const -> void;
  auto appendMode(IMode *mode) -> void;
  static auto enabledStateChanged(IMode *mode) -> void;
  auto activateModeHelper(Id id) -> void;
  auto extensionsInitializedHelper() -> void;

  MainWindow *m_main_window{};
  FancyTabWidget *m_mode_stack{};
  FancyActionBar *m_action_bar{};
  QMap<QAction*, int> m_actions;
  QVector<IMode*> m_modes;
  QVector<Command*> m_mode_commands;
  Context m_added_contexts;
  int m_old_current{};
  ModeManager::Style m_mode_style = ModeManager::Style::IconsAndText;
  bool m_starting_up = true;
  Id m_pending_first_active_mode; // Valid before extentionsInitialized.
};

static ModeManagerPrivate *d;
static ModeManager *m_instance = nullptr;

static auto indexOf(const Id id) -> int
{
  for (auto i = 0; i < d->m_modes.count(); ++i) {
    if (d->m_modes.at(i)->id() == id)
      return i;
  }

  qDebug() << "Warning, no such mode:" << id.toString();
  return -1;
}

auto ModeManagerPrivate::showMenu(const int index, const QMouseEvent *event) const -> void
{
  QTC_ASSERT(m_modes.at(index)->menu(), return);
  m_modes.at(index)->menu()->popup(event->globalPos());
}

ModeManager::ModeManager(MainWindow *main_window, FancyTabWidget *modeStack)
{
  m_instance = this;
  d = new ModeManagerPrivate();
  d->m_main_window = main_window;
  d->m_mode_stack = modeStack;
  d->m_old_current = -1;
  d->m_action_bar = new FancyActionBar(modeStack);
  d->m_mode_stack->addCornerWidget(d->m_action_bar);

  setModeStyle(d->m_mode_style);
  connect(d->m_mode_stack, &FancyTabWidget::currentAboutToShow, this, &ModeManager::currentTabAboutToChange);
  connect(d->m_mode_stack, &FancyTabWidget::currentChanged, this, &ModeManager::currentTabChanged);
  connect(d->m_mode_stack, &FancyTabWidget::menuTriggered, this, [](const int index, const QMouseEvent *e) { d->showMenu(index, e); });
}

ModeManager::~ModeManager()
{
  delete d;
  d = nullptr;
  m_instance = nullptr;
}

/*!
    Returns the id of the current mode.

    \sa activateMode()
    \sa currentMode()
*/
auto ModeManager::currentModeId() -> Id
{
  const auto current_index = d->m_mode_stack->currentIndex();

  if (current_index < 0)
    return {};

  return d->m_modes.at(current_index)->id();
}

static auto findMode(const Id id) -> IMode*
{
  if (const auto index = indexOf(id); index >= 0)
    return d->m_modes.at(index);

  return nullptr;
}

/*!
    Makes the mode with ID \a id the current mode.

    \sa currentMode()
    \sa currentModeId()
    \sa currentModeAboutToChange()
    \sa currentModeChanged()
*/
auto ModeManager::activateMode(const Id id) -> void
{
  d->activateModeHelper(id);
}

auto ModeManagerPrivate::activateModeHelper(const Id id) -> void
{
  if (m_starting_up) {
    m_pending_first_active_mode = id;
  } else {
    const auto current_index = m_mode_stack->currentIndex();
    if (const auto new_index = indexOf(id); new_index != current_index && new_index >= 0)
      m_mode_stack->setCurrentIndex(new_index);
  }
}

auto ModeManager::extensionsInitialized() -> void
{
  d->extensionsInitializedHelper();
}

auto ModeManagerPrivate::extensionsInitializedHelper() -> void
{
  m_starting_up = false;

  sort(m_modes, &IMode::priority);
  std::ranges::reverse(m_modes);

  for (const auto mode : qAsConst(m_modes))
    appendMode(mode);

  if (m_pending_first_active_mode.isValid())
    activateModeHelper(m_pending_first_active_mode);
}

auto ModeManager::addMode(IMode *mode) -> void
{
  QTC_ASSERT(d->m_starting_up, return);
  d->m_modes.append(mode);
}

auto ModeManagerPrivate::appendMode(IMode *mode) -> void
{
  const auto index = static_cast<int>(m_mode_commands.count());

  m_main_window->addContextObject(mode);
  m_mode_stack->insertTab(index, mode->widget(), mode->icon(), mode->displayName(), mode->menu() != nullptr);
  m_mode_stack->setTabEnabled(index, mode->isEnabled());

  // Register mode shortcut
  const auto action_id = mode->id().withPrefix("Orca.Mode.");
  const auto action = new QAction(ModeManager::tr("Switch to <b>%1</b> mode").arg(mode->displayName()), m_instance);
  auto cmd = ActionManager::registerAction(action, action_id);
  cmd->setDefaultKeySequence(QKeySequence(use_mac_shortcuts ? QString("Meta+%1").arg(index + 1) : QString("Ctrl+%1").arg(index + 1)));

  m_mode_commands.append(cmd);
  m_mode_stack->setTabToolTip(index, cmd->action()->toolTip());
  QObject::connect(cmd, &Command::keySequenceChanged, m_instance, [cmd, index, this] {
    m_mode_stack->setTabToolTip(index, cmd->action()->toolTip());
  });

  auto id = mode->id();
  QObject::connect(action, &QAction::triggered, [this, id] {
    ModeManager::activateMode(id);
    ICore::raiseWindow(m_mode_stack);
  });

  QObject::connect(mode, &IMode::enabledStateChanged, [this, mode] { enabledStateChanged(mode); });
}

auto ModeManager::removeMode(IMode *mode) -> void
{
  const auto index = static_cast<int>(d->m_modes.indexOf(mode));

  if (index >= d->m_modes.size() - 1 && d->m_modes.size() > 1)
    d->m_mode_stack->setCurrentIndex(static_cast<int>(d->m_modes.size() - 2));

  d->m_modes.remove(index);

  if (d->m_starting_up)
    return;

  d->m_mode_commands.remove(index);
  d->m_mode_stack->removeTab(index);
  d->m_main_window->removeContextObject(mode);
}

auto ModeManagerPrivate::enabledStateChanged(IMode *mode) -> void
{
  const auto index = static_cast<int>(d->m_modes.indexOf(mode));
  QTC_ASSERT(index >= 0, return);
  d->m_mode_stack->setTabEnabled(index, mode->isEnabled());

  // Make sure we leave any disabled mode to prevent possible crashes:
  if (mode->id() == ModeManager::currentModeId() && !mode->isEnabled()) {
    // This assumes that there is always at least one enabled mode.
    for (auto i = 0; i < d->m_modes.count(); ++i) {
      if (d->m_modes.at(i) != mode && d->m_modes.at(i)->isEnabled()) {
        ModeManager::activateMode(d->m_modes.at(i)->id());
        break;
      }
    }
  }
}

/*!
    Adds the \a action to the mode selector's tool bar.
    Actions are sorted by \a priority in descending order.
    Use this functionality very sparingly.
*/
auto ModeManager::addAction(QAction *action, const int priority) -> void
{
  d->m_actions.insert(action, priority);

  // Count the number of commands with a higher priority
  auto index = 0;
  for(const auto p: d->m_actions) {
    if (p > priority)
      ++index;
  }

  d->m_action_bar->insertAction(index, action);
}

/*!
    \internal
*/
auto ModeManager::addProjectSelector(QAction *action) -> void
{
  d->m_action_bar->addProjectSelector(action);
  d->m_actions.insert(nullptr, INT_MAX);
}

auto ModeManager::currentTabAboutToChange(const int index) -> void
{
  if (index >= 0) {
    if (const auto mode = d->m_modes.at(index)) emit currentModeAboutToChange(mode->id());
  }
}

auto ModeManager::currentTabChanged(const int index) -> void
{
  // Tab index changes to -1 when there is no tab left.
  if (index < 0)
    return;

  const auto mode = d->m_modes.at(index);

  if (!mode)
    return;

  // FIXME: This hardcoded context update is required for the Debug and Edit modes, since
  // they use the editor widget, which is already a context widget so the main window won't
  // go further up the parent tree to find the mode context.
  ICore::updateAdditionalContexts(d->m_added_contexts, mode->context());
  d->m_added_contexts = mode->context();

  const IMode *old_mode = nullptr;

  if (d->m_old_current >= 0)
    old_mode = d->m_modes.at(d->m_old_current);

  d->m_old_current = index;
  emit currentModeChanged(mode->id(), old_mode ? old_mode->id() : Id());
}

/*!
    \internal
*/
auto ModeManager::setFocusToCurrentMode() -> void
{
  const auto mode = findMode(currentModeId());
  QTC_ASSERT(mode, return);

  if (const auto widget = mode->widget()) {
    auto focus_widget = widget->focusWidget();
    if (!focus_widget)
      focus_widget = widget;
    focus_widget->setFocus();
  }
}

/*!
    \internal
*/
auto ModeManager::setModeStyle(const Style style) -> void
{
  const auto visible = style != Style::Hidden;
  const auto icons_only = style == Style::IconsOnly;

  d->m_mode_style = style;
  d->m_action_bar->setIconsOnly(icons_only);
  d->m_mode_stack->setIconsOnly(icons_only);
  d->m_mode_stack->setSelectionWidgetVisible(visible);
}

/*!
    \internal
*/
auto ModeManager::cycleModeStyle() -> void
{
  const auto next_style = static_cast<Style>((static_cast<int>(modeStyle()) + 1) % 3);
  setModeStyle(next_style);
}

/*!
    \internal
*/
auto ModeManager::modeStyle() -> Style
{
  return d->m_mode_style;
}

/*!
    Returns the pointer to the instance. Only use for connecting to signals.
*/
auto ModeManager::instance() -> ModeManager*
{
  return m_instance;
}

/*!
    Returns a pointer to the current mode.

    \sa activateMode()
    \sa currentModeId()
*/
auto ModeManager::currentMode() -> IMode*
{
  const auto current_index = d->m_mode_stack->currentIndex();
  return current_index < 0 ? nullptr : d->m_modes.at(current_index);
}

} // namespace Orca::Plugin::Core
