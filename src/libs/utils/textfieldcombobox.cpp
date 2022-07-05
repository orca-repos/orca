// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "textfieldcombobox.h"

#include "qtcassert.h"

namespace Utils {

/*!
    \class Utils::TextFieldComboBox
    \brief The TextFieldComboBox class is a non-editable combo box for text
    editing purposes that plays with \c QWizard::registerField (providing a
    settable 'text' property).

    Allows for a separation of values to be used for wizard fields replacement
    and display texts.
*/

TextFieldComboBox::TextFieldComboBox(QWidget *parent) : QComboBox(parent)
{
  setEditable(false);
  connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TextFieldComboBox::slotCurrentIndexChanged);
}

auto TextFieldComboBox::text() const -> QString
{
  return valueAt(currentIndex());
}

auto TextFieldComboBox::setText(const QString &s) -> void
{
  const int index = findData(QVariant(s), Qt::UserRole);
  if (index != -1 && index != currentIndex())
    setCurrentIndex(index);
}

auto TextFieldComboBox::slotCurrentIndexChanged(int i) -> void
{
  emit text4Changed(valueAt(i));
}

auto TextFieldComboBox::setItems(const QStringList &displayTexts, const QStringList &values) -> void
{
  QTC_ASSERT(displayTexts.size() == values.size(), return);
  clear();
  addItems(displayTexts);
  const int count = values.count();
  for (int i = 0; i < count; i++)
    setItemData(i, QVariant(values.at(i)), Qt::UserRole);
}

auto TextFieldComboBox::valueAt(int i) const -> QString
{
  return i >= 0 && i < count() ? itemData(i, Qt::UserRole).toString() : QString();
}

} // namespace Utils
