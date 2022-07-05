// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "actionmanager.h"
#include "actionmanager_p.h"
#include "actioncontainer_p.h"
#include "command_p.h"

#include <core/icore.h>

#include <utils/algorithm.h>
#include <utils/fadingindicator.h>
#include <utils/qtcassert.h>

#include <QAction>
#include <QApplication>
#include <QDebug>
#include <QMenuBar>
#include <QSettings>

namespace Core {
namespace {
enum class warn_about {
  find_failures = 0
};
} // namespace

using namespace Internal;
using namespace Utils;

static constexpr char k_keyboard_settings_key_v2[] = "KeyboardShortcutsV2";

/*!
    \class Core::ActionManager
    \inheaderfile coreplugin/actionmanager/actionmanager.h
    \ingroup mainclasses
    \inmodule Orca

    \brief The ActionManager class is responsible for registration of menus and
    menu items and keyboard shortcuts.

    The action manager is the central bookkeeper of actions and their shortcuts
    and layout. It is a singleton containing mostly static functions. If you
    need access to the instance, for example for connecting to signals, call
    its ActionManager::instance() function.

    The action manager makes it possible to provide a central place where the
    users can specify all their keyboard shortcuts, and provides a solution for
    actions that should behave differently in different contexts (like the
    copy/replace/undo/redo actions).

    See \l{The Action Manager and Commands} for an overview of the interaction
    between Core::ActionManager, Core::Command, and Core::Context.

    Register a globally active action "My Action" by putting the following in
    your plugin's ExtensionSystem::IPlugin::initialize() function.

    \code
        QAction *myAction = new QAction(tr("My Action"), this);
        Command *cmd = ActionManager::registerAction(myAction, "myplugin.myaction", Context(C_GLOBAL));
        cmd->setDefaultKeySequence(QKeySequence(tr("Ctrl+Alt+u")));
        connect(myAction, &QAction::triggered, this, &MyPlugin::performMyAction);
    \endcode

    The \c connect is done to your own QAction instance. If you create for
    example a tool button that should represent the action, add the action from
    Command::action() to it.

    \code
        QToolButton *myButton = new QToolButton(someParentWidget);
        myButton->setDefaultAction(cmd->action());
    \endcode

    Also use the action manager to add items to registered action containers
    like the application's menu bar or menus in that menu bar. Register your
    action via the Core::ActionManager::registerAction() function, get the
    action container for a specific ID (as specified for example in the
    Core::Constants namespace) with Core::ActionManager::actionContainer(), and
    add your command to this container.

    Building on the example, adding "My Action" to the "Tools" menu would be
    done with

    \code
        ActionManager::actionContainer(Core::Constants::M_TOOLS)->addAction(cmd);
    \endcode

    \sa Core::ICore
    \sa Core::Command
    \sa Core::ActionContainer
    \sa Core::IContext
    \sa {The Action Manager and Commands}
*/

/*!
    \fn void Core::ActionManager::commandListChanged()

    Emitted when the command list has changed.
*/

/*!
    \fn void Core::ActionManager::commandAdded(Utils::Id id)

    Emitted when a command (with the \a id) is added.
*/

static ActionManager *m_instance = nullptr;
static ActionManagerPrivate *d;

/*!
    \internal
*/
ActionManager::ActionManager(QObject *parent) : QObject(parent)
{
  m_instance = this;
  d = new ActionManagerPrivate;
  if constexpr (HostOsInfo::isMacHost())
    QCoreApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
}

/*!
    \internal
*/
ActionManager::~ActionManager()
{
  delete d;
}

/*!
    Returns the pointer to the instance. Only use for connecting to signals.
*/
auto ActionManager::instance() -> ActionManager*
{
  return m_instance;
}

/*!
    Creates a new menu action container or returns an existing container with
    the specified \a id. The ActionManager owns the returned ActionContainer.
    Add your menu to some other menu or a menu bar via the actionContainer()
    and ActionContainer::addMenu() functions.

    \sa actionContainer()
    \sa ActionContainer::addMenu()
*/
auto ActionManager::createMenu(const Id id) -> ActionContainer*
{
  if (const auto it = d->m_id_container_map.constFind(id); it != d->m_id_container_map.constEnd())
    return it.value();

  const auto mc = new MenuActionContainer(id);

  d->m_id_container_map.insert(id, mc);
  connect(mc, &QObject::destroyed, d, &ActionManagerPrivate::containerDestroyed);

  return mc;
}

/*!
    Creates a new menu bar action container or returns an existing container
    with the specified \a id. The ActionManager owns the returned
    ActionContainer.

    \sa createMenu()
    \sa ActionContainer::addMenu()
*/
auto ActionManager::createMenuBar(const Id id) -> ActionContainer*
{
  if (const auto it = d->m_id_container_map.constFind(id); it != d->m_id_container_map.constEnd())
    return it.value();

  const auto mb = new QMenuBar; // No parent (System menu bar on macOS)
  mb->setObjectName(id.toString());

  const auto mbc = new MenuBarActionContainer(id);
  mbc->setMenuBar(mb);

  d->m_id_container_map.insert(id, mbc);
  connect(mbc, &QObject::destroyed, d, &ActionManagerPrivate::containerDestroyed);

  return mbc;
}

/*!
    Creates a new (sub) touch bar action container or returns an existing
    container with the specified \a id. The ActionManager owns the returned
    ActionContainer.

    Note that it is only possible to create a single level of sub touch bars.
    The sub touch bar will be represented as a button with \a icon and \a text
    (either of which can be left empty), which opens the sub touch bar when
    touched.

    \sa actionContainer()
    \sa ActionContainer::addMenu()
*/
auto ActionManager::createTouchBar(const Id id, const QIcon &icon, const QString &text) -> ActionContainer*
{
  QTC_CHECK(!icon.isNull() || !text.isEmpty());

  if (ActionContainer *const c = d->m_id_container_map.value(id))
    return c;

  const auto ac = new TouchBarActionContainer(id, icon, text);
  d->m_id_container_map.insert(id, ac);

  connect(ac, &QObject::destroyed, d, &ActionManagerPrivate::containerDestroyed);
  return ac;
}

/*!
    Makes an \a action known to the system under the specified \a id.

    Returns a Command instance that represents the action in the application
    and is owned by the ActionManager. You can register several actions with
    the same \a id as long as the \a context is different. In this case
    triggering the action is forwarded to the registered QAction for the
    currently active context. If the optional \a context argument is not
    specified, the global context will be assumed. A \a scriptable action can
    be called from a script without the need for the user to interact with it.
*/
auto ActionManager::registerAction(QAction *action, const Id id, const Context &context, const bool scriptable) -> Command*
{
  const auto cmd = d->overridableAction(id);

  if (cmd) {
    cmd->d->addOverrideAction(action, context, scriptable);
    emit m_instance->commandListChanged();
    emit m_instance->commandAdded(id);
  }

  return cmd;
}

/*!
    Returns the Command instance that has been created with registerAction()
    for the specified \a id.

    \sa registerAction()
*/
auto ActionManager::command(const Id id) -> Command*
{
  const auto it = d->m_id_cmd_map.constFind(id);
  if (it == d->m_id_cmd_map.constEnd()) {
    if constexpr (static_cast<bool>(warn_about::find_failures))
      qWarning() << "ActionManagerPrivate::command(): failed to find :" << id.name();
    return nullptr;
  }
  return it.value();
}

/*!
    Returns the ActionContainter instance that has been created with
    createMenu(), createMenuBar(), createTouchBar() for the specified \a id.

    Use the ID \c{Core::Constants::MENU_BAR} to retrieve the main menu bar.

    Use the IDs \c{Core::Constants::M_FILE}, \c{Core::Constants::M_EDIT}, and
    similar constants to retrieve the various default menus.

    Use the ID \c{Core::Constants::TOUCH_BAR} to retrieve the main touch bar.

    \sa ActionManager::createMenu()
    \sa ActionManager::createMenuBar()
*/
auto ActionManager::actionContainer(const Id id) -> ActionContainer*
{
  const auto it = d->m_id_container_map.constFind(id);
  if (it == d->m_id_container_map.constEnd()) {
    if constexpr (static_cast<bool>(warn_about::find_failures))
      qWarning() << "ActionManagerPrivate::actionContainer(): failed to find :" << id.name();
    return nullptr;
  }
  return it.value();
}

/*!
    Returns all registered commands.
*/
auto ActionManager::commands() -> QList<Command*>
{
  return d->m_id_cmd_map.values();
}

/*!
    Removes the knowledge about an \a action under the specified \a id.

    Usually you do not need to unregister actions. The only valid use case for unregistering
    actions, is for actions that represent user definable actions, like for the custom Locator
    filters. If the user removes such an action, it also has to be unregistered from the action manager,
    to make it disappear from shortcut settings etc.
*/
auto ActionManager::unregisterAction(QAction *action, const Id id) -> void
{
  const auto cmd = d->m_id_cmd_map.value(id, nullptr);

  if (!cmd) {
    qWarning() << "unregisterAction: id" << id.name() << "is registered with a different command type.";
    return;
  }

  cmd->d->removeOverrideAction(action);

  if (cmd->d->isEmpty()) {
    // clean up
    ActionManagerPrivate::saveSettings(cmd);
    ICore::mainWindow()->removeAction(cmd->action());
    // ActionContainers listen to the commands' destroyed signals
    delete cmd->action();
    d->m_id_cmd_map.remove(id);
    delete cmd;
  }

  emit m_instance->commandListChanged();
}

/*!
    \internal
*/
auto ActionManager::setPresentationModeEnabled(const bool enabled) -> void
{
  if (enabled == isPresentationModeEnabled())
    return;

  // Signal/slots to commands:
  for (const auto command_list = commands(); const auto command : command_list) {
    if (command->action()) {
      if (enabled)
        connect(command->action(), &QAction::triggered, d, &ActionManagerPrivate::actionTriggered);
      else
        disconnect(command->action(), &QAction::triggered, d, &ActionManagerPrivate::actionTriggered);
    }
  }

  d->m_presentation_mode_enabled = enabled;
}

/*!
    Returns whether presentation mode is enabled.

    The presentation mode is enabled when starting \QC with the command line
    argument \c{-presentationMode}. In presentation mode, \QC displays any
    pressed shortcut in an overlay box.
*/
auto ActionManager::isPresentationModeEnabled() -> bool
{
  return d->m_presentation_mode_enabled;
}

/*!
    Decorates the specified \a text with a numbered accelerator key \a number,
    in the style of the \uicontrol {Recent Files} menu.
*/
auto ActionManager::withNumberAccelerator(const QString &text, const int number) -> QString
{
  if constexpr (HostOsInfo::isMacHost())
    return text;

  if (number > 9)
    return text;

  return QString("&%1 | %2").arg(number).arg(text);
}

/*!
    \internal
*/
auto ActionManager::saveSettings() -> void
{
  d->saveSettings();
}

/*!
    \internal
*/
auto ActionManager::setContext(const Context &context) -> void
{
  d->setContext(context);
}

/*!
    \class ActionManagerPrivate
    \inheaderfile actionmanager_p.h
    \internal
*/

ActionManagerPrivate::~ActionManagerPrivate()
{
  // first delete containers to avoid them reacting to command deletion
  for (const auto container : qAsConst(m_id_container_map))
    disconnect(container, &QObject::destroyed, this, &ActionManagerPrivate::containerDestroyed);

  qDeleteAll(m_id_container_map);
  qDeleteAll(m_id_cmd_map);
}

auto ActionManagerPrivate::setContext(const Context &context) -> void
{
  // here are possibilities for speed optimization if necessary:
  // let commands (de-)register themselves for contexts
  // and only update commands that are either in old or new contexts
  m_context = context;
  const auto cmdcend = m_id_cmd_map.constEnd();
  for (auto it = m_id_cmd_map.constBegin(); it != cmdcend; ++it)
    it.value()->d->setCurrentContext(m_context);
}

auto ActionManagerPrivate::hasContext(const Context &context) const -> bool
{
  return std::ranges::any_of(m_context, [&context](const Id &id) {
    return context.contains(id);
  });
}

auto ActionManagerPrivate::containerDestroyed() -> void
{
  const auto container = dynamic_cast<ActionContainerPrivate*>(sender());
  m_id_container_map.remove(m_id_container_map.key(container));
}

auto ActionManagerPrivate::actionTriggered() const -> void
{
  if (const auto action = qobject_cast<QAction*>(sender()))
    showShortcutPopup(action->shortcut().toString());
}

auto ActionManagerPrivate::showShortcutPopup(const QString &shortcut) -> void
{
  if (shortcut.isEmpty() || !ActionManager::isPresentationModeEnabled())
    return;

  auto window = QApplication::activeWindow();
  if (!window) {
    if (!QApplication::topLevelWidgets().isEmpty()) {
      window = QApplication::topLevelWidgets().first();
    } else {
      window = ICore::mainWindow();
    }
  }

  FadingIndicator::showText(window, shortcut);
}

auto ActionManagerPrivate::overridableAction(const Id id) -> Command*
{
  auto cmd = m_id_cmd_map.value(id, nullptr);

  if (!cmd) {
    cmd = new Command(id);
    m_id_cmd_map.insert(id, cmd);
    readUserSettings(id, cmd);
    ICore::mainWindow()->addAction(cmd->action());
    cmd->action()->setObjectName(id.toString());
    cmd->action()->setShortcutContext(Qt::ApplicationShortcut);
    cmd->d->setCurrentContext(m_context);

    if (ActionManager::isPresentationModeEnabled())
      connect(cmd->action(), &QAction::triggered, this, &ActionManagerPrivate::actionTriggered);
  }

  return cmd;
}

auto ActionManagerPrivate::readUserSettings(const Id id, Command *cmd) -> void
{
  QSettings *settings = ICore::settings();
  settings->beginGroup(k_keyboard_settings_key_v2);

  if (settings->contains(id.toString())) {
    if (const auto v = settings->value(id.toString()); static_cast<QMetaType::Type>(v.type()) == QMetaType::QStringList) {
      cmd->setKeySequences(Utils::transform<QList>(v.toStringList(), [](const QString &s) {
        return QKeySequence::fromString(s);
      }));
    } else {
      cmd->setKeySequences({QKeySequence::fromString(v.toString())});
    }
  }
  settings->endGroup();
}

auto ActionManagerPrivate::saveSettings(const Command *cmd) -> void
{
  const auto id = cmd->id().toString();
  const QString settings_key = QLatin1String(k_keyboard_settings_key_v2) + '/' + id;
  const auto keys = cmd->keySequences();

  if (const auto default_keys = cmd->defaultKeySequences(); keys != default_keys) {
    if (keys.isEmpty()) {
      ICore::settings()->setValue(settings_key, QString());
    } else if (keys.size() == 1) {
      ICore::settings()->setValue(settings_key, keys.first().toString());
    } else {
      ICore::settings()->setValue(settings_key, Utils::transform<QStringList>(keys, [](const QKeySequence &k) {
        return k.toString();
      }));
    }
  } else {
    ICore::settings()->remove(settings_key);
  }
}

auto ActionManagerPrivate::saveSettings() const -> void
{
  const auto cmdcend = m_id_cmd_map.constEnd();
  for (auto j = m_id_cmd_map.constBegin(); j != cmdcend; ++j) {
    saveSettings(j.value());
  }
}

} // namespace Core
