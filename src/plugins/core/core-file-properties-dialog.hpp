// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <utils/filepath.hpp>

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class FilePropertiesDialog;
}
QT_END_NAMESPACE

namespace Orca::Plugin::Core {

class FilePropertiesDialog final : public QDialog {
  Q_OBJECT

public:
  explicit FilePropertiesDialog(Utils::FilePath file_path, QWidget *parent = nullptr);
  ~FilePropertiesDialog() override;

private:
  auto refresh() const -> void;
  auto setPermission(QFile::Permissions new_permissions, bool set) const -> void;
  auto detectTextFileSettings() const -> void;

  Ui::FilePropertiesDialog *m_ui = nullptr;
  const Utils::FilePath m_file_path;
};

} // namespace Orca::Plugin::Core
