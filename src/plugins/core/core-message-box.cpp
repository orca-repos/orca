// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-message-box.hpp"

#include "core-interface.hpp"

#include <QMessageBox>

namespace Orca::Plugin::Core {
namespace {

auto message(const QMessageBox::Icon icon, const QString &title, const QString &desciption) -> QWidget*
{
  const auto message_box = new QMessageBox(icon, title, desciption, QMessageBox::Ok, ICore::dialogParent());
  message_box->setAttribute(Qt::WA_DeleteOnClose);
  message_box->setModal(true);
  message_box->show();
  return message_box;
}

} // namespace

auto warning(const QString &title, const QString &desciption) -> QWidget*
{
  return message(QMessageBox::Warning, title, desciption);
}

auto information(const QString &title, const QString &desciption) -> QWidget*
{
  return message(QMessageBox::Information, title, desciption);
}

auto critical(const QString &title, const QString &desciption) -> QWidget*
{
  return message(QMessageBox::Critical, title, desciption);
}

} // namespace Orca::Plugin::Core