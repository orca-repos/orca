// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-readonly-files-dialog.hpp"
#include "ui_core-readonly-files-dialog.h"

#include "core-document-interface.hpp"
#include "core-editor-manager-private.hpp"
#include "core-file-icon-provider.hpp"
#include "core-interface.hpp"
#include "core-vcs-manager.hpp"
#include "core-version-control-interface.hpp"

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/stringutils.hpp>

#include <QButtonGroup>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>

using namespace Utils;

namespace Orca::Plugin::Core {

class ReadOnlyFilesDialogPrivate {
  Q_DECLARE_TR_FUNCTIONS(Orca::Plugin::Core::ReadOnlyFilesDialog)
  Q_DISABLE_COPY_MOVE(ReadOnlyFilesDialogPrivate)

public:
  explicit ReadOnlyFilesDialogPrivate(ReadOnlyFilesDialog *parent, IDocument *document = nullptr, bool display_save_as = false);
  ~ReadOnlyFilesDialogPrivate();

  enum read_only_files_tree_column {
    make_writable = ReadOnlyFilesDialog::MakeWritable,
    open_with_vcs = ReadOnlyFilesDialog::OpenWithVCS,
    save_as = ReadOnlyFilesDialog::SaveAs,
    file_name = ReadOnlyFilesDialog::FileName,
    folder = ReadOnlyFilesDialog::Folder,
    number_of_columns
  };

  auto initDialog(const FilePaths &file_paths) -> void;
  auto promptFailWarning(const FilePaths &files, ReadOnlyFilesDialog::ReadOnlyResult type) const -> void;
  auto createRadioButtonForItem(QTreeWidgetItem *item, QButtonGroup *group, read_only_files_tree_column type) const -> QRadioButton*;
  auto setAll(int index) -> void;
  auto updateSelectAll() -> void;

  ReadOnlyFilesDialog *q;

  // Buttongroups containing the operation for one file.
  struct ButtonGroupForFile {
    FilePath file_path;
    QButtonGroup *group;
  };

  QList<ButtonGroupForFile> button_groups;
  QMap<int, int> set_all_index_for_operation;

  // The version control systems for every file, if the file isn't in VCS the value is 0.
  QHash<FilePath, IVersionControl*> version_controls;

  // Define if some specific operations should be allowed to make the files writable.
  const bool use_save_as;
  bool use_vcs;

  // Define if an error should be displayed when an operation fails.
  bool show_warnings;
  QString fail_warning;

  // The document is necessary for the Save As operation.
  IDocument *document;

