// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "sessiondialog.hpp"
#include "session.hpp"

#include <utils/algorithm.hpp>

#include <QInputDialog>
#include <QValidator>

using namespace ProjectExplorer;
using namespace Internal;

namespace ProjectExplorer {
namespace Internal {

class SessionValidator : public QValidator {
public:
  SessionValidator(QObject *parent, const QStringList &sessions);
  auto fixup(QString &input) const -> void override;
  auto validate(QString &input, int &pos) const -> State override;

private:
  QStringList m_sessions;
};

SessionValidator::SessionValidator(QObject *parent, const QStringList &sessions) : QValidator(parent), m_sessions(sessions) {}

auto SessionValidator::validate(QString &input, int &pos) const -> State
{
  Q_UNUSED(pos)

  if (input.contains(QLatin1Char('/')) || input.contains(QLatin1Char(':')) || input.contains(QLatin1Char('\\')) || input.contains(QLatin1Char('?')) || input.contains(QLatin1Char('*')))
    return Invalid;

  if (m_sessions.contains(input))
    return Intermediate;
  else
    return Acceptable;
}

auto SessionValidator::fixup(QString &input) const -> void
{
  auto i = 2;
  QString copy;
  do {
    copy = input + QLatin1String(" (") + QString::number(i) + QLatin1Char(')');
    ++i;
  } while (m_sessions.contains(copy));
  input = copy;
}

SessionNameInputDialog::SessionNameInputDialog(QWidget *parent) : QDialog(parent)
{
  const auto hlayout = new QVBoxLayout(this);
  auto label = new QLabel(tr("Enter the name of the session:"), this);
  hlayout->addWidget(label);
  m_newSessionLineEdit = new QLineEdit(this);
  m_newSessionLineEdit->setValidator(new SessionValidator(this, SessionManager::sessions()));
  hlayout->addWidget(m_newSessionLineEdit);
  auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  m_okButton = buttons->button(QDialogButtonBox::Ok);
  m_switchToButton = new QPushButton;
  buttons->addButton(m_switchToButton, QDialogButtonBox::AcceptRole);
  connect(m_switchToButton, &QPushButton::clicked, [this]() {
    m_usedSwitchTo = true;
  });
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  hlayout->addWidget(buttons);
  setLayout(hlayout);
}

auto SessionNameInputDialog::setActionText(const QString &actionText, const QString &openActionText) -> void
{
  m_okButton->setText(actionText);
  m_switchToButton->setText(openActionText);
}

auto SessionNameInputDialog::setValue(const QString &value) -> void
{
  m_newSessionLineEdit->setText(value);
}

auto SessionNameInputDialog::value() const -> QString
{
  return m_newSessionLineEdit->text();
}

auto SessionNameInputDialog::isSwitchToRequested() const -> bool
{
  return m_usedSwitchTo;
}

SessionDialog::SessionDialog(QWidget *parent) : QDialog(parent)
{
  m_ui.setupUi(this);
  m_ui.sessionView->setActivationMode(Utils::DoubleClickActivation);

  connect(m_ui.btCreateNew, &QAbstractButton::clicked, m_ui.sessionView, &SessionView::createNewSession);
  connect(m_ui.btClone, &QAbstractButton::clicked, m_ui.sessionView, &SessionView::cloneCurrentSession);
  connect(m_ui.btDelete, &QAbstractButton::clicked, m_ui.sessionView, &SessionView::deleteSelectedSessions);
  connect(m_ui.btSwitch, &QAbstractButton::clicked, m_ui.sessionView, &SessionView::switchToCurrentSession);
  connect(m_ui.btRename, &QAbstractButton::clicked, m_ui.sessionView, &SessionView::renameCurrentSession);
  connect(m_ui.sessionView, &SessionView::sessionActivated, m_ui.sessionView, &SessionView::switchToCurrentSession);

  connect(m_ui.sessionView, &SessionView::sessionsSelected, this, &SessionDialog::updateActions);
  connect(m_ui.sessionView, &SessionView::sessionSwitched, this, &QDialog::reject);

  m_ui.whatsASessionLabel->setOpenExternalLinks(true);
}

auto SessionDialog::setAutoLoadSession(bool check) -> void
{
  m_ui.autoLoadCheckBox->setChecked(check);
}

auto SessionDialog::autoLoadSession() const -> bool
{
  return m_ui.autoLoadCheckBox->checkState() == Qt::Checked;
}

auto SessionDialog::updateActions(const QStringList &sessions) -> void
{
  if (sessions.isEmpty()) {
    m_ui.btDelete->setEnabled(false);
    m_ui.btRename->setEnabled(false);
    m_ui.btClone->setEnabled(false);
    m_ui.btSwitch->setEnabled(false);
    return;
  }
  const auto defaultIsSelected = sessions.contains("default");
  const auto activeIsSelected = Utils::anyOf(sessions, [](const QString &session) {
    return session == SessionManager::activeSession();
  });
  m_ui.btDelete->setEnabled(!defaultIsSelected && !activeIsSelected);
  m_ui.btRename->setEnabled(sessions.size() == 1 && !defaultIsSelected);
  m_ui.btClone->setEnabled(sessions.size() == 1);
  m_ui.btSwitch->setEnabled(sessions.size() == 1);
}

} // namespace Internal
} // namespace ProjectExplorer
