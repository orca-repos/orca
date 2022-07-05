// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include "wizardpage.h"

QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QLabel;
QT_END_NAMESPACE

namespace Utils {
class OutputFormatter;
class ShellCommand;

class ORCA_UTILS_EXPORT ShellCommandPage : public WizardPage {
  Q_OBJECT

public:
  enum State {
    Idle,
    Running,
    Failed,
    Succeeded
  };

  explicit ShellCommandPage(QWidget *parent = nullptr);
  ~ShellCommandPage() override;

  auto setStartedStatus(const QString &startedStatus) -> void;
  auto start(ShellCommand *command) -> void;
  auto isComplete() const -> bool override;
  auto isRunning() const -> bool { return m_state == Running; }
  auto terminate() -> void;
  auto handleReject() -> bool override;

signals:
  auto finished(bool success) -> void;

private:
  auto slotFinished(bool ok, int exitCode, const QVariant &cookie) -> void;

  QPlainTextEdit *m_logPlainTextEdit;
  OutputFormatter *m_formatter;
  QLabel *m_statusLabel;
  ShellCommand *m_command = nullptr;
  QString m_startedStatus;
  bool m_overwriteOutput = false;
  State m_state = Idle;
};

} // namespace Utils
