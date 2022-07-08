// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "waitforstopdialog.hpp"

#include <utils/algorithm.hpp>

#include <QVBoxLayout>
#include <QTimer>
#include <QLabel>
#include <QPushButton>

using namespace ProjectExplorer;
using namespace Internal;

WaitForStopDialog::WaitForStopDialog(QList<RunControl*> runControls) : m_runControls(runControls)
{
  setWindowTitle(tr("Waiting for Applications to Stop"));

  const auto layout = new QVBoxLayout();
  setLayout(layout);

  m_progressLabel = new QLabel;
  layout->addWidget(m_progressLabel);

  const auto cancelButton = new QPushButton(tr("Cancel"));
  connect(cancelButton, &QPushButton::clicked, this, &QDialog::close);
  layout->addWidget(cancelButton);

  updateProgressText();

  foreach(RunControl *rc, runControls)
    connect(rc, &RunControl::stopped, this, &WaitForStopDialog::runControlFinished);

  m_timer.start();
}

auto WaitForStopDialog::canceled() -> bool
{
  return !m_runControls.isEmpty();
}

auto WaitForStopDialog::updateProgressText() -> void
{
  QString text = tr("Waiting for applications to stop.") + QLatin1String("\n\n");
  const auto names = Utils::transform(m_runControls, &RunControl::displayName);
  text += names.join(QLatin1Char('\n'));
  m_progressLabel->setText(text);
}

auto WaitForStopDialog::runControlFinished() -> void
{
  const auto rc = qobject_cast<RunControl*>(sender());
  m_runControls.removeOne(rc);

  if (m_runControls.isEmpty()) {
    if (m_timer.elapsed() < 1000)
      QTimer::singleShot(1000 - m_timer.elapsed(), this, &QDialog::close);
    else
      close();
  } else {
    updateProgressText();
  }
}
