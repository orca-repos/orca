// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "settingsselector.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>

namespace Utils {

// --------------------------------------------------------------------------
// SettingsSelector
// --------------------------------------------------------------------------

SettingsSelector::SettingsSelector(QWidget *parent) : QWidget(parent)
{
  auto layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(6);

  m_configurationCombo = new QComboBox(this);
  m_configurationCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
  m_configurationCombo->setMinimumContentsLength(80);

  m_addButton = new QPushButton(tr("Add"), this);
  m_removeButton = new QPushButton(tr("Remove"), this);
  m_renameButton = new QPushButton(tr("Rename"), this);

  m_label = new QLabel(this);
  m_label->setMinimumWidth(200);
  m_label->setBuddy(m_configurationCombo);

  layout->addWidget(m_label);
  layout->addWidget(m_configurationCombo);
  layout->addWidget(m_addButton);
  layout->addWidget(m_removeButton);
  layout->addWidget(m_renameButton);

  layout->addSpacerItem(new QSpacerItem(0, 0));

  updateButtonState();

  connect(m_addButton, &QAbstractButton::clicked, this, &SettingsSelector::add);
  connect(m_removeButton, &QAbstractButton::clicked, this, &SettingsSelector::removeButtonClicked);
  connect(m_renameButton, &QAbstractButton::clicked, this, &SettingsSelector::renameButtonClicked);
  connect(m_configurationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsSelector::currentChanged);
}

SettingsSelector::~SettingsSelector() = default;

auto SettingsSelector::setConfigurationModel(QAbstractItemModel *model) -> void
{
  if (m_configurationCombo->model()) {
    disconnect(m_configurationCombo->model(), &QAbstractItemModel::rowsInserted, this, &SettingsSelector::updateButtonState);
    disconnect(m_configurationCombo->model(), &QAbstractItemModel::rowsRemoved, this, &SettingsSelector::updateButtonState);
  }
  m_configurationCombo->setModel(model);
  connect(model, &QAbstractItemModel::rowsInserted, this, &SettingsSelector::updateButtonState);
  connect(model, &QAbstractItemModel::rowsRemoved, this, &SettingsSelector::updateButtonState);

  updateButtonState();
}

auto SettingsSelector::configurationModel() const -> QAbstractItemModel*
{
  return m_configurationCombo->model();
}

auto SettingsSelector::setLabelText(const QString &text) -> void
{
  m_label->setText(text);
}

auto SettingsSelector::labelText() const -> QString
{
  return m_label->text();
}

auto SettingsSelector::setCurrentIndex(int index) -> void
{
  m_configurationCombo->setCurrentIndex(index);
}

auto SettingsSelector::setAddMenu(QMenu *menu) -> void
{
  m_addButton->setMenu(menu);
}

auto SettingsSelector::addMenu() const -> QMenu*
{
  return m_addButton->menu();
}

auto SettingsSelector::currentIndex() const -> int
{
  return m_configurationCombo->currentIndex();
}

auto SettingsSelector::removeButtonClicked() -> void
{
  int pos = currentIndex();
  if (pos < 0)
    return;
  const QString title = tr("Remove");
  const QString message = tr("Do you really want to delete the configuration <b>%1</b>?").arg(m_configurationCombo->currentText());
  QMessageBox msgBox(QMessageBox::Question, title, message, QMessageBox::Yes | QMessageBox::No, this);
  msgBox.setDefaultButton(QMessageBox::No);
  msgBox.setEscapeButton(QMessageBox::No);
  if (msgBox.exec() == QMessageBox::No)
    return;

  emit remove(pos);
}

auto SettingsSelector::renameButtonClicked() -> void
{
  int pos = currentIndex();
  if (pos < 0)
    return;

  QAbstractItemModel *model = m_configurationCombo->model();
  int row = m_configurationCombo->currentIndex();
  QModelIndex idx = model->index(row, 0);
  QString baseName = model->data(idx, Qt::EditRole).toString();

  bool ok;
  const QString message = tr("New name for configuration <b>%1</b>:").arg(baseName);

  QString name = QInputDialog::getText(this, tr("Rename..."), message, QLineEdit::Normal, baseName, &ok);
  if (!ok)
    return;

  emit rename(pos, name);
}

auto SettingsSelector::updateButtonState() -> void
{
  bool haveItems = m_configurationCombo->count() > 0;
  m_addButton->setEnabled(true);
  m_removeButton->setEnabled(haveItems);
  m_renameButton->setEnabled(haveItems);
}

} // namespace Utils
