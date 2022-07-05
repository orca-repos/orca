// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "loggingmanager.h"

#include <utils/filepath.h>

#include <QDateTime>
#include <QLibraryInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

//
//    WARNING! Do not use qDebug(), qWarning() or similar inside this file -
//             same applies for indirect usages (e.g. QTC_ASSERT() and the like).
//             Using static functions of QLoggingCategory may cause dead locks as well.
//

namespace Core {
namespace Internal {

static QtMessageHandler s_original_message_handler = nullptr;

static LoggingViewManager *s_instance = nullptr;

static auto levelToString(const QtMsgType t) -> QString
{
  switch (t) {
  case QtCriticalMsg:
    return {"critical"};
  case QtDebugMsg:
    return {"debug"};
  case QtInfoMsg:
    return {"info"};
  case QtWarningMsg:
    return {"warning"};
  default:
    return {"fatal"}; // wrong but we don't care
  }
}

static auto parseLevel(const QString &level) -> QtMsgType
{
  switch (level.at(0).toLatin1()) {
  case 'c':
    return QtCriticalMsg;
  case 'd':
    return QtDebugMsg;
  case 'i':
    return QtInfoMsg;
  case 'w':
    return QtWarningMsg;
  default:
    return QtFatalMsg; // wrong but we don't care
  }
}

static auto parseLine(const QString &line, FilterRuleSpec *filter_rule) -> bool
{
  const auto parts = line.split('=');

  if (parts.size() != 2)
    return false;

  const auto category = parts.at(0);
  static const QRegularExpression regex("^(.+?)(\\.(debug|info|warning|critical))?$");
  const auto match = regex.match(category);

  if (!match.hasMatch())
    return false;

  const auto category_name = match.captured(1);

  if (category_name.size() > 2) {
    if (category_name.mid(1, category_name.size() - 2).contains('*'))
      return false;
  } else if (category_name.size() == 2) {
    if (category_name.count('*') == 2)
      return false;
  }

  filter_rule->category = category_name;

  if (match.capturedLength(2) == 0)
    filter_rule->level = Utils::nullopt;
  else
    filter_rule->level = Utils::make_optional(parseLevel(match.captured(2).mid(1)));

  if (const auto enabled = parts.at(1); enabled == "true" || enabled == "false") {
    filter_rule->enabled = enabled == "true";
    return true;
  }

  return false;
}

static auto fetchOriginalRules() -> QList<FilterRuleSpec>
{
  QList<FilterRuleSpec> rules;

  auto append_rules_from_file = [&rules](const QString &file_name) {
    QSettings ini_settings(file_name, QSettings::IniFormat);
    ini_settings.beginGroup("Rules");
    for (const auto keys = ini_settings.allKeys(); const auto &key : keys) {
      const auto value = ini_settings.value(key).toString();
      FilterRuleSpec filter_rule;
      if (parseLine(key + "=" + value, &filter_rule))
        rules.append(filter_rule);
    }
    ini_settings.endGroup();
  };

  auto ini_file = Utils::FilePath::fromString(QLibraryInfo::location(QLibraryInfo::DataPath)).pathAppended("qtlogging.ini");

  if (ini_file.exists())
    append_rules_from_file(ini_file.toString());

  if (const auto qt_project_string = QStandardPaths::locate(QStandardPaths::GenericConfigLocation, "QtProject/qtlogging.ini"); !qt_project_string.isEmpty())
    append_rules_from_file(qt_project_string);

  ini_file = Utils::FilePath::fromString(qEnvironmentVariable("QT_LOGGING_CONF"));

  if (ini_file.exists())
    append_rules_from_file(ini_file.toString());

  if (qEnvironmentVariableIsSet("QT_LOGGING_RULES")) {
    for (const auto rules_strings = qEnvironmentVariable("QT_LOGGING_RULES").split(';'); const auto &rule : rules_strings) {
      FilterRuleSpec filter_rule;
      if (parseLine(rule, &filter_rule))
        rules.append(filter_rule);
    }
  }

  return rules;
}

LoggingViewManager::LoggingViewManager(QObject *parent) : QObject(parent), m_original_logging_rules(qEnvironmentVariable("QT_LOGGING_RULES"))
{
  qRegisterMetaType<LoggingCategoryEntry>();
  s_instance = this;
  s_original_message_handler = qInstallMessageHandler(logMessageHandler);
  m_enabled = true;
  m_original_rules = fetchOriginalRules();
  prefillCategories();
  QLoggingCategory::setFilterRules("*=true");
}

LoggingViewManager::~LoggingViewManager()
{
  m_enabled = false;
  qInstallMessageHandler(s_original_message_handler);
  s_original_message_handler = nullptr;
  qputenv("QT_LOGGING_RULES", m_original_logging_rules.toLocal8Bit());
  QLoggingCategory::setFilterRules("*=false");
  resetFilterRules();
  s_instance = nullptr;
}

auto LoggingViewManager::instance() -> LoggingViewManager*
{
  return s_instance;
}

auto LoggingViewManager::logMessageHandler(const QtMsgType type, const QMessageLogContext &context, const QString &mssg) -> void
{
  if (!s_instance->m_enabled) {
    if (s_instance->enabledInOriginalRules(context, type))
      s_original_message_handler(type, context, mssg);
    return;
  }

  if (!context.category) {
    s_original_message_handler(type, context, mssg);
    return;
  }

  const auto category = QString::fromLocal8Bit(context.category);
  auto it = s_instance->m_categories.find(category);

  if (it == s_instance->m_categories.end()) {
    if (!s_instance->m_list_qt_internal && category.startsWith("qt."))
      return;
    LoggingCategoryEntry entry;
    entry.level = QtDebugMsg;
    entry.enabled = category == "default" || s_instance->enabledInOriginalRules(context, type);
    it = s_instance->m_categories.insert(category, entry);
    emit s_instance->foundNewCategory(category, entry);
  }

  if (const auto entry = it.value(); entry.enabled && enabled(type, entry.level)) {
    const auto timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    emit s_instance->receivedLog(timestamp, category, messageTypeToString(type), mssg);
  }
}

auto LoggingViewManager::isCategoryEnabled(const QString &category) -> bool
{
  const auto entry = m_categories.find(category);

  if (entry == m_categories.end()) // shall not happen - paranoia
    return false;

  return entry.value().enabled;
}

auto LoggingViewManager::setCategoryEnabled(const QString &category, const bool enabled) -> void
{
  const auto entry = m_categories.find(category);

  if (entry == m_categories.end()) // shall not happen - paranoia
    return;

  entry->enabled = enabled;
}

auto LoggingViewManager::setLogLevel(const QString &category, const QtMsgType type) -> void
{
  const auto entry = m_categories.find(category);

  if (entry == m_categories.end()) // shall not happen - paranoia
    return;

  entry->level = type;
}

auto LoggingViewManager::setListQtInternal(const bool list_qt_internal) -> void
{
  m_list_qt_internal = list_qt_internal;
}

auto LoggingViewManager::appendOrUpdate(const QString &category, const LoggingCategoryEntry &entry) -> void
{
  auto it = m_categories.find(category);
  auto append = it == m_categories.end();
  m_categories.insert(category, entry);
  if (append) emit foundNewCategory(category, entry);
  else emit updatedCategory(category, entry);
}

/*
 * Does not check categories for being present, will perform early exit if m_categories is not empty
 */
auto LoggingViewManager::prefillCategories() -> void
{
  if (!m_categories.isEmpty())
    return;

  for (auto i = 0, end = static_cast<int>(m_original_rules.size()); i < end; ++i) {
    const auto &rule = m_original_rules.at(i);
    if (rule.category.startsWith('*') || rule.category.endsWith('*'))
      continue;
    auto enabled = rule.enabled;
    // check following rules whether they might overwrite
    for (auto j = i + 1; j < end; ++j) {
      const auto &second_rule = m_original_rules.at(j);
      if (const QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(second_rule.category)); !regex.match(rule.category).hasMatch())
        continue;
      if (second_rule.level.has_value() && rule.level != second_rule.level)
        continue;
      enabled = second_rule.enabled;
    }
    LoggingCategoryEntry entry;
    entry.level = rule.level.value_or(QtInfoMsg);
    entry.enabled = enabled;
    m_categories.insert(rule.category, entry);
  }
}

auto LoggingViewManager::resetFilterRules() -> void
{
  for (const auto &rule : qAsConst(m_original_rules)) {
    const auto level = rule.level.has_value() ? '.' + levelToString(rule.level.value()) : QString();
    const QString rule_string = rule.category + level + '=' + (rule.enabled ? "true" : "false");
    QLoggingCategory::setFilterRules(rule_string);
  }
}

auto LoggingViewManager::enabledInOriginalRules(const QMessageLogContext &context, const QtMsgType type) -> bool
{
  if (!context.category)
    return false;

  const auto category = QString::fromUtf8(context.category);
  auto result = false;

  for (const auto &rule : qAsConst(m_original_rules)) {
    if (const QRegularExpression regex(QRegularExpression::wildcardToRegularExpression(rule.category)); regex.match(category).hasMatch()) {
      if (rule.level.has_value()) {
        if (rule.level.value() == type)
          result = rule.enabled;
      } else {
        result = rule.enabled;
      }
    }
  }

  return result;
}

} // namespace Internal
} // namespace Core
