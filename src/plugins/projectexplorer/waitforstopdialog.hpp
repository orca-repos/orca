// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QList>
#include <QDialog>
#include <QElapsedTimer>

#include "runcontrol.hpp"

QT_BEGIN_NAMESPACE
class QLabel;
QT_END_NAMESPACE

namespace ProjectExplorer {
namespace Internal {

class WaitForStopDialog : public QDialog {
  Q_OBJECT

public:
  explicit WaitForStopDialog(QList<RunControl*> runControls);

  auto canceled() -> bool;

private:
  auto updateProgressText() -> void;
  auto runControlFinished() -> void;

  QList<RunControl*> m_runControls;
  QLabel *m_progressLabel;
  QElapsedTimer m_timer;
};

} // namespace Internal
} // namespace ProjectExplorer
