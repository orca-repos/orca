// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "core-global.hpp"

#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QTextCodec>

namespace Utils {
class ListWidget;
}

namespace Orca::Plugin::Core {

class BaseTextDocument;

class CORE_EXPORT CodecSelector : public QDialog {
  Q_OBJECT

public:
  CodecSelector(QWidget *parent, const BaseTextDocument *doc);
  ~CodecSelector() override;

  auto selectedCodec() const -> QTextCodec*;

  // Enumeration returned from QDialog::exec()
  enum Result {
    Cancel,
    Reload,
    Save
  };

private:
  auto updateButtons() const -> void;
  auto buttonClicked(const QAbstractButton *button) -> void;

  bool m_has_decoding_error;
  bool m_is_modified;
  QLabel *m_label;
  Utils::ListWidget *m_list_widget;
  QDialogButtonBox *m_dialog_button_box;
  QAbstractButton *m_reload_button;
  QAbstractButton *m_save_button;
};

} // namespace Orca::Plugin::Core
