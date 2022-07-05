// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "terminalcommand.h"

#include <utils/algorithm.h>
#include <utils/commandline.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/qtcassert.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>

namespace Utils {

static QSettings *s_settings = nullptr;

TerminalCommand::TerminalCommand(const QString &command, const QString &openArgs, const QString &executeArgs, bool needsQuotes) : command(command), openArgs(openArgs), executeArgs(executeArgs), needsQuotes(needsQuotes) {}

auto TerminalCommand::operator==(const TerminalCommand &other) const -> bool
{
  return other.command == command && other.openArgs == openArgs && other.executeArgs == executeArgs;
}

auto TerminalCommand::operator<(const TerminalCommand &other) const -> bool
{
  if (command == other.command) {
    if (openArgs == other.openArgs)
      return executeArgs < other.executeArgs;
    return openArgs < other.openArgs;
  }
  return command < other.command;
}

auto TerminalCommand::setSettings(QSettings *settings) -> void
{
  s_settings = settings;
}

Q_GLOBAL_STATIC_WITH_ARGS(const QVector<TerminalCommand>, knownTerminals, ({ {"x-terminal-emulator", "", "-e"}, {"xdg-terminal", "", "", true}, {"xterm", "", "-e"}, {"aterm", "", "-e"}, {"Eterm", "", "-e"}, {"rxvt", "", "-e"}, {"urxvt", "", "-e"}, {"xfce4-terminal", "", "-x"}, {"konsole", "--separate --workdir .", "-e"}, {"gnome-terminal", "", "--"}}));

auto TerminalCommand::defaultTerminalEmulator() -> TerminalCommand
{
  static TerminalCommand defaultTerm;

  if (defaultTerm.command.isEmpty()) {
    if (HostOsInfo::isMacHost()) {
      const QString termCmd = QCoreApplication::applicationDirPath() + "/../Resources/scripts/openTerminal.py";
      if (QFileInfo::exists(termCmd))
        defaultTerm = {termCmd, "", ""};
      else
        defaultTerm = {"/usr/X11/bin/xterm", "", "-e"};

    } else if (HostOsInfo::isAnyUnixHost()) {
      defaultTerm = {"xterm", "", "-e"};
      const Environment env = Environment::systemEnvironment();
      for (const TerminalCommand &term : *knownTerminals) {
        const QString result = env.searchInPath(term.command).toString();
        if (!result.isEmpty()) {
          defaultTerm = {result, term.openArgs, term.executeArgs, term.needsQuotes};
          break;
        }
      }
    }
  }

  return defaultTerm;
}

auto TerminalCommand::availableTerminalEmulators() -> QVector<TerminalCommand>
{
  QVector<TerminalCommand> result;

  if (HostOsInfo::isAnyUnixHost()) {
    const Environment env = Environment::systemEnvironment();
    for (const TerminalCommand &term : *knownTerminals) {
      const QString command = env.searchInPath(term.command).toString();
      if (!command.isEmpty())
        result.push_back({command, term.openArgs, term.executeArgs});
    }
    // sort and put default terminal on top
    const TerminalCommand defaultTerm = defaultTerminalEmulator();
    result.removeAll(defaultTerm);
    sort(result);
    result.prepend(defaultTerm);
  }

  return result;
}

constexpr char kTerminalVersion[] = "4.8";
constexpr char kTerminalVersionKey[] = "General/Terminal/SettingsVersion";
constexpr char kTerminalCommandKey[] = "General/Terminal/Command";
constexpr char kTerminalOpenOptionsKey[] = "General/Terminal/OpenOptions";
constexpr char kTerminalExecuteOptionsKey[] = "General/Terminal/ExecuteOptions";

auto TerminalCommand::terminalEmulator() -> TerminalCommand
{
  if (s_settings && HostOsInfo::isAnyUnixHost()) {
    if (s_settings->value(kTerminalVersionKey).toString() == kTerminalVersion) {
      if (s_settings->contains(kTerminalCommandKey))
        return {s_settings->value(kTerminalCommandKey).toString(), s_settings->value(kTerminalOpenOptionsKey).toString(), s_settings->value(kTerminalExecuteOptionsKey).toString()};
    } else {
      // TODO remove reading of old settings some time after 4.8
      const QString value = s_settings->value("General/TerminalEmulator").toString().trimmed();
      if (!value.isEmpty()) {
        // split off command and options
        const QStringList splitCommand = ProcessArgs::splitArgs(value);
        if (QTC_GUARD(!splitCommand.isEmpty())) {
          const QString command = splitCommand.first();
          const QStringList quotedArgs = Utils::transform(splitCommand.mid(1), &ProcessArgs::quoteArgUnix);
          const QString options = quotedArgs.join(' ');
          return {command, "", options};
        }
      }
    }
  }

  return defaultTerminalEmulator();
}

auto TerminalCommand::setTerminalEmulator(const TerminalCommand &term) -> void
{
  if (s_settings && HostOsInfo::isAnyUnixHost()) {
    s_settings->setValue(kTerminalVersionKey, kTerminalVersion);
    if (term == defaultTerminalEmulator()) {
      s_settings->remove(kTerminalCommandKey);
      s_settings->remove(kTerminalOpenOptionsKey);
      s_settings->remove(kTerminalExecuteOptionsKey);
    } else {
      s_settings->setValue(kTerminalCommandKey, term.command);
      s_settings->setValue(kTerminalOpenOptionsKey, term.openArgs);
      s_settings->setValue(kTerminalExecuteOptionsKey, term.executeArgs);
    }
  }
}

} // Utils
