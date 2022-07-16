// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qrceditor.hpp"
#include "undocommands_p.hpp"

#include <aggregation/aggregate.hpp>
#include <core/core-item-view-find.hpp>

#include <QDebug>
#include <QScopedPointer>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>

using namespace ResourceEditor;
using namespace ResourceEditor::Internal;

QrcEditor::QrcEditor(RelativeResourceModel *model, QWidget *parent) : Orca::Plugin::Core::MiniSplitter(Qt::Vertical, parent), m_treeview(new ResourceView(model, &m_history))
{
  addWidget(m_treeview);
  auto widget = new QWidget;
  m_ui.setupUi(widget);
  addWidget(widget);
  m_treeview->setFrameStyle(QFrame::NoFrame);

  connect(m_ui.addPrefixButton, &QAbstractButton::clicked, this, &QrcEditor::onAddPrefix);
  connect(m_ui.addFilesButton, &QAbstractButton::clicked, this, &QrcEditor::onAddFiles);
  connect(m_ui.removeButton, &QAbstractButton::clicked, this, &QrcEditor::onRemove);
  connect(m_ui.removeNonExistingButton, &QPushButton::clicked, this, &QrcEditor::onRemoveNonExisting);

  connect(m_treeview, &ResourceView::removeItem, this, &QrcEditor::onRemove);
  connect(m_treeview->selectionModel(), &QItemSelectionModel::currentChanged, this, &QrcEditor::updateCurrent);
  connect(m_treeview, &ResourceView::itemActivated, this, &QrcEditor::itemActivated);
  connect(m_treeview, &ResourceView::contextMenuShown, this, &QrcEditor::showContextMenu);
  m_treeview->setFocus();

  connect(m_ui.aliasText, &QLineEdit::textEdited, this, &QrcEditor::onAliasChanged);
  connect(m_ui.prefixText, &QLineEdit::textEdited, this, &QrcEditor::onPrefixChanged);
  connect(m_ui.languageText, &QLineEdit::textEdited, this, &QrcEditor::onLanguageChanged);

  // Prevent undo command merging after a switch of focus:
  // (0) The initial text is "Green".
  // (1) The user appends " is a color." --> text is "Green is a color."
  // (2) The user clicks into some other line edit --> loss of focus
  // (3) The user gives focuse again and substitutes "Green" with "Red"
  //     --> text now is "Red is a color."
  // (4) The user hits undo --> text now is "Green is a color."
  //     Without calling advanceMergeId() it would have been "Green", instead.
  connect(m_ui.aliasText, &QLineEdit::editingFinished, m_treeview, &ResourceView::advanceMergeId);
  connect(m_ui.prefixText, &QLineEdit::editingFinished, m_treeview, &ResourceView::advanceMergeId);
  connect(m_ui.languageText, &QLineEdit::editingFinished, m_treeview, &ResourceView::advanceMergeId);

  connect(&m_history, &QUndoStack::canRedoChanged, this, &QrcEditor::updateHistoryControls);
  connect(&m_history, &QUndoStack::canUndoChanged, this, &QrcEditor::updateHistoryControls);

  const auto agg = new Aggregation::Aggregate;
  agg->add(m_treeview);
  agg->add(new Orca::Plugin::Core::ItemViewFind(m_treeview));

  updateHistoryControls();
  updateCurrent();
}

QrcEditor::~QrcEditor() = default;

auto QrcEditor::loaded(bool success) -> void
{
  if (!success)
    return;
  // Set "focus"
  m_treeview->setCurrentIndex(m_treeview->model()->index(0, 0));
  // Expand prefix nodes
  m_treeview->expandAll();
}

auto QrcEditor::refresh() -> void
{
  m_treeview->refresh();
}

// Propagates a change of selection in the tree
// to the alias/prefix/language edit controls
auto QrcEditor::updateCurrent() -> void
{
  const auto isValid = m_treeview->currentIndex().isValid();
  const auto isPrefix = m_treeview->isPrefix(m_treeview->currentIndex()) && isValid;
  const auto isFile = !isPrefix && isValid;

  m_ui.aliasLabel->setEnabled(isFile);
  m_ui.aliasText->setEnabled(isFile);
  m_currentAlias = m_treeview->currentAlias();
  m_ui.aliasText->setText(m_currentAlias);

  m_ui.prefixLabel->setEnabled(isPrefix);
  m_ui.prefixText->setEnabled(isPrefix);
  m_currentPrefix = m_treeview->currentPrefix();
  m_ui.prefixText->setText(m_currentPrefix);

  m_ui.languageLabel->setEnabled(isPrefix);
  m_ui.languageText->setEnabled(isPrefix);
  m_currentLanguage = m_treeview->currentLanguage();
  m_ui.languageText->setText(m_currentLanguage);

  m_ui.addFilesButton->setEnabled(isValid);
  m_ui.removeButton->setEnabled(isValid);
}

auto QrcEditor::updateHistoryControls() -> void
{
  emit undoStackChanged(m_history.canUndo(), m_history.canRedo());
}

