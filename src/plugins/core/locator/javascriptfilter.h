// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/locator/ilocatorfilter.h>

#include <QTimer>

#include <atomic>
#include <memory>

QT_BEGIN_NAMESPACE
class QJSEngine;
QT_END_NAMESPACE

namespace Core {
namespace Internal {

class JavaScriptFilter final : public ILocatorFilter {
  Q_OBJECT

public:
  JavaScriptFilter();
  ~JavaScriptFilter() override;

  auto prepareSearch(const QString &entry) -> void override;
  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;

private:
  auto setupEngine() const -> void;

  mutable std::unique_ptr<QJSEngine> m_engine;
  QTimer m_abort_timer;
  std::atomic_bool m_aborted = false;
};

} // namespace Internal
} // namespace Core
