// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "messagemanager.hpp"
#include "messageoutputwindow.hpp"

#include <extensionsystem/pluginmanager.hpp>
#include <utils/qtcassert.hpp>

#include <QFont>
#include <QThread>
#include <QTime>

/*!
    \class Core::MessageManager
    \inheaderfile coreplugin/messagemanager.h
    \ingroup mainclasses
    \inmodule Orca

    \brief The MessageManager class is used to post messages in the
    \uicontrol{General Messages} pane.
*/

namespace Core {

static MessageManager *m_instance = nullptr;
static Internal::MessageOutputWindow *m_message_output_window = nullptr;

/*!
    \internal
*/
auto MessageManager::instance() -> MessageManager*
{
  return m_instance;
}

enum class Flag {
  Silent,
  Flash,
  Disrupt
};

static auto showOutputPane(const Flag flags) -> void
{
  QTC_ASSERT(m_message_output_window, return);

  switch (flags) {
  case Flag::Silent:
    break;
  case Flag::Flash:
    m_message_output_window->flash();
    break;
  case Flag::Disrupt:
    m_message_output_window->popup(IOutputPane::ModeSwitch | IOutputPane::WithFocus);
    break;
  }
}

static auto doWrite(const QString &text, const Flag flags) -> void
{
  QTC_ASSERT(m_message_output_window, return);
  showOutputPane(flags);
  m_message_output_window->append(text + '\n');
}

static auto write(const QString &text, Flag flags) -> void
{
  QTC_ASSERT(m_instance, return);
  if (QThread::currentThread() == m_instance->thread())
    doWrite(text, flags);
  else
    QMetaObject::invokeMethod(m_instance, [text, flags] {
      doWrite(text, flags);
    }, Qt::QueuedConnection);
}

/*!
    \internal
*/
MessageManager::MessageManager()
{
  m_instance = this;
  m_message_output_window = nullptr;
}

/*!
    \internal
*/
MessageManager::~MessageManager()
{
  if (m_message_output_window) {
    ExtensionSystem::PluginManager::removeObject(m_message_output_window);
    delete m_message_output_window;
  }
  m_instance = nullptr;
}

/*!
    \internal
*/
auto MessageManager::init() -> void
{
  m_message_output_window = new Internal::MessageOutputWindow;
  ExtensionSystem::PluginManager::addObject(m_message_output_window);
}

/*!
    \internal
*/
auto MessageManager::setFont(const QFont &font) -> void
{
  QTC_ASSERT(m_message_output_window, return);
  m_message_output_window->setFont(font);
}

/*!
    \internal
*/
auto MessageManager::setWheelZoomEnabled(bool enabled) -> void
{
  QTC_ASSERT(m_message_output_window, return);
  m_message_output_window->setWheelZoomEnabled(enabled);
}

/*!
    Writes the \a message to the \uicontrol{General Messages} pane without
    any further action.

    This is the preferred method of posting messages, since it does not
    interrupt the user.

    \sa writeFlashing()
    \sa writeDisrupting()
*/
auto MessageManager::writeSilently(const QString &message) -> void
{
  write(message, Flag::Silent);
}

/*!
    Writes the \a message to the \uicontrol{General Messages} pane and flashes
    the output pane button.

    This notifies the user that something important has happened that might
    require the user's attention. Use sparingly, since continually flashing the
    button is annoying, especially if the condition is something the user might
    not be able to fix.

    \sa writeSilently()
    \sa writeDisrupting()
*/
auto MessageManager::writeFlashing(const QString &message) -> void
{
  write(message, Flag::Flash);
}

/*!
    Writes the \a message to the \uicontrol{General Messages} pane and brings
    the pane to the front.

    This might interrupt a user's workflow, so only use this as a direct
    response to something a user did, like explicitly running a tool.

    \sa writeSilently()
    \sa writeFlashing()
*/
auto MessageManager::writeDisrupting(const QString &message) -> void
{
  write(message, Flag::Disrupt);
}

/*!
    \overload writeSilently()
*/
auto MessageManager::writeSilently(const QStringList &messages) -> void
{
  writeSilently(messages.join('\n'));
}

/*!
    \overload writeFlashing()
*/
auto MessageManager::writeFlashing(const QStringList &messages) -> void
{
  writeFlashing(messages.join('\n'));
}

/*!
    \overload writeDisrupting()
*/
auto MessageManager::writeDisrupting(const QStringList &messages) -> void
{
  writeDisrupting(messages.join('\n'));
}

} // namespace Core
