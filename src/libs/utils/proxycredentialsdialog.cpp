// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "proxycredentialsdialog.h"
#include "ui_proxycredentialsdialog.h"

#include <utils/networkaccessmanager.h>
#include <QNetworkProxy>

using namespace Utils;

/*!
    \class Utils::ProxyCredentialsDialog

    Dialog for asking the user about proxy credentials (username, password).
*/

ProxyCredentialsDialog::ProxyCredentialsDialog(const QNetworkProxy &proxy, QWidget *parent) : QDialog(parent), ui(new Ui::ProxyCredentialsDialog)
{
  ui->setupUi(this);

  setUserName(proxy.user());
  setPassword(proxy.password());

  const QString proxyString = QString::fromLatin1("%1:%2").arg(proxy.hostName()).arg(proxy.port());
  ui->infotext->setText(ui->infotext->text().arg(proxyString));
}

ProxyCredentialsDialog::~ProxyCredentialsDialog()
{
  delete ui;
}

auto ProxyCredentialsDialog::userName() const -> QString
{
  return ui->usernameLineEdit->text();
}

auto ProxyCredentialsDialog::setUserName(const QString &username) -> void
{
  ui->usernameLineEdit->setText(username);
}

auto ProxyCredentialsDialog::password() const -> QString
{
  return ui->passwordLineEdit->text();
}

auto ProxyCredentialsDialog::setPassword(const QString &passwd) -> void
{
  ui->passwordLineEdit->setText(passwd);
}

