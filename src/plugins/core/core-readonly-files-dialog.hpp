// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <utils/fileutils.hpp>

#include <QDialog>

namespace Orca::Plugin::Core {

class IDocument;
class ReadOnlyFilesDialogPrivate;

class CORE_EXPORT ReadOnlyFilesDialog : public QDialog {
  Q_OBJECT

  enum ReadOnlyFilesTreeColumn {
    MakeWritable = 0,
    OpenWithVCS = 1,
    SaveAs = 2,
    FileName = 3,
    Folder = 4,
    NumberOfColumns
  };

public:
  enum ReadOnlyResult {
    RO_Cancel = -1,
    RO_OpenVCS = OpenWithVCS,
    RO_MakeWritable = MakeWritable,
    RO_SaveAs = SaveAs
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
  friend class ReadOnlyFilesDialogPrivate;
  ReadOnlyFilesDialogPrivate *d;
};

} // namespace Orca::Plugin::Core
