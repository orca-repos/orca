// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "projectexplorer_export.hpp"
#include "task.hpp"

#include <utils/id.hpp>

#include <QObject>
#include <QString>

QT_BEGIN_NAMESPACE
class QAction;
QT_END_NAMESPACE

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT ITaskHandler : public QObject {
  Q_OBJECT

public:
  explicit ITaskHandler(bool isMultiHandler = false);
  ~ITaskHandler() override;

  virtual auto isDefaultHandler() const -> bool { return false; }
  virtual auto canHandle(const Task &) const -> bool { return m_isMultiHandler; }
  virtual auto handle(const Task &) -> void;       // Non-multi-handlers should implement this.
  virtual auto handle(const Tasks &tasks) -> void; // Multi-handlers should implement this.
  virtual auto actionManagerId() const -> Utils::Id { return Utils::Id(); }
  virtual auto createAction(QObject *parent) const -> QAction* = 0;
  auto canHandle(const Tasks &tasks) const -> bool;

private:
  const bool m_isMultiHandler;
};

} // namespace ProjectExplorer
