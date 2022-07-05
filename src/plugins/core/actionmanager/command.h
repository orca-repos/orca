// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <utils/hostosinfo.h>
#include <utils/id.h>

#include <QObject>

QT_BEGIN_NAMESPACE
class QAction;
class QKeySequence;
class QToolButton;
QT_END_NAMESPACE

namespace Core {
namespace Internal {
class ActionManagerPrivate;
class CommandPrivate;
} // namespace Internal

class ActionManager;
class Context;

constexpr bool use_mac_shortcuts = Utils::HostOsInfo::isMacHost();

class CORE_EXPORT Command final : public QObject {
  Q_OBJECT

public:
  enum command_attribute {
    ca_hide = 1,
    ca_update_text = 2,
    ca_update_icon = 4,
    ca_non_configurable = 8
  };

  Q_DECLARE_FLAGS(CommandAttributes, command_attribute)

  ~Command() override;

  auto setDefaultKeySequence(const QKeySequence &key) -> void;
  auto setDefaultKeySequences(const QList<QKeySequence> &keys) -> void;
  auto defaultKeySequences() const -> QList<QKeySequence>;
  auto keySequences() const -> QList<QKeySequence>;
  auto keySequence() const -> QKeySequence;
  // explicitly set the description (used e.g. in shortcut settings)
  // default is to use the action text for actions, or the whatsThis for shortcuts,
  // or, as a last fall back if these are empty, the command ID string
  // override the default e.g. if the text is context dependent and contains file names etc
  auto setDescription(const QString &text) const -> void;
  auto description() const -> QString;
  auto id() const -> Utils::Id;
  auto action() const -> QAction*;
  auto context() const -> Context;
  auto setAttribute(command_attribute attr) const -> void;
  auto removeAttribute(command_attribute attr) const -> void;
  auto hasAttribute(command_attribute attr) const -> bool;
  auto isActive() const -> bool;
  auto setKeySequences(const QList<QKeySequence> &keys) -> void;
  auto stringWithAppendedShortcut(const QString &str) const -> QString;
  auto augmentActionWithShortcutToolTip(QAction *action) const -> void;
  static auto toolButtonWithAppendedShortcut(QAction *action, Command *cmd) -> QToolButton*;
  auto isScriptable() const -> bool;
  auto isScriptable(const Context &) const -> bool;
  auto setTouchBarText(const QString &text) const -> void;
  auto touchBarText() const -> QString;
  auto setTouchBarIcon(const QIcon &icon) const -> void;
  auto touchBarIcon() const -> QIcon;
  auto touchBarAction() const -> QAction*;

signals:
  auto keySequenceChanged() -> void;
  auto activeStateChanged() -> void;

private:
  friend class ActionManager;
  friend class Internal::ActionManagerPrivate;

  Command(Utils::Id id);

  Internal::CommandPrivate *d;
};

} // namespace Core

Q_DECLARE_OPERATORS_FOR_FLAGS(Core::Command::CommandAttributes)
