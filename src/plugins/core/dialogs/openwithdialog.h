// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "ui_openwithdialog.h"

#include <QDialog>

namespace Utils {
class FilePath;
}

namespace Core {
namespace Internal {

// Present the user with a file name and a list of available
// editor kinds to choose from.
class OpenWithDialog final : public QDialog, public Ui::OpenWithDialog {
  Q_OBJECT

public:
  OpenWithDialog(const Utils::FilePath &file_path, QWidget *parent);

  auto setEditors(const QStringList &) const -> void;
  auto editor() const -> int;
  auto setCurrentEditor(int index) const -> void;

private:
  auto currentItemChanged(const QListWidgetItem *, QListWidgetItem *) const -> void;
  auto setOkButtonEnabled(bool) const -> void;
};

} // namespace Internal
} // namespace Core