// Helper for resolveLocationIssues():
// For code clarity, a context with convenience functions to execute
// the dialogs required for checking the image file paths
// (and keep them around for file dialog execution speed).
// Basically, resolveLocationIssues() checks the paths of the images
// and asks the user to copy them into the resource file location.
// When the user does a multiselection of files, this requires popping
// up the dialog several times in a row.
struct ResolveLocationContext {
  auto execLocationMessageBox(QWidget *parent, const QString &file, bool wantSkipButton) -> QAbstractButton*;
  auto execCopyFileDialog(QWidget *parent, const QDir &dir, const QString &targetPath) -> QString;

  QScopedPointer<QMessageBox> messageBox;
  QScopedPointer<QFileDialog> copyFileDialog;

  QPushButton *copyButton = nullptr;
  QPushButton *skipButton = nullptr;
  QPushButton *abortButton = nullptr;
};

auto ResolveLocationContext::execLocationMessageBox(QWidget *parent, const QString &file, bool wantSkipButton) -> QAbstractButton*
{
  if (messageBox.isNull()) {
    messageBox.reset(new QMessageBox(QMessageBox::Warning, QrcEditor::tr("Invalid file location"), QString(), QMessageBox::NoButton, parent));
    copyButton = messageBox->addButton(QrcEditor::tr("Copy"), QMessageBox::ActionRole);
    abortButton = messageBox->addButton(QrcEditor::tr("Abort"), QMessageBox::RejectRole);
    messageBox->setDefaultButton(copyButton);
  }
  if (wantSkipButton && !skipButton) {
    skipButton = messageBox->addButton(QrcEditor::tr("Skip"), QMessageBox::DestructiveRole);
    messageBox->setEscapeButton(skipButton);
  }
  messageBox->setText(QrcEditor::tr("The file %1 is not in a subdirectory of the resource file. You now have the option to copy this file to a valid location.").arg(QDir::toNativeSeparators(file)));
  messageBox->exec();
  return messageBox->clickedButton();
}

auto ResolveLocationContext::execCopyFileDialog(QWidget *parent, const QDir &dir, const QString &targetPath) -> QString
{
  // Delayed creation of file dialog.
  if (copyFileDialog.isNull()) {
    copyFileDialog.reset(new QFileDialog(parent, QrcEditor::tr("Choose Copy Location")));
    copyFileDialog->setFileMode(QFileDialog::AnyFile);
    copyFileDialog->setAcceptMode(QFileDialog::AcceptSave);
  }
  copyFileDialog->selectFile(targetPath);
  // Repeat until the path entered is no longer above 'dir'
  // (relative is not "../").
  while (true) {
    if (copyFileDialog->exec() != QDialog::Accepted)
      return QString();
    const auto files = copyFileDialog->selectedFiles();
    if (files.isEmpty())
      return QString();
    const auto relPath = dir.relativeFilePath(files.front());
    if (!relPath.startsWith(QLatin1String("../")))
      return files.front();
  }
  return QString();
}

// Helper to copy a file with message boxes
static inline auto copyFile(const QString &file, const QString &copyName, QWidget *parent) -> bool
{
  if (QFile::exists(copyName)) {
    if (!QFile::remove(copyName)) {
      QMessageBox::critical(parent, QrcEditor::tr("Overwriting Failed"), QrcEditor::tr("Could not overwrite file %1.").arg(QDir::toNativeSeparators(copyName)));
      return false;
    }
  }
  if (!QFile::copy(file, copyName)) {
    QMessageBox::critical(parent, QrcEditor::tr("Copying Failed"), QrcEditor::tr("Could not copy the file to %1.").arg(QDir::toNativeSeparators(copyName)));
    return false;
  }
  return true;
}

auto QrcEditor::resolveLocationIssues(QStringList &files) -> void
{
  const auto dir = m_treeview->filePath().toFileInfo().absoluteDir();
  const QString dotdotSlash = QLatin1String("../");
  auto i = 0;
  const int count = files.count();
  const int initialCount = files.count();

  // Find first troublesome file
  for (; i < count; i++) {
    auto const &file = files.at(i);
    const auto relativePath = dir.relativeFilePath(file);
    if (relativePath.startsWith(dotdotSlash))
      break;
  }

  // All paths fine -> no interaction needed
  if (i == count)
    return;

  // Interact with user from now on
  ResolveLocationContext context;
  auto abort = false;
  for (auto it = files.begin(); it != files.end();) {
    // Path fine -> skip file
    auto const &file = *it;
    auto const relativePath = dir.relativeFilePath(file);
    if (!relativePath.startsWith(dotdotSlash))
      continue;
    // Path troublesome and aborted -> remove file
    auto ok = false;
    if (!abort) {
      // Path troublesome -> query user "Do you want copy/abort/skip".
      auto clickedButton = context.execLocationMessageBox(this, file, initialCount > 1);
      if (clickedButton == context.copyButton) {
        const QFileInfo fi(file);
        QFileInfo suggestion;
        QDir tmpTarget(dir.path() + QLatin1Char('/') + QLatin1String("Resources"));
        if (tmpTarget.exists())
          suggestion.setFile(tmpTarget, fi.fileName());
        else
          suggestion.setFile(dir, fi.fileName());
        // Prompt for copy location, copy and replace name.
        const auto copyName = context.execCopyFileDialog(this, dir, suggestion.absoluteFilePath());
        ok = !copyName.isEmpty() && copyFile(file, copyName, this);
        if (ok)
          *it = copyName;
      } else if (clickedButton == context.abortButton) {
        abort = true;
      }
    } // !abort
    if (ok) {
      // Remove files where user canceled or failures occurred.
      ++it;
    } else {
      it = files.erase(it);
    }
  } // for files
}

