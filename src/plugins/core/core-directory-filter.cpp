// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-directory-filter.hpp"

#include "core-constants.hpp"
#include "core-locator.hpp"

#include "ui_core-directory-filter.h"

#include <utils/algorithm.hpp>
#include <utils/filesearch.hpp>

#include <QJsonArray>
#include <QJsonObject>

using namespace Utils;

namespace Orca::Plugin::Core {

/*!
    \class Orca::Plugin::Core::DirectoryFilter
    \inmodule Orca
    \internal
*/

constexpr char k_display_name_key[] = "displayName";
constexpr char k_directories_key[] = "directories";
constexpr char k_filters_key[] = "filters";
constexpr char k_files_key[] = "files";
constexpr char k_exclusion_filters_key[] = "exclusionFilters";

const QStringList k_filters_default = {"*.hpp", "*.cpp", "*.ui", "*.qrc"};
const QStringList k_exclusion_filters_default = {"*/.git/*", "*/.cvs/*", "*/.svn/*"};

static auto defaultDisplayName() -> QString
{
  return DirectoryFilter::tr("Generic Directory Filter");
}

DirectoryFilter::DirectoryFilter(const Id id) : m_filters(k_filters_default), m_exclusion_filters(k_exclusion_filters_default)
{
  setId(id);
  setDefaultIncludedByDefault(true);
  setDisplayName(defaultDisplayName());
  setDescription(tr("Matches all files from a custom set of directories. Append \"+<number>\" or " "\":<number>\" to jump to the given line number. Append another " "\"+<number>\" or \":<number>\" to jump to the column number as well."));
}

auto DirectoryFilter::saveState(QJsonObject &object) const -> void
{
  QMutexLocker locker(&m_lock); // m_files is modified in other thread

  if (displayName() != defaultDisplayName())
    object.insert(k_display_name_key, displayName());

  if (!m_directories.isEmpty())
    object.insert(k_directories_key, QJsonArray::fromStringList(m_directories));

  if (m_filters != k_filters_default)
    object.insert(k_filters_key, QJsonArray::fromStringList(m_filters));

  if (!m_files.isEmpty())
    object.insert(k_files_key, QJsonArray::fromStringList(transform(m_files, &FilePath::toString)));

  if (m_exclusion_filters != k_exclusion_filters_default)
    object.insert(k_exclusion_filters_key, QJsonArray::fromStringList(m_exclusion_filters));
}

static auto toStringList(const QJsonArray &array) -> QStringList
{
  return transform(array.toVariantList(), &QVariant::toString);
}

auto DirectoryFilter::restoreState(const QJsonObject &object) -> void
{
  QMutexLocker locker(&m_lock);
  setDisplayName(object.value(k_display_name_key).toString(defaultDisplayName()));
  m_directories = toStringList(object.value(k_directories_key).toArray());
  m_filters = toStringList(object.value(k_filters_key).toArray(QJsonArray::fromStringList(k_filters_default)));
  m_files = transform(toStringList(object.value(k_files_key).toArray()), &FilePath::fromString);
  m_exclusion_filters = toStringList(object.value(k_exclusion_filters_key).toArray(QJsonArray::fromStringList(k_exclusion_filters_default)));
}

auto DirectoryFilter::restoreState(const QByteArray &state) -> void
{
  if (isOldSetting(state)) {
    // TODO read old settings, remove some time after Qt Creator 4.15
    QMutexLocker locker(&m_lock);
    QString name;
    QStringList directories;
    QString shortcut;
    bool default_filter;
    QStringList files;
    QDataStream in(state);

    in >> name;
    in >> directories;
    in >> m_filters;
    in >> shortcut;
    in >> default_filter;
    in >> files;

    m_files = transform(files, &FilePath::fromString);

    if (!in.atEnd()) // Qt Creator 4.3 and later
      in >> m_exclusion_filters;
    else
      m_exclusion_filters.clear();

    if (m_is_custom_filter)
      m_directories = directories;

    setDisplayName(name);
    setShortcutString(shortcut);
    setIncludedByDefault(default_filter);

    locker.unlock();
  } else {
    ILocatorFilter::restoreState(state);
  }
  updateFileIterator();
}

auto DirectoryFilter::openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool
{
  if (!m_ui) {
    m_ui = new Ui::DirectoryFilterOptions;
  }

  auto success = false;
  QDialog dialog(parent);

  m_dialog = &dialog;
  m_ui->setupUi(&dialog);

  dialog.setWindowTitle(msgConfigureDialogTitle());

  m_ui->prefixLabel->setText(msgPrefixLabel());
  m_ui->prefixLabel->setToolTip(msgPrefixToolTip());
  m_ui->defaultFlag->setText(msgIncludeByDefault());
  m_ui->defaultFlag->setToolTip(msgIncludeByDefaultToolTip());
  m_ui->nameEdit->setText(displayName());
  m_ui->nameEdit->selectAll();

  connect(m_ui->addButton, &QPushButton::clicked, this, &DirectoryFilter::handleAddDirectory, Qt::DirectConnection);
  connect(m_ui->editButton, &QPushButton::clicked, this, &DirectoryFilter::handleEditDirectory, Qt::DirectConnection);
  connect(m_ui->removeButton, &QPushButton::clicked, this, &DirectoryFilter::handleRemoveDirectory, Qt::DirectConnection);
  connect(m_ui->directoryList, &QListWidget::itemSelectionChanged, this, &DirectoryFilter::updateOptionButtons, Qt::DirectConnection);

  m_ui->directoryList->clear();

  // Note: assuming we only change m_directories in the Gui thread,
  // we don't need to protect it here with mutex
  m_ui->directoryList->addItems(m_directories);
  m_ui->nameLabel->setVisible(m_is_custom_filter);
  m_ui->nameEdit->setVisible(m_is_custom_filter);
  m_ui->directoryLabel->setVisible(m_is_custom_filter);
  m_ui->directoryList->setVisible(m_is_custom_filter);
  m_ui->addButton->setVisible(m_is_custom_filter);
  m_ui->editButton->setVisible(m_is_custom_filter);
  m_ui->removeButton->setVisible(m_is_custom_filter);
  m_ui->filePatternLabel->setText(msgFilePatternLabel());
  m_ui->filePatternLabel->setBuddy(m_ui->filePattern);
  m_ui->filePattern->setToolTip(msgFilePatternToolTip());

  // Note: assuming we only change m_filters in the Gui thread,
  // we don't need to protect it here with mutex
  m_ui->filePattern->setText(transform(m_filters, &QDir::toNativeSeparators).join(','));
  m_ui->exclusionPatternLabel->setText(msgExclusionPatternLabel());
  m_ui->exclusionPatternLabel->setBuddy(m_ui->exclusionPattern);
  m_ui->exclusionPattern->setToolTip(msgFilePatternToolTip());

  // Note: assuming we only change m_exclusionFilters in the Gui thread,
  // we don't need to protect it here with mutex
  m_ui->exclusionPattern->setText(transform(m_exclusion_filters, &QDir::toNativeSeparators).join(','));
  m_ui->shortcutEdit->setText(shortcutString());
  m_ui->defaultFlag->setChecked(isIncludedByDefault());

  updateOptionButtons();
  dialog.adjustSize();

  if (dialog.exec() == QDialog::Accepted) {
    QMutexLocker locker(&m_lock);

    auto directories_changed = false;
    const auto old_directories = m_directories;
    const auto old_filters = m_filters;
    const auto old_exclusion_filters = m_exclusion_filters;

    setDisplayName(m_ui->nameEdit->text().trimmed());
    m_directories.clear();

    const auto old_count = old_directories.count();
    const auto new_count = m_ui->directoryList->count();

    if (old_count != new_count)
      directories_changed = true;

    for (auto i = 0; i < new_count; ++i) {
      m_directories.append(m_ui->directoryList->item(i)->text());
      if (!directories_changed && m_directories.at(i) != old_directories.at(i))
        directories_changed = true;
    }

    m_filters = splitFilterUiText(m_ui->filePattern->text());
    m_exclusion_filters = splitFilterUiText(m_ui->exclusionPattern->text());
    setShortcutString(m_ui->shortcutEdit->text().trimmed());
    setIncludedByDefault(m_ui->defaultFlag->isChecked());
    needs_refresh = directories_changed || old_filters != m_filters || old_exclusion_filters != m_exclusion_filters;
    success = true;
  }
  return success;
}

auto DirectoryFilter::handleAddDirectory() const -> void
{
  if (const auto dir = FileUtils::getExistingDirectory(m_dialog, tr("Select Directory")); !dir.isEmpty())
    m_ui->directoryList->addItem(dir.toUserOutput());
}

auto DirectoryFilter::handleEditDirectory() const -> void
{
  if (m_ui->directoryList->selectedItems().count() < 1)
    return;

  const auto current_item = m_ui->directoryList->selectedItems().at(0);

  if (const auto dir = FileUtils::getExistingDirectory(m_dialog, tr("Select Directory"), FilePath::fromUserInput(current_item->text())); !dir.isEmpty())
    current_item->setText(dir.toUserOutput());
}

auto DirectoryFilter::handleRemoveDirectory() const -> void
{
  if (m_ui->directoryList->selectedItems().count() < 1)
    return;
  const auto current_item = m_ui->directoryList->selectedItems().at(0);
  delete m_ui->directoryList->takeItem(m_ui->directoryList->row(current_item));
}

auto DirectoryFilter::updateOptionButtons() const -> void
{
  const auto haveSelectedItem = !m_ui->directoryList->selectedItems().isEmpty();
  m_ui->editButton->setEnabled(haveSelectedItem);
  m_ui->removeButton->setEnabled(haveSelectedItem);
}

auto DirectoryFilter::updateFileIterator() const -> void
{
  QMutexLocker locker(&m_lock);
  setFileIterator(new ListIterator(m_files));
}

auto DirectoryFilter::refresh(QFutureInterface<void> &future) -> void
{
  {
    QMutexLocker locker(&m_lock);

    if (m_directories.isEmpty()) {
      m_files.clear();
      QMetaObject::invokeMethod(this, &DirectoryFilter::updateFileIterator, Qt::QueuedConnection);
      future.setProgressRange(0, 1);
      future.setProgressValueAndText(1, tr("%1 filter update: 0 files").arg(displayName()));
      return;
    }
  } // release lock

  const auto directories = m_directories;
  const auto filters = m_filters;
  const auto exclusion_filters = m_exclusion_filters;

  const SubDirFileIterator sub_dir_iterator(directories, filters, exclusion_filters);
  future.setProgressRange(0, sub_dir_iterator.maxProgress());
  FilePaths files_found;

  const auto end = sub_dir_iterator.end();
  for (auto it = sub_dir_iterator.begin(); it != end; ++it) {
    if (future.isCanceled())
      break;
    files_found << FilePath::fromString((*it).filePath);
    if (future.isProgressUpdateNeeded() || future.progressValue() == 0 /*workaround for regression in Qt*/) {
      future.setProgressValueAndText(sub_dir_iterator.currentProgress(), tr("%1 filter update: %n files", nullptr, static_cast<int>(files_found.size())).arg(displayName()));
    }
  }

  if (!future.isCanceled()) {
    QMutexLocker locker(&m_lock);
    m_files = files_found;
    QMetaObject::invokeMethod(this, &DirectoryFilter::updateFileIterator, Qt::QueuedConnection);
    future.setProgressValue(sub_dir_iterator.maxProgress());
  } else {
    future.setProgressValueAndText(sub_dir_iterator.currentProgress(), tr("%1 filter update: canceled").arg(displayName()));
  }
}

auto DirectoryFilter::setIsCustomFilter(const bool value) -> void
{
  m_is_custom_filter = value;
}

auto DirectoryFilter::setDirectories(const QStringList &directories) -> void
{
  if (directories == m_directories)
    return;

  QMutexLocker locker(&m_lock);
  m_directories = directories;
  Locator::instance()->refresh({this});
}

auto DirectoryFilter::addDirectory(const QString &directory) -> void
{
  setDirectories(m_directories + QStringList(directory));
}

auto DirectoryFilter::removeDirectory(const QString &directory) -> void
{
  auto directories = m_directories;
  directories.removeOne(directory);
  setDirectories(directories);
}

auto DirectoryFilter::directories() const -> QStringList
{
  return m_directories;
}

auto DirectoryFilter::setFilters(const QStringList &filters) -> void
{
  QMutexLocker locker(&m_lock);
  m_filters = filters;
}

auto DirectoryFilter::setExclusionFilters(const QStringList &exclusion_filters) -> void
{
  QMutexLocker locker(&m_lock);
  m_exclusion_filters = exclusion_filters;
}

} // namespace Orca::Plugin::Core
