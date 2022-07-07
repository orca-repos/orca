// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.hpp"

QT_BEGIN_NAMESPACE
class QString;
class QWidget;
QT_END_NAMESPACE

namespace Utils {
class FilePath;

enum ReloadPromptAnswer {
  ReloadCurrent,
  ReloadAll,
  ReloadSkipCurrent,
  ReloadNone,
  ReloadNoneAndDiff,
  CloseCurrent
};

ORCA_UTILS_EXPORT auto reloadPrompt(const FilePath &fileName, bool modified, bool enableDiffOption, QWidget *parent) -> ReloadPromptAnswer;
ORCA_UTILS_EXPORT auto reloadPrompt(const QString &title, const QString &prompt, const QString &details, bool enableDiffOption, QWidget *parent) -> ReloadPromptAnswer;

enum FileDeletedPromptAnswer {
  FileDeletedClose,
  FileDeletedCloseAll,
  FileDeletedSaveAs,
  FileDeletedSave
};

ORCA_UTILS_EXPORT auto fileDeletedPrompt(const QString &fileName, QWidget *parent) -> FileDeletedPromptAnswer;

} // namespace Utils
