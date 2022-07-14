// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-mime-type-magic-dialog.hpp"

#include "core-interface.hpp"

#include <utils/headerviewstretcher.hpp>
#include <utils/qtcassert.hpp>

#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>

namespace Orca::Plugin::Core {

static auto typeValue(const int i) -> Utils::Internal::MimeMagicRule::Type
{
  QTC_ASSERT(i < Utils::Internal::MimeMagicRule::Byte, return Utils::Internal::MimeMagicRule::Invalid);
  return static_cast<Utils::Internal::MimeMagicRule::Type>(i + 1/*0==invalid*/);
}

MimeTypeMagicDialog::MimeTypeMagicDialog(QWidget *parent) : QDialog(parent)
{
  ui.setupUi(this);
  setWindowTitle(tr("Add Magic Header"));

  connect(ui.useRecommendedGroupBox, &QGroupBox::toggled, this, &MimeTypeMagicDialog::applyRecommended);
  connect(ui.buttonBox, &QDialogButtonBox::accepted, this, &MimeTypeMagicDialog::validateAccept);
  connect(ui.informationLabel, &QLabel::linkActivated, this, [](const QString &link) {
    QDesktopServices::openUrl(QUrl(link));
  });
  connect(ui.typeSelector, QOverload<int>::of(&QComboBox::activated), this, [this]() {
    if (ui.useRecommendedGroupBox->isChecked())
      setToRecommendedValues();
  });

  ui.valueLineEdit->setFocus();
}

auto MimeTypeMagicDialog::setToRecommendedValues() const -> void
{
  ui.startRangeSpinBox->setValue(0);
  ui.endRangeSpinBox->setValue(ui.typeSelector->currentIndex() == 1/*regexp*/ ? 200 : 0);
  ui.prioritySpinBox->setValue(50);
}

auto MimeTypeMagicDialog::applyRecommended(const bool checked) -> void
{
  if (checked) {
    // save previous custom values
    m_custom_range_start = ui.startRangeSpinBox->value();
    m_custom_range_end = ui.endRangeSpinBox->value();
    m_custom_priority = ui.prioritySpinBox->value();
    setToRecommendedValues();
  } else {
    // restore previous custom values
    ui.startRangeSpinBox->setValue(m_custom_range_start);
    ui.endRangeSpinBox->setValue(m_custom_range_end);
    ui.prioritySpinBox->setValue(m_custom_priority);
  }

  ui.startRangeLabel->setEnabled(!checked);
  ui.startRangeSpinBox->setEnabled(!checked);
  ui.endRangeLabel->setEnabled(!checked);
  ui.endRangeSpinBox->setEnabled(!checked);
  ui.priorityLabel->setEnabled(!checked);
  ui.prioritySpinBox->setEnabled(!checked);
  ui.noteLabel->setEnabled(!checked);
}

auto MimeTypeMagicDialog::validateAccept() -> void
{
  QString error_message;

  if (const auto rule = createRule(&error_message); rule.isValid())
    accept();
  else
    QMessageBox::critical(ICore::dialogParent(), tr("Error"), error_message);
}

auto MimeTypeMagicDialog::setMagicData(const MagicData &data) const -> void
{
  ui.valueLineEdit->setText(QString::fromUtf8(data.m_rule.value()));
  ui.typeSelector->setCurrentIndex(data.m_rule.type() - 1/*0 == invalid*/);
  ui.maskLineEdit->setText(QString::fromLatin1(MagicData::normalizedMask(data.m_rule)));
  ui.useRecommendedGroupBox->setChecked(false); // resets values
  ui.startRangeSpinBox->setValue(data.m_rule.startPos());
  ui.endRangeSpinBox->setValue(data.m_rule.endPos());
  ui.prioritySpinBox->setValue(data.m_priority);
}

auto MimeTypeMagicDialog::magicData() const -> MagicData
{
  MagicData data(createRule(), ui.prioritySpinBox->value());
  return data;
}

auto MagicData::operator==(const MagicData &other) const -> bool
{
  return m_priority == other.m_priority && m_rule == other.m_rule;
}

/*!
    Returns the mask, or an empty string if the mask is the default mask which is set by
    MimeMagicRule when setting an empty mask for string patterns.
 */
auto MagicData::normalizedMask(const Utils::Internal::MimeMagicRule &rule) -> QByteArray
{
  // convert mask and see if it is the "default" one (which corresponds to "empty" mask)
  // see MimeMagicRule constructor
  auto mask = rule.mask();

  if (rule.type() == Utils::Internal::MimeMagicRule::String) {
    if (const auto actual_mask = QByteArray::fromHex(QByteArray::fromRawData(mask.constData() + 2, mask.size() - 2)); actual_mask.count(static_cast<char>(-1)) == actual_mask.size()) {
      // is the default-filled 0xfffffffff mask
      mask.clear();
    }
  }

  return mask;
}

auto MimeTypeMagicDialog::createRule(QString *error_message) const -> Utils::Internal::MimeMagicRule
{
  const auto type = typeValue(ui.typeSelector->currentIndex());
  Utils::Internal::MimeMagicRule rule(type, ui.valueLineEdit->text().toUtf8(), ui.startRangeSpinBox->value(), ui.endRangeSpinBox->value(), ui.maskLineEdit->text().toLatin1(), error_message);

  if (type == Utils::Internal::MimeMagicRule::Invalid) {
    if (error_message)
      *error_message = tr("Internal error: Type is invalid");
  }

  return rule;
}

} // namespace Orca::Plugin::Core
