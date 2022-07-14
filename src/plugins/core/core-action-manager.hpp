// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-command.hpp"

#include "core-constants.hpp"
#include "core-context-interface.hpp"
#include "core-global.hpp"

#include <QList>
#include <QObject>

QT_BEGIN_NAMESPACE
class QAction;
class QString;
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class ActionContainer;
class Command;
class Context;
class CorePlugin;
class MainWindow;

class CORE_EXPORT ActionManager final : public QObject {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ActionManager)

public:
  static auto instance() -> ActionManager*;
  static auto createMenu(Utils::Id id) -> ActionContainer*;
  static auto createMenuBar(Utils::Id id) -> ActionContainer*;
  static auto createTouchBar(Utils::Id id, const QIcon &icon, const QString &text = QString()) -> ActionContainer*;
  static auto registerAction(QAction *action, Utils::Id id, const Context &context = Context(C_GLOBAL), bool scriptable = false) -> Command*;
  static auto command(Utils::Id id) -> Command*;
  static auto actionContainer(Utils::Id id) -> ActionContainer*;
  static auto commands() -> QList<Command*>;
  static auto unregisterAction(QAction *action, Utils::Id id) -> void;
  static auto setPresentationModeEnabled(bool enabled) -> void;
  static auto isPresentationModeEnabled() -> bool;
  static auto withNumberAccelerator(const QString &text, int number) -> QString;

signals:
  auto commandListChanged() -> void;
  auto commandAdded(Utils::Id id) -> void;

private:
  explicit ActionManager(QObject *parent = nullptr);
  ~ActionManager() override;

  static auto saveSettings() -> void;
  static auto setContext(const Context &context) -> void;

  friend class CorePlugin; // initialization
  friend class MainWindow; // saving settings and setting context
};

} // namespace Orca::Plugin::Core
