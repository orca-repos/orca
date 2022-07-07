// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "shellcommandpage.hpp"
#include "shellcommand.hpp"
#include "outputformatter.hpp"
#include "qtcassert.hpp"
#include "theme/theme.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QLabel>
#include <QPlainTextEdit>
#include <QVBoxLayout>

/*!
    \class Utils::ShellCommandPage

    \brief The ShellCommandPage implements a page showing the
    progress of a \c ShellCommand.

    Turns complete when the command succeeds.
*/

namespace Utils {

ShellCommandPage::ShellCommandPage(QWidget *parent) : WizardPage(parent), m_startedStatus(tr("Command started..."))
{
  resize(264, 200);
  auto verticalLayout = new QVBoxLayout(this);
  m_logPlainTextEdit = new QPlainTextEdit;
  m_formatter = new OutputFormatter;
  m_logPlainTextEdit->setReadOnly(true);
  m_formatter->setPlainTextEdit(m_logPlainTextEdit);

  verticalLayout->addWidget(m_logPlainTextEdit);

  m_statusLabel = new QLabel;
  verticalLayout->addWidget(m_statusLabel);
  setTitle(tr("Run Command"));
}

ShellCommandPage::~ShellCommandPage()
{
  QTC_ASSERT(m_state != Running, QApplication::restoreOverrideCursor());
  delete m_formatter;
}

auto ShellCommandPage::setStartedStatus(const QString &startedStatus) -> void
{
  m_startedStatus = startedStatus;
}

auto ShellCommandPage::start(ShellCommand *command) -> void
{
  if (!command) {
    m_logPlainTextEdit->setPlainText(tr("No job running, please abort."));
    return;
  }

  QTC_ASSERT(m_state != Running, return);
  m_command = command;
  command->setProgressiveOutput(true);
  connect(command, &ShellCommand::stdOutText, this, [this](const QString &text) {
    m_formatter->appendMessage(text, StdOutFormat);
  });
  connect(command, &ShellCommand::stdErrText, this, [this](const QString &text) {
    m_formatter->appendMessage(text, StdErrFormat);
  });
  connect(command, &ShellCommand::finished, this, &ShellCommandPage::slotFinished);
  QApplication::setOverrideCursor(Qt::WaitCursor);
  m_logPlainTextEdit->clear();
  m_overwriteOutput = false;
  m_statusLabel->setText(m_startedStatus);
  m_statusLabel->setPalette(QPalette());
  m_state = Running;
  command->execute();

  wizard()->button(QWizard::BackButton)->setEnabled(false);
}

auto ShellCommandPage::slotFinished(bool ok, int exitCode, const QVariant &) -> void
{
  QTC_ASSERT(m_state == Running, return);

  const bool success = (ok && exitCode == 0);
  QString message;
  QPalette palette;

  if (success) {
    m_state = Succeeded;
    message = tr("Succeeded.");
    palette.setColor(QPalette::WindowText, orcaTheme()->color(Theme::TextColorNormal).name());
  } else {
    m_state = Failed;
    message = tr("Failed.");
    palette.setColor(QPalette::WindowText, orcaTheme()->color(Theme::TextColorError).name());
  }

  m_statusLabel->setText(message);
  m_statusLabel->setPalette(palette);

  QApplication::restoreOverrideCursor();
  wizard()->button(QWizard::BackButton)->setEnabled(true);

  if (success) emit completeChanged();
  emit finished(success);
}

auto ShellCommandPage::terminate() -> void
{
  if (m_command)
    m_command->cancel();
}

auto ShellCommandPage::handleReject() -> bool
{
  if (!isRunning())
    return false;

  terminate();
  return true;
}

auto ShellCommandPage::isComplete() const -> bool
{
  return m_state == Succeeded;
}

} // namespace Utils