auto QrcEditor::setResourceDragEnabled(bool e) -> void
{
  m_treeview->setResourceDragEnabled(e);
}

auto QrcEditor::resourceDragEnabled() const -> bool
{
  return m_treeview->resourceDragEnabled();
}

auto QrcEditor::editCurrentItem() -> void
{
  if (m_treeview->selectionModel()->currentIndex().isValid())
    m_treeview->edit(m_treeview->selectionModel()->currentIndex());
}

auto QrcEditor::currentResourcePath() const -> QString
{
  return m_treeview->currentResourcePath();
}

// Slot for change of line edit content 'alias'
auto QrcEditor::onAliasChanged(const QString &alias) -> void
{
  const auto &before = m_currentAlias;
  const auto &after = alias;
  m_treeview->setCurrentAlias(before, after);
  m_currentAlias = alias;
  updateHistoryControls();
}

// Slot for change of line edit content 'prefix'
auto QrcEditor::onPrefixChanged(const QString &prefix) -> void
{
  const auto &before = m_currentPrefix;
  const auto &after = prefix;
  m_treeview->setCurrentPrefix(before, after);
  m_currentPrefix = prefix;
  updateHistoryControls();
}

// Slot for change of line edit content 'language'
auto QrcEditor::onLanguageChanged(const QString &language) -> void
{
  const auto &before = m_currentLanguage;
  const auto &after = language;
  m_treeview->setCurrentLanguage(before, after);
  m_currentLanguage = language;
  updateHistoryControls();
}

// Slot for 'Remove' button
auto QrcEditor::onRemove() -> void
{
  // Find current item, push and execute command
  const auto current = m_treeview->currentIndex();
  auto afterDeletionArrayIndex = current.row();
  auto afterDeletionParent = current.parent();
  m_treeview->findSamePlacePostDeletionModelIndex(afterDeletionArrayIndex, afterDeletionParent);
  QUndoCommand *const removeCommand = new RemoveEntryCommand(m_treeview, current);
  m_history.push(removeCommand);
  const auto afterDeletionModelIndex = m_treeview->model()->index(afterDeletionArrayIndex, 0, afterDeletionParent);
  m_treeview->setCurrentIndex(afterDeletionModelIndex);
  updateHistoryControls();
}

// Slot for 'Remove missing files' button
auto QrcEditor::onRemoveNonExisting() -> void
{
  const auto toRemove = m_treeview->nonExistingFiles();

  QUndoCommand *const removeCommand = new RemoveMultipleEntryCommand(m_treeview, toRemove);
  m_history.push(removeCommand);
  updateHistoryControls();
}

// Slot for 'Add File' button
auto QrcEditor::onAddFiles() -> void
{
  auto const current = m_treeview->currentIndex();
  int const currentIsPrefixNode = m_treeview->isPrefix(current);
  auto const prefixArrayIndex = currentIsPrefixNode ? current.row() : m_treeview->model()->parent(current).row();
  auto const cursorFileArrayIndex = currentIsPrefixNode ? 0 : current.row();
  auto fileNames = m_treeview->fileNamesToAdd();
  fileNames = m_treeview->existingFilesSubtracted(prefixArrayIndex, fileNames);
  resolveLocationIssues(fileNames);
  if (fileNames.isEmpty())
    return;
  QUndoCommand *const addFilesCommand = new AddFilesCommand(m_treeview, prefixArrayIndex, cursorFileArrayIndex, fileNames);
  m_history.push(addFilesCommand);
  updateHistoryControls();
}

// Slot for 'Add Prefix' button
auto QrcEditor::onAddPrefix() -> void
{
  QUndoCommand *const addEmptyPrefixCommand = new AddEmptyPrefixCommand(m_treeview);
  m_history.push(addEmptyPrefixCommand);
  updateHistoryControls();
  m_ui.prefixText->selectAll();
  m_ui.prefixText->setFocus();
}

// Slot for 'Undo' button
auto QrcEditor::onUndo() -> void
{
  m_history.undo();
  updateCurrent();
  updateHistoryControls();
}

// Slot for 'Redo' button
auto QrcEditor::onRedo() -> void
{
  m_history.redo();
  updateCurrent();
  updateHistoryControls();
}
