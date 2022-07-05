// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "fileutils.h"
#include "hostosinfo.h"

#include <QStringList>

namespace Utils {

class AbstractMacroExpander;
class CommandLine;
class Environment;
class MacroExpander;

class ORCA_UTILS_EXPORT ProcessArgs {
public:
  static auto createWindowsArgs(const QString &args) -> ProcessArgs;
  static auto createUnixArgs(const QStringList &args) -> ProcessArgs;
  auto toWindowsArgs() const -> QString;
  auto toUnixArgs() const -> QStringList;
  auto toString() const -> QString;

  enum SplitError {
    SplitOk = 0,
    //! All went just fine
    BadQuoting,
    //! Command contains quoting errors
    FoundMeta //! Command contains complex shell constructs
  };

  //! Quote a single argument for usage in a unix shell command
  static auto quoteArgUnix(const QString &arg) -> QString;
  //! Quote a single argument for usage in a shell command
  static auto quoteArg(const QString &arg, OsType osType = HostOsInfo::hostOs()) -> QString;
  //! Quote a single argument and append it to a shell command
  static auto addArg(QString *args, const QString &arg, OsType osType = HostOsInfo::hostOs()) -> void;
  //! Join an argument list into a shell command
  static auto joinArgs(const QStringList &args, OsType osType = HostOsInfo::hostOs()) -> QString;
  //! Prepare argument of a shell command for feeding into QProcess
  static auto prepareArgs(const QString &args, SplitError *err, OsType osType, const Environment *env = nullptr, const FilePath *pwd = nullptr, bool abortOnMeta = true) -> ProcessArgs;
  //! Prepare a shell command for feeding into QProcess
  static auto prepareCommand(const CommandLine &cmdLine, QString *outCmd, ProcessArgs *outArgs, const Environment *env = nullptr, const FilePath *pwd = nullptr) -> bool;
  //! Quote and append each argument to a shell command
  static auto addArgs(QString *args, const QStringList &inArgs) -> void;
  //! Append already quoted arguments to a shell command
  static auto addArgs(QString *args, const QString &inArgs) -> void;
  //! Split a shell command into separate arguments.
  static auto splitArgs(const QString &cmd, OsType osType = HostOsInfo::hostOs(), bool abortOnMeta = false, SplitError *err = nullptr, const Environment *env = nullptr, const QString *pwd = nullptr) -> QStringList;
  //! Safely replace the expandos in a shell command
  static auto expandMacros(QString *cmd, AbstractMacroExpander *mx, OsType osType = HostOsInfo::hostOs()) -> bool;

  /*! Iterate over arguments from a command line.
   *  Assumes that the name of the actual command is *not* part of the line.
   *  Terminates after the first command if the command line is complex.
   */
  class ORCA_UTILS_EXPORT ArgIterator {
  public:
    ArgIterator(QString *str, OsType osType = HostOsInfo::hostOs()) : m_str(str), m_osType(osType) {}
    //! Get the next argument. Returns false on encountering end of first command.
    auto next() -> bool;
    //! True iff the argument is a plain string, possibly after unquoting.
    auto isSimple() const -> bool { return m_simple; }
    //! Return the string value of the current argument if it is simple, otherwise empty.
    auto value() const -> QString { return m_value; }
    //! Delete the last argument fetched via next() from the command line.
    auto deleteArg() -> void;
    //! Insert argument into the command line after the last one fetched via next().
    //! This may be used before the first call to next() to insert at the front.
    auto appendArg(const QString &str) -> void;
  private:
    QString *m_str, m_value;
    int m_pos = 0;
    int m_prev = -1;
    bool m_simple;
    OsType m_osType;
  };

  class ORCA_UTILS_EXPORT ConstArgIterator {
  public:
    ConstArgIterator(const QString &str, OsType osType = HostOsInfo::hostOs()) : m_str(str), m_ait(&m_str, osType) {}
    auto next() -> bool { return m_ait.next(); }
    auto isSimple() const -> bool { return m_ait.isSimple(); }
    auto value() const -> QString { return m_ait.value(); }
  private:
    QString m_str;
    ArgIterator m_ait;
  };

private:
  QString m_windowsArgs;
  QStringList m_unixArgs;
  bool m_isWindows;
};

class ORCA_UTILS_EXPORT CommandLine {
public:
  enum RawType { Raw };

  CommandLine();
  explicit CommandLine(const FilePath &executable);
  CommandLine(const FilePath &exe, const QStringList &args);
  CommandLine(const FilePath &exe, const QString &unparsedArgs, RawType);

  static auto fromUserInput(const QString &cmdline, MacroExpander *expander = nullptr) -> CommandLine;
  auto addArg(const QString &arg) -> void;
  auto addArgs(const QStringList &inArgs) -> void;
  auto addCommandLineAsArgs(const CommandLine &cmd) -> void;
  auto addArgs(const QString &inArgs, RawType) -> void;
  auto toUserOutput() const -> QString;
  auto executable() const -> FilePath { return m_executable; }
  auto setExecutable(const FilePath &executable) -> void { m_executable = executable; }
  auto arguments() const -> QString { return m_arguments; }
  auto setArguments(const QString &args) -> void { m_arguments = args; }
  auto splitArguments() const -> QStringList;
  auto isEmpty() const -> bool { return m_executable.isEmpty(); }

  friend auto operator==(const CommandLine &first, const CommandLine &second) -> bool
  {
    return first.m_executable == second.m_executable && first.m_arguments == second.m_arguments;
  }

private:
  FilePath m_executable;
  QString m_arguments;
};

} // namespace Utils

QT_BEGIN_NAMESPACE ORCA_UTILS_EXPORT auto operator<<(QDebug dbg, const Utils::CommandLine &cmd) -> QDebug;
QT_END_NAMESPACE

Q_DECLARE_METATYPE(Utils::CommandLine)
