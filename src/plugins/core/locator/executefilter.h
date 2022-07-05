// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ilocatorfilter.h"

#include <utils/qtcprocess.h>

#include <QQueue>
#include <QTextCodec>

namespace Core {
namespace Internal {

class ExecuteFilter final : public Core::ILocatorFilter {
  Q_OBJECT

  struct ExecuteData {
    Utils::CommandLine command;
    Utils::FilePath working_directory;
  };

public:
  ExecuteFilter();
  ~ExecuteFilter() override;

  auto matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry> override;
  auto accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void override;

private:
  auto finished() -> void;
  auto readStandardOutput() -> void;
  auto readStandardError() -> void;
  auto runHeadCommand() -> void;
  auto createProcess() -> void;
  auto removeProcess() -> void;
  auto saveState(QJsonObject &object) const -> void final;
  auto restoreState(const QJsonObject &object) -> void final;
  auto headCommand() const -> QString;

  QQueue<ExecuteData> m_task_queue;
  QStringList m_command_history;
  Utils::QtcProcess *m_process = nullptr;
  QTextCodec::ConverterState m_stdout_state;
  QTextCodec::ConverterState m_stderr_state;
};

} // namespace Internal
} // namespace Core
