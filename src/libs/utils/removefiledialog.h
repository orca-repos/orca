// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QDialog>

namespace Utils {

class FilePath;
namespace Ui { class RemoveFileDialog; }

class ORCA_UTILS_EXPORT RemoveFileDialog : public QDialog {
  Q_OBJECT

public:
  explicit RemoveFileDialog(const FilePath &filePath, QWidget *parent = nullptr);
  ~RemoveFileDialog() override;

  auto setDeleteFileVisible(bool visible) -> void;
  auto isDeleteFileChecked() const -> bool;

private:
  Ui::RemoveFileDialog *m_ui;
};

} // namespace Utils
