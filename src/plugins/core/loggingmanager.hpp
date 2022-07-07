// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/optional.hpp>

#include <QColor>
#include <QLoggingCategory>
#include <QMap>
#include <QObject>

namespace Core {
namespace Internal {

struct FilterRuleSpec {
  QString category;
  Utils::optional<QtMsgType> level;
  bool enabled{};
};

class LoggingCategoryEntry {
public:
  QtMsgType level = QtDebugMsg;
  bool enabled = false;
  QColor color;
};

class LoggingViewManager final : public QObject {
  Q_OBJECT

public:
  static auto messageTypeToString(const QtMsgType type) -> QString
  {
    switch (type) {
    case QtDebugMsg:
      return {"Debug"};
    case QtInfoMsg:
      return {"Info"};
    case QtCriticalMsg:
      return {"Critical"};
    case QtWarningMsg:
      return {"Warning"};
    case QtFatalMsg:
      return {"Fatal"};
    default:
      return {"Unknown"};
    }
  }

  static auto messageTypeFromString(const QString &type) -> QtMsgType
  {
    if (type.isEmpty())
      return QtDebugMsg;

    // shortcut - only handle expected
    switch (type.at(0).toLatin1()) {
    case 'I':
      return QtInfoMsg;
    case 'C':
      return QtCriticalMsg;
    case 'W':
      return QtWarningMsg;
    case 'D': default:
      return QtDebugMsg;
    }
  }

  explicit LoggingViewManager(QObject *parent = nullptr);
  ~LoggingViewManager() override;

  static auto instance() -> LoggingViewManager*;

  static auto enabled(const QtMsgType current, const QtMsgType stored) -> bool
  {
    if (stored == QtInfoMsg)
      return true;
    if (current == stored)
      return true;
    if (stored == QtDebugMsg)
      return current != QtInfoMsg;
    if (stored == QtWarningMsg)
      return current == QtCriticalMsg || current == QtFatalMsg;
    if (stored == QtCriticalMsg)
      return current == QtFatalMsg;
    return false;
  }

  static auto logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &mssg) -> void;
  auto setEnabled(const bool enabled) -> void { m_enabled = enabled; }
  auto isEnabled() const -> bool { return m_enabled; }
  auto isCategoryEnabled(const QString &category) -> bool;
  auto setCategoryEnabled(const QString &category, bool enabled) -> void;
  auto setLogLevel(const QString &category, QtMsgType type) -> void;
  auto setListQtInternal(bool list_qt_internal) -> void;
  auto originalRules() const -> QList<FilterRuleSpec> { return m_original_rules; }
  auto categories() const -> QMap<QString, LoggingCategoryEntry> { return m_categories; }
  auto appendOrUpdate(const QString &category, const LoggingCategoryEntry &entry) -> void;

signals:
  auto receivedLog(const QString &timestamp, const QString &type, const QString &category, const QString &msg) -> void;
  auto foundNewCategory(const QString &category, const LoggingCategoryEntry &entry) -> void;
  auto updatedCategory(const QString &category, const LoggingCategoryEntry &entry) -> void;

private:
  auto prefillCategories() -> void;
  auto resetFilterRules() -> void;
  auto enabledInOriginalRules(const QMessageLogContext &context, QtMsgType type) -> bool;

  QMap<QString, LoggingCategoryEntry> m_categories;
  const QString m_original_logging_rules;
  QList<FilterRuleSpec> m_original_rules;
  bool m_enabled = false;
  bool m_list_qt_internal = false;
};

} // namespace Internal
} // namespace Core

Q_DECLARE_METATYPE(Core::Internal::LoggingCategoryEntry)
