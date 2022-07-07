// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QList>

#include "ui_saveitemsdialog.h"

namespace Core {

class IDocument;

namespace Internal {

class SaveItemsDialog final : public QDialog {
  Q_OBJECT

public:
  SaveItemsDialog(QWidget *parent, const QList<IDocument*> &items);

  auto setMessage(const QString &msg) const -> void;
  auto setAlwaysSaveMessage(const QString &msg) const -> void;
  auto alwaysSaveChecked() const -> bool;
  auto itemsToSave() const -> QList<IDocument*>;
  auto filesToDiff() const -> QStringList;

private:
  auto collectItemsToSave() -> void;
  auto collectFilesToDiff() -> void;
  auto discardAll() -> void;
  auto updateButtons() const -> void;
  auto adjustButtonWidths() const -> void;

  Ui::SaveItemsDialog m_ui{};
  QList<IDocument*> m_items_to_save;
  QStringList m_files_to_diff;
  QPushButton *m_diff_button = nullptr;
};

} // namespace Internal
} // namespace Core
