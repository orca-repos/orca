// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "fileutils.h"

#include <core/coreconstants.h>
#include <core/documentmanager.h>
#include <core/foldernavigationwidget.h>
#include <core/icore.h>
#include <core/iversioncontrol.h>
#include <core/messagemanager.h>
#include <core/navigationwidget.h>
#include <core/vcsmanager.h>

#include <utils/commandline.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>
#include <utils/textfileformat.h>
#include <utils/unixutils.h>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextStream>
#include <QTextCodec>
#include <QWidget>

#ifdef Q_OS_WIN

#include <Windows.h>
#include <cstring>

#endif

using namespace Utils;

namespace Core {

// Show error with option to open settings.
static auto showGraphicalShellError(QWidget *parent, const QString &app, const QString &error) -> void
{
  const auto title = QApplication::translate("Core::Internal", "Launching a file browser failed");
  const auto msg = QApplication::translate("Core::Internal", "Unable to start the file manager:\n\n%1\n\n").arg(app);

  QMessageBox mbox(QMessageBox::Warning, title, msg, QMessageBox::Close, parent);

  if (!error.isEmpty())
    mbox.setDetailedText(QApplication::translate("Core::Internal", "\"%1\" returned the following error:\n\n%2").arg(app, error));

  const QAbstractButton *settings_button = mbox.addButton(ICore::msgShowOptionsDialog(), QMessageBox::ActionRole);
  mbox.exec();

  if (mbox.clickedButton() == settings_button)
    ICore::showOptionsDialog(Constants::SETTINGS_ID_INTERFACE, parent);
}

auto FileUtils::showInGraphicalShell(QWidget *parent, const FilePath &path) -> void
{
  const auto file_info = path.toFileInfo();

  // Mac, Windows support folder or file.
  if constexpr (HostOsInfo::isWindowsHost()) {
    const auto explorer = Environment::systemEnvironment().searchInPath(QLatin1String("explorer.exe"));
    if (explorer.isEmpty()) {
      QMessageBox::warning(parent, QApplication::translate("Core::Internal", "Launching Windows Explorer Failed"), QApplication::translate("Core::Internal", "Could not find explorer.exe in path to launch Windows Explorer."));
      return;
    }

    QStringList param;

    if (!path.isDir())
      param += QLatin1String("/select,");

    param += QDir::toNativeSeparators(file_info.canonicalFilePath());
    QtcProcess::startDetached({explorer, param});
  } else if constexpr (HostOsInfo::isMacHost()) {
    QtcProcess::startDetached({"/usr/bin/open", {"-R", file_info.canonicalFilePath()}});
  } else {
    // we cannot select a file here, because no file browser really supports it...
    const auto folder = file_info.isDir() ? file_info.absoluteFilePath() : file_info.filePath();
    const auto app = UnixUtils::fileBrowser(ICore::settings());
    auto browser_args = ProcessArgs::splitArgs(UnixUtils::substituteFileBrowserParameters(app, folder));

    QString error;

    if (browser_args.isEmpty()) {
      error = QApplication::translate("Core::Internal", "The command for file browser is not set.");
    } else {
      QProcess browser_proc;
      browser_proc.setProgram(browser_args.takeFirst());
      browser_proc.setArguments(browser_args);

      const auto success = browser_proc.startDetached();

      error = QString::fromLocal8Bit(browser_proc.readAllStandardError());
      if (!success && error.isEmpty())
        error = QApplication::translate("Core::Internal", "Error while starting file browser.");
    }
    if (!error.isEmpty())
      showGraphicalShellError(parent, app, error);
  }
}

auto FileUtils::showInFileSystemView(const FilePath &path) -> void
{
  const auto widget = NavigationWidget::activateSubWidget(FolderNavigationWidgetFactory::instance()->id(), Side::Left);
  if (auto *nav_widget = qobject_cast<FolderNavigationWidget*>(widget))
    nav_widget->syncWithFilePath(path);
}

static auto startTerminalEmulator(const QString &working_dir, const Environment &env) -> void
{
  #ifdef Q_OS_WIN
  STARTUPINFO si;
  ZeroMemory(&si, sizeof si);
  si.cb = sizeof si;

  PROCESS_INFORMATION pinfo;
  ZeroMemory(&pinfo, sizeof pinfo);

  static const auto quote_win_command = [](const QString &program) {
    constexpr QChar double_quote = QLatin1Char('"');

    // add the program as the first arg ... it works better
    auto program_name = program;
    program_name.replace(QLatin1Char('/'), QLatin1Char('\\'));
    if (!program_name.startsWith(double_quote) && !program_name.endsWith(double_quote) && program_name.contains(QLatin1Char(' '))) {
      program_name.prepend(double_quote);
      program_name.append(double_quote);
    }
    return program_name;
  };

  // cmdLine is assumed to be detached -
  // https://blogs.msdn.microsoft.com/oldnewthing/20090601-00/?p=18083
  const auto cmd_line = quote_win_command(QString::fromLocal8Bit(qgetenv("COMSPEC")));
  const QString total_environment = env.toStringList().join(QChar(QChar::Null)) + QChar(QChar::Null);
  const LPVOID env_ptr = env != Environment::systemEnvironment() ? (WCHAR*)total_environment.utf16() : nullptr;

  if (const bool success = CreateProcessW(nullptr, (WCHAR*)cmd_line.utf16(), 0, 0, FALSE, CREATE_NEW_CONSOLE | CREATE_UNICODE_ENVIRONMENT, env_ptr, working_dir.isEmpty() ? 0 : (WCHAR*)working_dir.utf16(), &si, &pinfo)) {
    CloseHandle(pinfo.hThread);
    CloseHandle(pinfo.hProcess);
  }

  #else
  const TerminalCommand term = TerminalCommand::terminalEmulator();
  QProcess process;
  process.setProgram(term.command);
  process.setArguments(ProcessArgs::splitArgs(term.openArgs));
  process.setProcessEnvironment(env.toProcessEnvironment());
  process.setWorkingDirectory(workingDir);
  process.startDetached();
  #endif
}

auto FileUtils::openTerminal(const FilePath &path) -> void
{
  openTerminal(path, Environment::systemEnvironment());
}

auto FileUtils::openTerminal(const FilePath &path, const Environment &env) -> void
{
  const auto file_info = path.toFileInfo();
  const auto working_dir = QDir::toNativeSeparators(file_info.isDir() ? file_info.absoluteFilePath() : file_info.absolutePath());
  startTerminalEmulator(working_dir, env);
}

auto FileUtils::msgFindInDirectory() -> QString
{
  return QApplication::translate("Core::Internal", "Find in This Directory...");
}

auto FileUtils::msgFileSystemAction() -> QString
{
  return QApplication::translate("Core::Internal", "Show in File System View");
}

auto FileUtils::msgGraphicalShellAction() -> QString
{
  if constexpr (HostOsInfo::isWindowsHost())
    return QApplication::translate("Core::Internal", "Show in Explorer");
  else if constexpr (HostOsInfo::isMacHost())
    return QApplication::translate("Core::Internal", "Show in Finder");
  else
    return QApplication::translate("Core::Internal", "Show Containing Folder");
}

auto FileUtils::msgTerminalHereAction() -> QString
{
  if constexpr (HostOsInfo::isWindowsHost())
    return QApplication::translate("Core::Internal", "Open Command Prompt Here");
  else
    return QApplication::translate("Core::Internal", "Open Terminal Here");
}

auto FileUtils::msgTerminalWithAction() -> QString
{
  if constexpr (HostOsInfo::isWindowsHost())
    return QApplication::translate("Core::Internal", "Open Command Prompt With", "Opens a submenu for choosing an environment, such as \"Run Environment\"");
  else
    return QApplication::translate("Core::Internal", "Open Terminal With", "Opens a submenu for choosing an environment, such as \"Run Environment\"");
}

auto FileUtils::removeFiles(const FilePaths &file_paths, const bool delete_from_fs) -> void
{
  // remove from version control
  VcsManager::promptToDelete(file_paths);

  if (!delete_from_fs)
    return;

  // remove from file system
  for (const auto &fp : file_paths) {
    QFile file(fp.toString());
    if (!file.exists()) // could have been deleted by vc
      continue;
    if (!file.remove()) {
      MessageManager::writeDisrupting(QCoreApplication::translate("Core::Internal", "Failed to remove file \"%1\".").arg(fp.toUserOutput()));
    }
  }
}

auto FileUtils::renameFile(const FilePath &org_file_path, const FilePath &new_file_path, const HandleIncludeGuards handle_guards) -> bool
{
  if (org_file_path == new_file_path)
    return false;

  const auto dir = org_file_path.absolutePath();
  const auto vc = VcsManager::findVersionControlForDirectory(dir);
  auto result = false;

  if (vc && vc->supportsOperation(IVersionControl::MoveOperation))
    result = vc->vcsMove(org_file_path, new_file_path);

  if (!result) // The moving via vcs failed or the vcs does not support moving, fall back
    result = org_file_path.renameFile(new_file_path);

  if (result) {
    // yeah we moved, tell the filemanager about it
    DocumentManager::renamedFile(org_file_path, new_file_path);
  }

  if (result)
    updateHeaderFileGuardIfApplicable(org_file_path, new_file_path, handle_guards);
  return result;
}

auto FileUtils::updateHeaderFileGuardIfApplicable(const FilePath &old_file_path, const FilePath &new_file_path, const HandleIncludeGuards handle_guards) -> void
{
  if (handle_guards == HandleIncludeGuards::No)
    return;

  if (updateHeaderFileGuardAfterRename(new_file_path.toString(), old_file_path.baseName()))
    return;

  MessageManager::writeDisrupting(QCoreApplication::translate("Core::FileUtils", "Failed to rename the include guard in file \"%1\".").arg(new_file_path.toUserOutput()));
}

auto FileUtils::updateHeaderFileGuardAfterRename(const QString &header_path, const QString &old_header_base_name) -> bool
{
  auto ret = true;

  QFile header_file(header_path);
  if (!header_file.open(QFile::ReadOnly | QFile::Text))
    return false;

  const QRegularExpression guard_condition_reg_exp(QString("(#ifndef)(\\s*)(_*)%1_H(_*)(\\s*)").arg(old_header_base_name.toUpper()));
  QRegularExpression guard_define_regexp, guard_close_reg_exp;
  QRegularExpressionMatch guard_condition_match, guard_define_match, guard_close_match;
  auto guard_start_line = -1;
  auto guard_close_line = -1;

  auto data = header_file.readAll();
  header_file.close();

  const auto header_file_text_format = TextFileFormat::detect(data);
  QTextStream in_stream(&data);
  auto line_counter = 0;
  QString line;

  while (!in_stream.atEnd()) {
    // use trimmed line to get rid from the maunder leading spaces
    in_stream.readLineInto(&line);
    line = line.trimmed();
    if (line == QStringLiteral("#pragma once")) {
      // if pragma based guard found skip reading the whole file
      break;
    }
    if (guard_start_line == -1) {
      // we are still looking for the guard condition
      guard_condition_match = guard_condition_reg_exp.match(line);
      if (guard_condition_match.hasMatch()) {
        guard_define_regexp.setPattern(QString("(#define\\s*%1)%2(_H%3\\s*)").arg(guard_condition_match.captured(3), old_header_base_name.toUpper(), guard_condition_match.captured(4)));
        // read the next line for the guard define
        line = in_stream.readLine();
        if (!in_stream.atEnd()) {
          guard_define_match = guard_define_regexp.match(line);
          if (guard_define_match.hasMatch()) {
            // if a proper guard define present in the next line store the line number
            guard_close_reg_exp.setPattern(QString(R"((#endif\s*)(\/\/|\/\*)(\s*%1)%2(_H%3\s*)((\*\/)?))").arg(guard_condition_match.captured(3), old_header_base_name.toUpper(), guard_condition_match.captured(4)));
            guard_start_line = line_counter;
            line_counter++;
          }
        } else {
          // it the line after the guard opening is not something what we expect
          // then skip the whole guard replacing process
          break;
        }
      }
    } else {
      // guard start found looking for the guard closing endif
      guard_close_match = guard_close_reg_exp.match(line);
      if (guard_close_match.hasMatch()) {
        guard_close_line = line_counter;
        break;
      }
    }
    line_counter++;
  }

  if (guard_start_line != -1) {
    // At least the guard have been found ->
    // copy the contents of the header to a temporary file with the updated guard lines
    in_stream.seek(0);

    const QFileInfo fi(header_file);
    const auto guard_condition = QString("#ifndef%1%2%3_H%4%5").arg(guard_condition_match.captured(2), guard_condition_match.captured(3), fi.baseName().toUpper(), guard_condition_match.captured(4), guard_condition_match.captured(5));
    const auto guard_define = QString("%1%2%3").arg(guard_define_match.captured(1), fi.baseName().toUpper(), guard_define_match.captured(2));
    const auto guard_close = QString("%1%2%3%4%5%6").arg(guard_close_match.captured(1), guard_close_match.captured(2), guard_close_match.captured(3), fi.baseName().toUpper(), guard_close_match.captured(4), guard_close_match.captured(5));

    if (QFile tmp_header(header_path + ".tmp"); tmp_header.open(QFile::WriteOnly)) {
      const auto line_end = header_file_text_format.lineTerminationMode == TextFileFormat::LFLineTerminator ? QStringLiteral("\n") : QStringLiteral("\r\n");
      // write into temporary string,
      // after that write with codec into file (QTextStream::setCodec is gone in Qt 6)
      QString out_string;
      QTextStream out_stream(&out_string);
      auto line_counter = 0;
      while (!in_stream.atEnd()) {
        in_stream.readLineInto(&line);
        if (line_counter == guard_start_line) {
          out_stream << guard_condition << line_end;
          out_stream << guard_define << line_end;
          in_stream.readLine();
          line_counter++;
        } else if (line_counter == guard_close_line) {
          out_stream << guard_close << line_end;
        } else {
          out_stream << line << line_end;
        }
        line_counter++;
      }
      const auto text_codec = header_file_text_format.codec == nullptr ? QTextCodec::codecForName("UTF-8") : header_file_text_format.codec;
      tmp_header.write(text_codec->fromUnicode(out_string));
      tmp_header.close();
    } else {
      // if opening the temp file failed report error
      ret = false;
    }
  }

  if (ret && guard_start_line != -1) {
    // if the guard was found (and updated updated properly) swap the temp and the target file
    ret = QFile::remove(header_path);
    if (ret)
      ret = QFile::rename(header_path + ".tmp", header_path);
  }

  return ret;
}

} // namespace Core
