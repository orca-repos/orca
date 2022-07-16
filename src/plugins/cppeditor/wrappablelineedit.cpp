// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "wrappablelineedit.hpp"

#include <QMimeData>

namespace CppEditor {

WrappableLineEdit::WrappableLineEdit(QWidget *parent) : QPlainTextEdit(parent)
{
  setMaximumBlockCount(1); // Restrict to a single line.
}

auto WrappableLineEdit::keyPressEvent(QKeyEvent *event) -> void
{
  switch (event->key()) {
  case Qt::Key_Enter:
  case Qt::Key_Return:
    return; // Eat these to avoid new lines.
  case Qt::Key_Backtab:
  case Qt::Key_Tab:
    // Let the parent handle these because they might be used for navigation purposes.
    event->ignore();
    return;
  default:
    return QPlainTextEdit::keyPressEvent(event);
  }
}

auto WrappableLineEdit::insertFromMimeData(const QMimeData *source) -> void
{
  insertPlainText(source->text().simplified()); // Filter out new lines.
}

} // namespace CppEditor
