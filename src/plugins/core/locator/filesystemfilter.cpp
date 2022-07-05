// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "filesystemfilter.h"
#include "basefilefilter.h"
#include "locatorwidget.h"

#include <core/coreconstants.h>
#include <core/documentmanager.h>
#include <core/editormanager/editormanager.h>
#include <core/icore.h>
#include <core/vcsmanager.h>

#include <utils/checkablemessagebox.h>
#include <utils/fileutils.h>
#include <utils/link.h>

#include <QDir>
#include <QJsonObject>
#include <QPushButton>
#include <QRegularExpression>

using namespace Utils;

namespace Core {
namespace Internal {

auto FileSystemFilter::matchLevelFor(const QRegularExpressionMatch &match, const QString &match_text) -> MatchLevel
{
  const int consecutive_pos = match.capturedStart(1);

  if (consecutive_pos == 0)
    return MatchLevel::Best;

  if (consecutive_pos > 0) {
    if (const auto prev_char = match_text.at(consecutive_pos - 1); prev_char == '_' || prev_char == '.')
      return MatchLevel::Better;
  }

  if (match.capturedStart() == 0)
    return MatchLevel::Good;

  return MatchLevel::Normal;
}

FileSystemFilter::FileSystemFilter()
{
  setId("Files in file system");
  setDisplayName(tr("Files in File System"));
  setDescription(tr("Opens a file given by a relative path to the current document, or absolute " "path. \"~\" refers to your home directory. You have the option to create a " "file if it does not exist yet."));
  setDefaultShortcutString("f");
  setDefaultIncludedByDefault(false);
}

auto FileSystemFilter::prepareSearch(const QString &entry) -> void
{
  Q_UNUSED(entry)
  m_current_document_directory = DocumentManager::fileDialogInitialDirectory().toString();
  m_current_include_hidden = m_include_hidden;
}

auto FileSystemFilter::matchesFor(QFutureInterface<LocatorFilterEntry> &future, const QString &entry) -> QList<LocatorFilterEntry>
{
  QList<LocatorFilterEntry> entries[static_cast<int>(MatchLevel::Count)];
  const QFileInfo entry_info(entry);
  const auto entry_file_name = entry_info.fileName();
  auto directory = entry_info.path();

  if (entry_info.isRelative()) {
    if (entry_info.filePath().startsWith("~/"))
      directory.replace(0, 1, QDir::homePath());
    else if (!m_current_document_directory.isEmpty())
      directory.prepend(m_current_document_directory + "/");
  }

  const QDir dir_info(directory);
  auto dir_filter = QDir::Dirs | QDir::Drives | QDir::NoDot | QDir::NoDotDot;
  QDir::Filters file_filter = QDir::Files;

  if (m_current_include_hidden) {
    dir_filter |= QDir::Hidden;
    file_filter |= QDir::Hidden;
  }

  // use only 'name' for case sensitivity decision, because we need to make the path
  // match the case on the file system for case-sensitive file systems
  const auto case_sensitivity_ = caseSensitivity(entry_file_name);
  const auto dirs = QStringList("..") + dir_info.entryList(dir_filter, QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);
  const auto files = dir_info.entryList(file_filter, QDir::Name | QDir::IgnoreCase | QDir::LocaleAware);

  auto reg_exp = createRegExp(entry_file_name, case_sensitivity_);
  if (!reg_exp.isValid())
    return {};

  for (const auto &dir : dirs) {
    if (future.isCanceled())
      break;

    if (const auto match = reg_exp.match(dir); match.hasMatch()) {
      const auto level = matchLevelFor(match, dir);
      const auto full_path = dir_info.filePath(dir);
      LocatorFilterEntry filter_entry(this, dir, QVariant());
      filter_entry.file_path = FilePath::fromString(full_path);
      filter_entry.highlight_info = highlightInfo(match);
      entries[static_cast<int>(level)].append(filter_entry);
    }
  }

  // file names can match with +linenumber or :linenumber
  QString postfix;
  auto link = Link::fromString(entry_file_name, true, &postfix);
  reg_exp = createRegExp(link.targetFilePath.toString(), case_sensitivity_);

  if (!reg_exp.isValid())
    return {};

  for (const auto &file : files) {
    if (future.isCanceled())
      break;

    if (const auto match = reg_exp.match(file); match.hasMatch()) {
      const auto level = matchLevelFor(match, file);
      const auto full_path = dir_info.filePath(file);
      LocatorFilterEntry filter_entry(this, file, QString(full_path + postfix));
      filter_entry.file_path = FilePath::fromString(full_path);
      filter_entry.highlight_info = highlightInfo(match);
      entries[static_cast<int>(level)].append(filter_entry);
    }
  }

  // "create and open" functionality
  const auto full_file_path = dir_info.filePath(entry_file_name);
  if (const auto contains_wildcard = entry.contains('?') || entry.contains('*'); !contains_wildcard && !QFileInfo::exists(full_file_path) && dir_info.exists()) {
    LocatorFilterEntry create_and_open(this, tr("Create and Open \"%1\"").arg(entry), full_file_path);
    create_and_open.file_path = FilePath::fromString(full_file_path);
    create_and_open.extra_info = FilePath::fromString(dir_info.absolutePath()).shortNativePath();
    entries[static_cast<int>(MatchLevel::Normal)].append(create_and_open);
  }

  return std::accumulate(std::begin(entries), std::end(entries), QList<LocatorFilterEntry>());
}

constexpr char k_always_create[] = "Locator/FileSystemFilter/AlwaysCreate";

auto FileSystemFilter::accept(const LocatorFilterEntry &selection, QString *new_text, int *selection_start, int *selection_length) const -> void
{
  Q_UNUSED(selection_length)

  if (selection.file_path.isDir()) {
    const QString value = shortcutString() + ' ' + selection.file_path.absoluteFilePath().cleanPath().pathAppended("/").toUserOutput();
    *new_text = value;
    *selection_start = static_cast<int>(value.length());
  } else {
    // Don't block locator filter execution with dialog
    QMetaObject::invokeMethod(EditorManager::instance(), [selection] {
      const auto target_file = FilePath::fromVariant(selection.internal_data);
      if (!selection.file_path.exists()) {
        if (CheckableMessageBox::shouldAskAgain(ICore::settings(), k_always_create)) {
          CheckableMessageBox message_box(ICore::dialogParent());
          message_box.setWindowTitle(tr("Create File"));
          message_box.setIcon(QMessageBox::Question);
          message_box.setText(tr("Create \"%1\"?").arg(target_file.shortNativePath()));
          message_box.setCheckBoxVisible(true);
          message_box.setCheckBoxText(tr("Always create"));
          message_box.setChecked(false);
          message_box.setStandardButtons(QDialogButtonBox::Cancel);
          const auto create_button = message_box.addButton(tr("Create"), QDialogButtonBox::AcceptRole);
          message_box.setDefaultButton(QDialogButtonBox::Cancel);
          message_box.exec();

          if (message_box.clickedButton() != create_button)
            return;

          if (message_box.isChecked())
            CheckableMessageBox::doNotAskAgain(ICore::settings(), k_always_create);
        }
        QFile file(target_file.toString());
        file.open(QFile::WriteOnly);
        file.close();
        VcsManager::promptToAdd(target_file.absolutePath(), {target_file});
      }
      BaseFileFilter::openEditorAt(selection);
    }, Qt::QueuedConnection);
  }
}

auto FileSystemFilter::openConfigDialog(QWidget *parent, bool &needs_refresh) -> bool
{
  Q_UNUSED(needs_refresh)

  Ui::FileSystemFilterOptions ui;
  QDialog dialog(parent);
  ui.setupUi(&dialog);
  dialog.setWindowTitle(msgConfigureDialogTitle());
  ui.prefixLabel->setText(msgPrefixLabel());
  ui.prefixLabel->setToolTip(msgPrefixToolTip());
  ui.includeByDefault->setText(msgIncludeByDefault());
  ui.includeByDefault->setToolTip(msgIncludeByDefaultToolTip());
  ui.hiddenFilesFlag->setChecked(m_include_hidden);
  ui.includeByDefault->setChecked(isIncludedByDefault());
  ui.shortcutEdit->setText(shortcutString());

  if (dialog.exec() == QDialog::Accepted) {
    m_include_hidden = ui.hiddenFilesFlag->isChecked();
    setShortcutString(ui.shortcutEdit->text().trimmed());
    setIncludedByDefault(ui.includeByDefault->isChecked());
    return true;
  }

  return false;
}

constexpr char k_include_hidden_key[] = "includeHidden";

auto FileSystemFilter::saveState(QJsonObject &object) const -> void
{
  if (m_include_hidden != k_include_hidden_default)
    object.insert(k_include_hidden_key, m_include_hidden);
}

auto FileSystemFilter::restoreState(const QJsonObject &object) -> void
{
  m_current_include_hidden = object.value(k_include_hidden_key).toBool(k_include_hidden_default);
}

auto FileSystemFilter::restoreState(const QByteArray &state) -> void
{
  if (isOldSetting(state)) {
    // TODO read old settings, remove some time after Qt Creator 4.15
    QDataStream in(state);
    in >> m_include_hidden;

    // An attempt to prevent setting this on old configuration
    if (!in.atEnd()) {
      QString shortcut;
      bool defaultFilter;
      in >> shortcut;
      in >> defaultFilter;
      setShortcutString(shortcut);
      setIncludedByDefault(defaultFilter);
    }
  } else {
    ILocatorFilter::restoreState(state);
  }
}

} // Internal
} // Core
