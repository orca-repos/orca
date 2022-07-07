// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QMetaType>
#include <QVector>

QT_BEGIN_NAMESPACE
class QSettings;
QT_END_NAMESPACE

namespace Utils {

class Environment;

class ORCA_UTILS_EXPORT TerminalCommand {
public:
  TerminalCommand() = default;
  TerminalCommand(const QString &command, const QString &openArgs, const QString &executeArgs, bool needsQuotes = false);

  auto operator==(const TerminalCommand &other) const -> bool;
  auto operator<(const TerminalCommand &other) const -> bool;

  QString command;
  QString openArgs;
  QString executeArgs;
  bool needsQuotes = false;

  static auto setSettings(QSettings *settings) -> void;
  static auto defaultTerminalEmulator() -> TerminalCommand;
  static auto availableTerminalEmulators() -> QVector<TerminalCommand>;
  static auto terminalEmulator() -> TerminalCommand;
  static auto setTerminalEmulator(const TerminalCommand &term) -> void;
};

} // Utils

Q_DECLARE_METATYPE(Utils::TerminalCommand)