  // Operation text for the tree widget header and combo box entries for
  // modifying operations for all files.
  const QString mixed_text;
  QString make_writable_text;
  QString version_control_open_text;
  const QString save_as_text;
  Ui::ReadOnlyFilesDialog ui{};
};

ReadOnlyFilesDialogPrivate::ReadOnlyFilesDialogPrivate(ReadOnlyFilesDialog *parent, IDocument *document, const bool display_save_as) : q(parent), use_save_as(display_save_as), use_vcs(false), show_warnings(false), document(document), mixed_text(tr("Mixed")), make_writable_text(tr("Make Writable")), version_control_open_text(tr("Open with VCS")), save_as_text(tr("Save As")) { }

ReadOnlyFilesDialogPrivate::~ReadOnlyFilesDialogPrivate()
{
  for (const auto &[file_path, group] : qAsConst(button_groups))
    delete group;
}

using namespace Internal;

/*!
 * \class Orca::Plugin::Core::ReadOnlyFilesDialog
 * \inmodule Orca
 * \internal
 * \brief The ReadOnlyFilesDialog class implements a dialog to show a set of
 * files that are classified as not writable.
 *
 * Automatically checks which operations are allowed to make the file writable. These operations
 * are \c MakeWritable (RO_MakeWritable), which tries to set the file permissions in the file system,
 * \c OpenWithVCS (RO_OpenVCS) if the open operation is allowed by the version control system,
 * and \c SaveAs (RO_SaveAs), which is used to save the changes to a document under another file
 * name.
 */

/*! \enum ReadOnlyFilesDialog::ReadOnlyResult
    This enum holds the operations that are allowed to make the file writable.

     \value RO_Cancel
            Cancels the operation.
     \value RO_OpenVCS
            Opens the file under control of the version control system.
     \value RO_MakeWritable
            Sets the file permissions in the file system.
     \value RO_SaveAs
            Saves changes to a document under another file name.
*/

ReadOnlyFilesDialog::ReadOnlyFilesDialog(const FilePaths &file_paths, QWidget *parent) : QDialog(parent), d(new ReadOnlyFilesDialogPrivate(this))
{
  d->initDialog(file_paths);
}

ReadOnlyFilesDialog::ReadOnlyFilesDialog(const FilePath &file_path, QWidget *parent) : QDialog(parent), d(new ReadOnlyFilesDialogPrivate(this))
{
  d->initDialog({file_path});
}

ReadOnlyFilesDialog::ReadOnlyFilesDialog(IDocument *document, QWidget *parent, bool displaySaveAs) : QDialog(parent), d(new ReadOnlyFilesDialogPrivate(this, document, displaySaveAs))
{
  d->initDialog({document->filePath()});
}

ReadOnlyFilesDialog::ReadOnlyFilesDialog(const QList<IDocument*> &documents, QWidget *parent) : QDialog(parent), d(new ReadOnlyFilesDialogPrivate(this))
{
  FilePaths files;
  for (const auto document : documents)
    files << document->filePath();
  d->initDialog(files);
}

ReadOnlyFilesDialog::~ReadOnlyFilesDialog()
{
  delete d;
}

/*!
 * Sets a user defined message in the dialog.
 * \internal
 */
auto ReadOnlyFilesDialog::setMessage(const QString &message) const -> void
{
  d->ui.msgLabel->setText(message);
}

/*!
 * Enables the error output to the user via a message box. \a warning should
 * show the possible consequences if the file is still read only.
 * \internal
 */
auto ReadOnlyFilesDialog::setShowFailWarning(const bool show, const QString &warning) const -> void
{
  d->show_warnings = show;
  d->fail_warning = warning;
}

/*!
 * Opens a message box with an error description according to the type.
 * \internal
 */
auto ReadOnlyFilesDialogPrivate::promptFailWarning(const FilePaths &files, const ReadOnlyFilesDialog::ReadOnlyResult type) const -> void
{
  if (files.isEmpty())
    return;

  QString title;
  QString message;
  QString details;

  if (files.count() == 1) {
    const auto &file = files.first();
    switch (type) {
    case ReadOnlyFilesDialog::RO_OpenVCS: {
      if (const auto vc = version_controls[file]) {
        const auto open_text = stripAccelerator(vc->vcsOpenText());
        title = tr("Failed to %1 File").arg(open_text);
        message = tr("%1 file %2 from version control system %3 failed.").arg(open_text).arg(file.toUserOutput()).arg(vc->displayName()) + '\n' + fail_warning;
      } else {
        title = tr("No Version Control System Found");
        message = tr("Cannot open file %1 from version control system.\n" "No version control system found.").arg(file.toUserOutput()) + '\n' + fail_warning;
      }
      break;
    }
    case ReadOnlyFilesDialog::RO_MakeWritable:
      title = tr("Cannot Set Permissions");
      message = tr("Cannot set permissions for %1 to writable.").arg(file.toUserOutput()) + '\n' + fail_warning;
      break;
    case ReadOnlyFilesDialog::RO_SaveAs:
      title = tr("Cannot Save File");
      message = tr("Cannot save file %1").arg(file.toUserOutput()) + '\n' + fail_warning;
      break;
    default:
      title = tr("Canceled Changing Permissions");
      message = fail_warning;
      break;
    }
  } else {
    title = tr("Could Not Change Permissions on Some Files");
    message = fail_warning + QLatin1Char('\n') + tr("See details for a complete list of files.");
    details = transform(files, &FilePath::toString).join('\n');
  }

  QMessageBox msg_box(QMessageBox::Warning, title, message, QMessageBox::Ok, ICore::dialogParent());
  msg_box.setDetailedText(details);
  msg_box.exec();
}

/*!
 * Executes the ReadOnlyFilesDialog dialog.
 * Returns ReadOnlyResult to provide information about the operation that was
 * used to make the files writable.
 *
 * \internal
 *
 * Also displays an error dialog when some operations cannot be executed and the
 * function \c setShowFailWarning() was called.
 */
auto ReadOnlyFilesDialog::exec() -> int
{
  if (QDialog::exec() != Accepted)
    return RO_Cancel;

  auto result = RO_Cancel;
  FilePaths failed_to_make_writable;

  for (const auto &[file_path, group] : qAsConst(d->button_groups)) {
    result = static_cast<ReadOnlyResult>(group->checkedId());

    switch (result) {
    case RO_MakeWritable:
      if (!FileUtils::makeWritable(file_path)) {
        failed_to_make_writable << file_path;
        continue;
      }
      break;
    case RO_OpenVCS:
      if (!d->version_controls[file_path]->vcsOpen(file_path)) {
        failed_to_make_writable << file_path;
        continue;
      }
      break;
    case RO_SaveAs:
      if (!EditorManagerPrivate::saveDocumentAs(d->document)) {
        failed_to_make_writable << file_path;
        continue;
      }
      break;
    default:
      failed_to_make_writable << file_path;
      continue;
    }

    if (!file_path.toFileInfo().isWritable())
      failed_to_make_writable << file_path;
  }

  if (!failed_to_make_writable.isEmpty()) {
    if (d->show_warnings)
      d->promptFailWarning(failed_to_make_writable, result);

  }
  return failed_to_make_writable.isEmpty() ? result : RO_Cancel;
}

/*!
 * Creates a radio button in the group \a group and in the column specified by
 * \a type.
 * Returns the created button.
 * \internal
 */
auto ReadOnlyFilesDialogPrivate::createRadioButtonForItem(QTreeWidgetItem *item, QButtonGroup *group, const read_only_files_tree_column type) const -> QRadioButton*
{
  const auto radio_button = new QRadioButton(q);
  group->addButton(radio_button, type);
  item->setTextAlignment(type, Qt::AlignHCenter);
  ui.treeWidget->setItemWidget(item, type, radio_button);
  return radio_button;
}

/*!
 * Checks the type of the select all combo box and changes the user selection
 * per file accordingly.
 * \internal
 */
auto ReadOnlyFilesDialogPrivate::setAll(const int index) -> void
{
  // If mixed is the current index, no need to change the user selection.
  if (index == set_all_index_for_operation[-1/*mixed*/])
    return;

  // Get the selected type from the select all combo box.
  auto type = number_of_columns;
  if (index == set_all_index_for_operation[make_writable])
    type = make_writable;
  else if (index == set_all_index_for_operation[open_with_vcs])
    type = open_with_vcs;
  else if (index == set_all_index_for_operation[save_as])
    type = save_as;

  // Check for every file if the selected operation is available and change it to the operation.
  for (const auto &[file_path, group] : qAsConst(button_groups)) {
    if (const auto radio_button = qobject_cast<QRadioButton*>(group->button(type)))
      radio_button->setChecked(true);
  }
}

/*!
 * Updates the select all combo box depending on the selection the user made in
 * the tree widget.
 * \internal
 */
auto ReadOnlyFilesDialogPrivate::updateSelectAll() -> void
{
  auto selected_operation = -1;

  for (const auto &[file_path, group] : qAsConst(button_groups)) {
    if (selected_operation == -1) {
      selected_operation = group->checkedId();
    } else if (selected_operation != group->checkedId()) {
      ui.setAll->setCurrentIndex(0);
      return;
    }
  }

  ui.setAll->setCurrentIndex(set_all_index_for_operation[selected_operation]);
}

/*!
 * Adds files to the dialog and checks for a possible operation to make the file
 * writable.
 * \a filePaths contains the list of the files that should be added to the
 * dialog.
 * \internal
 */
auto ReadOnlyFilesDialogPrivate::initDialog(const FilePaths &file_paths) -> void
{
  ui.setupUi(q);
  ui.buttonBox->addButton(tr("Change &Permission"), QDialogButtonBox::AcceptRole);
  ui.buttonBox->addButton(QDialogButtonBox::Cancel);

  QString vcs_open_text_for_all;
  QString vcs_make_writable_text_for_all;

  auto use_make_writable = false;

  for (const auto &file_path : file_paths) {
    const auto visible_name = file_path.fileName();
    const auto directory = file_path.absolutePath();

    // Setup a default entry with filename Folder and make writable radio button.
    const auto item = new QTreeWidgetItem(ui.treeWidget);
    item->setText(file_name, visible_name);
    item->setIcon(file_name, icon(file_path));
    item->setText(folder, directory.shortNativePath());

    const auto radio_button_group = new QButtonGroup;

    // Add a button for opening the file with a version control system
    // if the file is managed by an version control system which allows opening files.
    const auto version_control_for_file = VcsManager::findVersionControlForDirectory(directory);
    const auto file_managed_by_vcs = version_control_for_file && version_control_for_file->openSupportMode(file_path) != IVersionControl::NoOpen;

    if (file_managed_by_vcs) {
      const auto vcs_open_text_for_file = stripAccelerator(version_control_for_file->vcsOpenText());
      const auto vcs_make_writable_textfor_file = stripAccelerator(version_control_for_file->vcsMakeWritableText());
      if (!use_vcs) {
        vcs_open_text_for_all = vcs_open_text_for_file;
        vcs_make_writable_text_for_all = vcs_make_writable_textfor_file;
        use_vcs = true;
      } else {
        // If there are different open or make writable texts choose the default one.
        if (vcs_open_text_for_file != vcs_open_text_for_all)
          vcs_open_text_for_all.clear();
        if (vcs_make_writable_textfor_file != vcs_make_writable_text_for_all)
          vcs_make_writable_text_for_all.clear();
      }
      // Add make writable if it is supported by the reposetory.
      if (version_control_for_file->openSupportMode(file_path) == IVersionControl::OpenOptional) {
        use_make_writable = true;
        createRadioButtonForItem(item, radio_button_group, make_writable);
      }
      createRadioButtonForItem(item, radio_button_group, open_with_vcs)->setChecked(true);
    } else {
      use_make_writable = true;
      createRadioButtonForItem(item, radio_button_group, make_writable)->setChecked(true);
    }

    // Add a Save As radio button if requested.
    if (use_save_as)
      createRadioButtonForItem(item, radio_button_group, save_as);

    // If the file is managed by a version control system save the vcs for this file.
    version_controls[file_path] = file_managed_by_vcs ? version_control_for_file : nullptr;

    // Also save the buttongroup for every file to get the result for each entry.
    button_groups.append({file_path, radio_button_group});
    QObject::connect(radio_button_group, QOverload<QAbstractButton*>::of(&QButtonGroup::buttonClicked), [this] { updateSelectAll(); });
  }

  // Apply the Mac file dialog style.
  if constexpr (HostOsInfo::isMacHost())
    ui.treeWidget->setAlternatingRowColors(true);

  // Do not show any options to the user if he has no choice.
  if (!use_save_as && (!use_vcs || !use_make_writable)) {
    ui.treeWidget->setColumnHidden(make_writable, true);
    ui.treeWidget->setColumnHidden(open_with_vcs, true);
    ui.treeWidget->setColumnHidden(save_as, true);
    ui.treeWidget->resizeColumnToContents(file_name);
    ui.treeWidget->resizeColumnToContents(folder);
    ui.setAll->setVisible(false);
    ui.setAllLabel->setVisible(false);
    ui.verticalLayout->removeItem(ui.setAllLayout);

    if (use_vcs)
      ui.msgLabel->setText(tr("The following files are not checked out yet.\n" "Do you want to check them out now?"));

    return;
  }

  // If there is just one file entry, there is no need to show the select all combo box
  if (file_paths.count() < 2) {
    ui.setAll->setVisible(false);
    ui.setAllLabel->setVisible(false);
    ui.verticalLayout->removeItem(ui.setAllLayout);
  }

  // Add items to the Set all combo box.
  ui.setAll->addItem(mixed_text);
  set_all_index_for_operation[-1/*mixed*/] = ui.setAll->count() - 1;

  if (use_vcs) {
    // If the files are managed by just one version control system, the Open and Make Writable
    // text for the specific system is used.
    if (!vcs_open_text_for_all.isEmpty() && vcs_open_text_for_all != version_control_open_text) {
      version_control_open_text = vcs_open_text_for_all;
      ui.treeWidget->headerItem()->setText(open_with_vcs, version_control_open_text);
    }
    if (!vcs_make_writable_text_for_all.isEmpty() && vcs_make_writable_text_for_all != make_writable_text) {
      make_writable_text = vcs_make_writable_text_for_all;
      ui.treeWidget->headerItem()->setText(make_writable, make_writable_text);
    }
    ui.setAll->addItem(version_control_open_text);
    ui.setAll->setCurrentIndex(ui.setAll->count() - 1);
    set_all_index_for_operation[open_with_vcs] = ui.setAll->count() - 1;
  }

  if (use_make_writable) {
    ui.setAll->addItem(make_writable_text);
    set_all_index_for_operation[make_writable] = ui.setAll->count() - 1;
    if (ui.setAll->currentIndex() == -1)
      ui.setAll->setCurrentIndex(ui.setAll->count() - 1);
  }

  if (use_save_as) {
    ui.setAll->addItem(save_as_text);
    set_all_index_for_operation[save_as] = ui.setAll->count() - 1;
  }

  QObject::connect(ui.setAll, QOverload<int>::of(&QComboBox::activated), [this](const int index) { setAll(index); });

  // Filter which columns should be visible and resize them to content.
  for (auto i = 0; i < number_of_columns; ++i) {
    if ((i == save_as && !use_save_as) || (i == open_with_vcs && !use_vcs) || (i == make_writable && !use_make_writable)) {
      ui.treeWidget->setColumnHidden(i, true);
      continue;
    }
    ui.treeWidget->resizeColumnToContents(i);
  }
}

} // namespace Orca::Plugin::Core
