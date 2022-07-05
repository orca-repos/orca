// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "newdialog.h"

#include <core/iwizardfactory.h>

#include <QDialog>
#include <QList>

QT_BEGIN_NAMESPACE
class QModelIndex;
class QSortFilterProxyModel;
class QPushButton;
class QStandardItem;
class QStandardItemModel;
QT_END_NAMESPACE

namespace Core {
namespace Internal {

namespace Ui {
class NewDialog;
}

class NewDialogWidget final : public QDialog, public NewDialog {
  Q_OBJECT

public:
  explicit NewDialogWidget(QWidget *parent);
  ~NewDialogWidget() override;

  auto setWizardFactories(QList<IWizardFactory*> factories, const Utils::FilePath &default_location, const QVariantMap &extra_variables) -> void override;
  auto showDialog() -> void override;
  auto selectedPlatform() const -> Utils::Id;
  auto widget() -> QWidget* override { return this; }

  auto setWindowTitle(const QString &title) -> void override
  {
    QDialog::setWindowTitle(title);
  }

protected:
  auto event(QEvent *) -> bool override;

private:
  auto currentCategoryChanged(const QModelIndex &) const -> void;
  auto currentItemChanged(const QModelIndex &) -> void;
  auto accept() -> void override;
  auto reject() -> void override;
  auto updateOkButton() const -> void;
  auto setSelectedPlatform(int index) const -> void;
  auto currentWizardFactory() const -> Core::IWizardFactory*;
  auto addItem(QStandardItem *top_level_category_item, IWizardFactory *factory) -> void;
  auto saveState() const -> void;

  Ui::NewDialog *m_ui;
  QStandardItemModel *m_model;
  QSortFilterProxyModel *m_filter_proxy_model;
  QPushButton *m_ok_button = nullptr;
  QList<QStandardItem*> m_category_items;
  Utils::FilePath m_default_location;
  QVariantMap m_extra_variables;
};

} // namespace Internal
} // namespace Core
