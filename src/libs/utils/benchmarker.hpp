// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

#include <QString>
#include <QElapsedTimer>

QT_BEGIN_NAMESPACE
class QLoggingCategory;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT Benchmarker {
public:
  Benchmarker(const QString &testsuite, const QString &testcase, const QString &tagData = QString());
  Benchmarker(const QLoggingCategory &cat, const QString &testsuite, const QString &testcase, const QString &tagData = QString());
  ~Benchmarker();

  auto report(qint64 ms) -> void;
  static auto report(const QString &testsuite, const QString &testcase, qint64 ms, const QString &tags = QString()) -> void;
  static auto report(const QLoggingCategory &cat, const QString &testsuite, const QString &testcase, qint64 ms, const QString &tags = QString()) -> void;

private:
  const QLoggingCategory &m_category;
  QElapsedTimer m_timer;
  QString m_tagData;
  QString m_testsuite;
  QString m_testcase;
};

} // namespace Utils
