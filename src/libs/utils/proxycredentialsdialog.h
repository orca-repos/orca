// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"
#include <QDialog>

QT_FORWARD_DECLARE_CLASS(QNetworkProxy)

namespace Utils {

namespace Ui {
class ProxyCredentialsDialog;
}

class ORCA_UTILS_EXPORT ProxyCredentialsDialog : public QDialog {
  Q_OBJECT

public:
  explicit ProxyCredentialsDialog(const QNetworkProxy &proxy, QWidget *parent = nullptr);
  ~ProxyCredentialsDialog() override;

  auto userName() const -> QString;
  auto setUserName(const QString &username) -> void;
  auto password() const -> QString;
  auto setPassword(const QString &passwd) -> void;

private:
  Ui::ProxyCredentialsDialog *ui;
};

} // namespace Utils
