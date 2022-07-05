// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "namevaluesdialog.h"

#include <utils/environment.h>
#include <utils/hostosinfo.h>

#include <QDialogButtonBox>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSet>
#include <QVBoxLayout>

namespace Utils {

namespace Internal {

static auto cleanUp(const EnvironmentItems &items) -> EnvironmentItems
{
  EnvironmentItems uniqueItems;
  QSet<QString> uniqueSet;
  for (int i = items.count() - 1; i >= 0; i--) {
    EnvironmentItem item = items.at(i);
    if (HostOsInfo::isWindowsHost())
      item.name = item.name.toUpper();
    const QString &itemName = item.name;
    QString emptyName = itemName;
    emptyName.remove(QLatin1Char(' '));
    if (!emptyName.isEmpty() && !uniqueSet.contains(itemName)) {
      uniqueItems.prepend(item);
      uniqueSet.insert(itemName);
    }
  }
  return uniqueItems;
}

NameValueItemsWidget::NameValueItemsWidget(QWidget *parent) : QWidget(parent)
{
  m_editor = new QPlainTextEdit(this);
  auto layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_editor);
}

auto NameValueItemsWidget::setEnvironmentItems(const EnvironmentItems &items) -> void
{
  EnvironmentItems sortedItems = items;
  EnvironmentItem::sort(&sortedItems);
  const QStringList list = EnvironmentItem::toStringList(sortedItems);
  m_editor->document()->setPlainText(list.join(QLatin1Char('\n')));
}

auto NameValueItemsWidget::environmentItems() const -> EnvironmentItems
{
  const QStringList list = m_editor->document()->toPlainText().split(QLatin1String("\n"));
  EnvironmentItems items = EnvironmentItem::fromStringList(list);
  return cleanUp(items);
}

auto NameValueItemsWidget::setPlaceholderText(const QString &text) -> void
{
  m_editor->setPlaceholderText(text);
}
} // namespace Internal

NameValuesDialog::NameValuesDialog(const QString &windowTitle, const QString &helpText, QWidget *parent) : QDialog(parent)
{
  resize(640, 480);
  m_editor = new Internal::NameValueItemsWidget(this);
  auto box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);
  connect(box, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);

  auto helpLabel = new QLabel(this);
  helpLabel->setText(helpText);

  auto layout = new QVBoxLayout(this);
  layout->addWidget(m_editor);
  layout->addWidget(helpLabel);

  layout->addWidget(box);

  setWindowTitle(windowTitle);
}

auto NameValuesDialog::setNameValueItems(const EnvironmentItems &items) -> void
{
  m_editor->setEnvironmentItems(items);
}

auto NameValuesDialog::nameValueItems() const -> EnvironmentItems
{
  return m_editor->environmentItems();
}

auto NameValuesDialog::setPlaceholderText(const QString &text) -> void
{
  m_editor->setPlaceholderText(text);
}

auto NameValuesDialog::getNameValueItems(QWidget *parent, const NameValueItems &initial, const QString &placeholderText, Polisher polisher, const QString &windowTitle, const QString &helpText) -> Utils::optional<NameValueItems>
{
  NameValuesDialog dialog(windowTitle, helpText, parent);
  if (polisher)
    polisher(&dialog);
  dialog.setNameValueItems(initial);
  dialog.setPlaceholderText(placeholderText);
  bool result = dialog.exec() == QDialog::Accepted;
  if (result)
    return dialog.nameValueItems();

  return {};
}

} // namespace Utils
