// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QStringList>
#include <QMap>

namespace ExtensionSystem {
namespace Internal {

class PluginManagerPrivate;

class OptionsParser {
public:
  OptionsParser(const QStringList &args, const QMap<QString, bool> &appOptions, QMap<QString, QString> *foundAppOptions, QString *errorString, PluginManagerPrivate *pmPrivate);

  auto parse() -> bool;

  static const char *NO_LOAD_OPTION;
  static const char *LOAD_OPTION;
  static const char *TEST_OPTION;
  static const char *NOTEST_OPTION;
  static const char *SCENARIO_OPTION;
  static const char *PROFILE_OPTION;
  static const char *NO_CRASHCHECK_OPTION;

private:
  // return value indicates if the option was processed
  // it doesn't indicate success (--> m_hasError)
  auto checkForEndOfOptions() -> bool;
  auto checkForLoadOption() -> bool;
  auto checkForNoLoadOption() -> bool;
  auto checkForTestOptions() -> bool;
  auto checkForScenarioOption() -> bool;
  auto checkForAppOption() -> bool;
  auto checkForPluginOption() -> bool;
  auto checkForProfilingOption() -> bool;
  auto checkForNoCrashcheckOption() -> bool;
  auto checkForUnknownOption() -> bool;
  auto forceDisableAllPluginsExceptTestedAndForceEnabled() -> void;

  enum TokenType {
    OptionalToken,
    RequiredToken
  };

  auto nextToken(TokenType type = OptionalToken) -> bool;

  const QStringList &m_args;
  const QMap<QString, bool> &m_appOptions;
  QMap<QString, QString> *m_foundAppOptions;
  QString *m_errorString;
  PluginManagerPrivate *m_pmPrivate;

  // state
  QString m_currentArg;
  QStringList::const_iterator m_it;
  QStringList::const_iterator m_end;
  bool m_isDependencyRefreshNeeded;
  bool m_hasError;
};

} // namespace Internal
} // namespace ExtensionSystem