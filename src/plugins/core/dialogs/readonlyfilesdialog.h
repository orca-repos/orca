// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <core/core_global.h>

#include <utils/fileutils.h>

#include <QDialog>

namespace Core {
class IDocument;

namespace Internal {
class ReadOnlyFilesDialogPrivate;
}

class CORE_EXPORT ReadOnlyFilesDialog final : public QDialog {
  Q_OBJECT
  Q_DISABLE_COPY_MOVE(ReadOnlyFilesDialog)
  
  enum read_only_files_tree_column {
    make_writable = 0,
    open_with_vcs = 1,
    save_as = 2,
    file_name = 3,
    folder = 4,
    number_of_columns
  };

public:
  enum read_only_result {
    ro_cancel = -1,
    ro_open_vcs = open_with_vcs,
    ro_make_writable = make_writable,
    ro_save_as = save_as
  };

  explicit ReadOnlyFilesDialog(const Utils::FilePaths &file_paths, QWidget *parent = nullptr);
  explicit ReadOnlyFilesDialog(const Utils::FilePath &file_path, QWidget *parent = nullptr);
  explicit ReadOnlyFilesDialog(IDocument *document, QWidget *parent = nullptr, bool displaySaveAs = false);
  explicit ReadOnlyFilesDialog(const QList<IDocument*> &documents, QWidget *parent = nullptr);
  ~ReadOnlyFilesDialog() override;

  auto setMessage(const QString &message) const -> void;
  auto setShowFailWarning(bool show, const QString &warning = QString()) const -> void;
  auto exec() -> int override;

private:
  friend class Internal::ReadOnlyFilesDialogPrivate;
  Internal::ReadOnlyFilesDialogPrivate *d;
};

} // namespace Core
