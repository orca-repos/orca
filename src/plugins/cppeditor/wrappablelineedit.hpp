// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QPlainTextEdit>

namespace CppEditor {

class WrappableLineEdit : public QPlainTextEdit {
  Q_OBJECT

public:
  explicit WrappableLineEdit(QWidget *parent = nullptr);

protected:
  auto keyPressEvent(QKeyEvent *event) -> void override;
  auto insertFromMimeData(const QMimeData *source) -> void override;
};

} // namespace CppEditor
