// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core_global.h"

#include <utils/fileutils.h>

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace Utils {
class Environment;
}

namespace Core {

enum class HandleIncludeGuards {
  No,
  Yes
};

struct CORE_EXPORT FileUtils {
  // Helpers for common directory browser options.
  static auto showInGraphicalShell(QWidget *parent, const Utils::FilePath &path) -> void;
  static auto showInFileSystemView(const Utils::FilePath &path) -> void;
  static auto openTerminal(const Utils::FilePath &path) -> void;
  static auto openTerminal(const Utils::FilePath &path, const Utils::Environment &env) -> void;
  static auto msgFindInDirectory() -> QString;
  static auto msgFileSystemAction() -> QString;
  // Platform-dependent action descriptions
  static auto msgGraphicalShellAction() -> QString;
  static auto msgTerminalHereAction() -> QString;
  static auto msgTerminalWithAction() -> QString;
  // File operations aware of version control and file system case-insensitiveness
  static auto removeFiles(const Utils::FilePaths &file_paths, bool delete_from_fs) -> void;
  static auto renameFile(const Utils::FilePath &org_file_path, const Utils::FilePath &new_file_path, HandleIncludeGuards handle_guards = HandleIncludeGuards::No) -> bool;
  static auto updateHeaderFileGuardIfApplicable(const Utils::FilePath &old_file_path, const Utils::FilePath &new_file_path, HandleIncludeGuards handle_guards) -> void;

private:
  // This method is used to refactor the include guards in the renamed headers
  static auto updateHeaderFileGuardAfterRename(const QString &header_path, const QString &old_header_base_name) -> bool;
};

} // namespace Core
