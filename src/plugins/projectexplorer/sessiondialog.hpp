// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_sessiondialog.h"

#include <QString>
#include <QDialog>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

namespace ProjectExplorer {
namespace Internal {

class SessionDialog : public QDialog {
  Q_OBJECT

public:
  explicit SessionDialog(QWidget *parent = nullptr);
  auto setAutoLoadSession(bool) -> void;
  auto autoLoadSession() const -> bool;

private:
  auto updateActions(const QStringList &sessions) -> void;

  Ui::SessionDialog m_ui;
};

class SessionNameInputDialog : public QDialog {
  Q_OBJECT

public:
  explicit SessionNameInputDialog(QWidget *parent);

  auto setActionText(const QString &actionText, const QString &openActionText) -> void;
  auto setValue(const QString &value) -> void;
  auto value() const -> QString;
  auto isSwitchToRequested() const -> bool;

private:
  QLineEdit *m_newSessionLineEdit = nullptr;
  QPushButton *m_switchToButton = nullptr;
  QPushButton *m_okButton = nullptr;
  bool m_usedSwitchTo = false;
};

} // namespace Internal
} // namespace ProjectExplorer
