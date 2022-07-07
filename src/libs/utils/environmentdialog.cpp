// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "environmentdialog.hpp"

#include <utils/environment.hpp>
#include <utils/hostosinfo.hpp>

#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QLabel>

namespace Utils {

auto EnvironmentDialog::getEnvironmentItems(QWidget *parent, const EnvironmentItems &initial, const QString &placeholderText, Polisher polisher) -> Utils::optional<EnvironmentItems>
{
  return getNameValueItems(parent, initial, placeholderText, polisher, tr("Edit Environment"), tr("Enter one environment variable per line.\n" "To set or change a variable, use VARIABLE=VALUE.\n" "To append to a variable, use VARIABLE+=VALUE.\n" "To prepend to a variable, use VARIABLE=+VALUE.\n" "Existing variables can be referenced in a VALUE with ${OTHER}.\n" "To clear a variable, put its name on a line with nothing else on it.\n" "To disable a variable, prefix the line with \"#\"."));
}

} // namespace Utils
