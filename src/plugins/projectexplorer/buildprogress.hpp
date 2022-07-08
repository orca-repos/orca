// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "taskwindow.hpp"

#include <QPointer>
#include <QWidget>

QT_FORWARD_DECLARE_CLASS(QLabel)

namespace ProjectExplorer {
namespace Internal {

class BuildProgress : public QWidget {
  Q_OBJECT

public:
  explicit BuildProgress(TaskWindow *taskWindow, Qt::Orientation orientation = Qt::Vertical);

private:
  auto updateState() -> void;

  QWidget *m_contentWidget;
  QLabel *m_errorIcon;
  QLabel *m_warningIcon;
  QLabel *m_errorLabel;
  QLabel *m_warningLabel;
  QPointer<TaskWindow> m_taskWindow;
};

} // namespace Internal
} // namespace ProjectExplorer
