// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/filepath.hpp>

#include <QDialog>

namespace Core {
namespace Internal {

namespace Ui {
class AddToVcsDialog;
}

class AddToVcsDialog final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(AddToVcsDialog)

public:
  explicit AddToVcsDialog(QWidget *parent, const QString &title, const Utils::FilePaths &files, const QString &vcs_display_name);
  ~AddToVcsDialog() override;

private:
  Ui::AddToVcsDialog *ui;
};


} // namespace Internal
} // namespace Core
