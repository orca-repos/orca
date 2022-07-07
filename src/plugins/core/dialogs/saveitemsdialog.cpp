// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "saveitemsdialog.hpp"

#include <core/diffservice.hpp>
#include <core/fileiconprovider.hpp>
#include <core/idocument.hpp>

#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>

#include <QDir>
#include <QFileInfo>
#include <QPushButton>

Q_DECLARE_METATYPE(Core::IDocument*)

namespace Core {
namespace Internal {

SaveItemsDialog::SaveItemsDialog(QWidget *parent, const QList<IDocument*> &items) : QDialog(parent)
{
  m_ui.setupUi(this);

  // QDialogButtonBox's behavior for "destructive" is wrong, the "do not save" should be left-aligned

  QDialogButtonBox::ButtonRole discard_button_role;
  if constexpr (Utils::HostOsInfo::isMacHost())
    discard_button_role = QDialogButtonBox::ResetRole;
  else
    discard_button_role = QDialogButtonBox::DestructiveRole;
  
  if (DiffService::instance()) {
    m_diff_button = m_ui.buttonBox->addButton(tr("&Diff"), discard_button_role);
    connect(m_diff_button, &QAbstractButton::clicked, this, &SaveItemsDialog::collectFilesToDiff);
  }

  const auto discard_button = m_ui.buttonBox->addButton(tr("Do &Not Save"), discard_button_role);
  m_ui.buttonBox->button(QDialogButtonBox::Save)->setDefault(true);
  m_ui.treeWidget->setFocus();
  m_ui.saveBeforeBuildCheckBox->setVisible(false);

  for (auto document : items) {
    QString visible_name;
    QString directory;
    auto file_path = document->filePath();

    if (file_path.isEmpty()) {
      visible_name = document->fallbackSaveAsFileName();
    } else {
      directory = file_path.absolutePath().toUserOutput();
      visible_name = file_path.fileName();
    }

    const auto item = new QTreeWidgetItem(m_ui.treeWidget, QStringList() << visible_name << QDir::toNativeSeparators(directory));

    if (!file_path.isEmpty())
      item->setIcon(0, FileIconProvider::icon(file_path));

    item->setData(0, Qt::UserRole, QVariant::fromValue(document));
  }

  m_ui.treeWidget->resizeColumnToContents(0);
  m_ui.treeWidget->selectAll();

  if constexpr (Utils::HostOsInfo::isMacHost())
    m_ui.treeWidget->setAlternatingRowColors(true);

  adjustButtonWidths();
  updateButtons();

  connect(m_ui.buttonBox->button(QDialogButtonBox::Save), &QAbstractButton::clicked, this, &SaveItemsDialog::collectItemsToSave);
  connect(discard_button, &QAbstractButton::clicked, this, &SaveItemsDialog::discardAll);
  connect(m_ui.treeWidget, &QTreeWidget::itemSelectionChanged, this, &SaveItemsDialog::updateButtons);
}

auto SaveItemsDialog::setMessage(const QString &msg) const -> void
{
  m_ui.msgLabel->setText(msg);
}

auto SaveItemsDialog::updateButtons() const -> void
{
  const auto count = m_ui.treeWidget->selectedItems().count();
  const auto save_button = m_ui.buttonBox->button(QDialogButtonBox::Save);
  auto buttons_enabled = true;
  auto save_text = tr("&Save");
  auto diff_text = tr("&Diff && Cancel");

  if (count == m_ui.treeWidget->topLevelItemCount()) {
    save_text = tr("&Save All");
    diff_text = tr("&Diff All && Cancel");
  } else if (count == 0) {
    buttons_enabled = false;
  } else {
    save_text = tr("&Save Selected");
    diff_text = tr("&Diff Selected && Cancel");
  }

  save_button->setEnabled(buttons_enabled);
  save_button->setText(save_text);

  if (m_diff_button) {
    m_diff_button->setEnabled(buttons_enabled);
    m_diff_button->setText(diff_text);
  }
}

auto SaveItemsDialog::adjustButtonWidths() const -> void
{
  // give save button a size that all texts fit in, so it doesn't get resized
  // Mac: make cancel + save button same size (work around dialog button box issue)
  QStringList possible_texts;
  possible_texts << tr("Save") << tr("Save All");

  if (m_ui.treeWidget->topLevelItemCount() > 1)
    possible_texts << tr("Save Selected");

  auto max_text_width = 0;
  const auto save_button = m_ui.buttonBox->button(QDialogButtonBox::Save);

   for (const auto &text : qAsConst(possible_texts)) {
    save_button->setText(text);
    if (const auto hint = save_button->sizeHint().width(); hint > max_text_width)
      max_text_width = hint;
  }

  if constexpr (Utils::HostOsInfo::isMacHost()) {
    auto cancel_button = m_ui.buttonBox->button(QDialogButtonBox::Cancel);
    if (auto cancel_button_width = cancel_button->sizeHint().width(); cancel_button_width > max_text_width)
      max_text_width = cancel_button_width;
    cancel_button->setMinimumWidth(max_text_width);
  }
  save_button->setMinimumWidth(max_text_width);
}

auto SaveItemsDialog::collectItemsToSave() -> void
{
  m_items_to_save.clear();
  for (const auto items = m_ui.treeWidget->selectedItems(); const QTreeWidgetItem *item : items) {
    m_items_to_save.append(item->data(0, Qt::UserRole).value<IDocument*>());
  }
  accept();
}

auto SaveItemsDialog::collectFilesToDiff() -> void
{
  m_files_to_diff.clear();
  for (const auto items = m_ui.treeWidget->selectedItems(); const QTreeWidgetItem *item : items) {
    if (const auto doc = item->data(0, Qt::UserRole).value<IDocument*>())
      m_files_to_diff.append(doc->filePath().toString());
  }
  reject();
}

auto SaveItemsDialog::discardAll() -> void
{
  m_ui.treeWidget->clearSelection();
  collectItemsToSave();
}

auto SaveItemsDialog::itemsToSave() const -> QList<IDocument*>
{
  return m_items_to_save;
}

auto SaveItemsDialog::filesToDiff() const -> QStringList
{
  return m_files_to_diff;
}

auto SaveItemsDialog::setAlwaysSaveMessage(const QString &msg) const -> void
{
  m_ui.saveBeforeBuildCheckBox->setText(msg);
  m_ui.saveBeforeBuildCheckBox->setVisible(true);
}

auto SaveItemsDialog::alwaysSaveChecked() const -> bool
{
  return m_ui.saveBeforeBuildCheckBox->isChecked();
}

} // namespace Internal
} // namespace Core
