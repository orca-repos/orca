// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "devicefactoryselectiondialog.hpp"
#include "ui_devicefactoryselectiondialog.h"

#include "idevice.hpp"
#include "idevicefactory.hpp"

#include <utils/fileutils.hpp>

#include <QPushButton>

namespace ProjectExplorer {
namespace Internal {

DeviceFactorySelectionDialog::DeviceFactorySelectionDialog(QWidget *parent) : QDialog(parent), ui(new Ui::DeviceFactorySelectionDialog)
{
  ui->setupUi(this);
  ui->buttonBox->button(QDialogButtonBox::Ok)->setText(tr("Start Wizard"));

  for (const IDeviceFactory *const factory : IDeviceFactory::allDeviceFactories()) {
    if (!factory->canCreate())
      continue;
    QListWidgetItem *item = new QListWidgetItem(factory->displayName());
    item->setData(Qt::UserRole, QVariant::fromValue(factory->deviceType()));
    ui->listWidget->addItem(item);
  }

  connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, &DeviceFactorySelectionDialog::handleItemSelectionChanged);
  connect(ui->listWidget, &QListWidget::itemDoubleClicked, this, &DeviceFactorySelectionDialog::handleItemDoubleClicked);
  handleItemSelectionChanged();
}

DeviceFactorySelectionDialog::~DeviceFactorySelectionDialog()
{
  delete ui;
}

auto DeviceFactorySelectionDialog::handleItemSelectionChanged() -> void
{
  ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(!ui->listWidget->selectedItems().isEmpty());
}

auto DeviceFactorySelectionDialog::handleItemDoubleClicked() -> void
{
  accept();
}

auto DeviceFactorySelectionDialog::selectedId() const -> Utils::Id
{
  const QList<QListWidgetItem*> selected = ui->listWidget->selectedItems();
  if (selected.isEmpty())
    return Utils::Id();
  return selected.at(0)->data(Qt::UserRole).value<Utils::Id>();
}

} // namespace Internal
} // namespace ProjectExplorer
