// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "completinglineedit.hpp"

#include <QAbstractItemView>
#include <QCompleter>
#include <QEvent>
#include <QKeyEvent>

namespace Utils {

CompletingLineEdit::CompletingLineEdit(QWidget *parent) : QLineEdit(parent) {}

auto CompletingLineEdit::event(QEvent *e) -> bool
{
  // workaround for ORCABUG-9453
  if (e->type() == QEvent::ShortcutOverride) {
    if (QCompleter *comp = completer()) {
      if (comp->popup() && comp->popup()->isVisible()) {
        auto ke = static_cast<QKeyEvent*>(e);
        if (ke->key() == Qt::Key_Escape && !ke->modifiers()) {
          ke->accept();
          return true;
        }
      }
    }
  }
  return QLineEdit::event(e);
}

auto CompletingLineEdit::keyPressEvent(QKeyEvent *e) -> void
{
  if (e->key() == Qt::Key_Down && !e->modifiers()) {
    if (QCompleter *comp = completer()) {
      if (!comp->popup()->isVisible()) {
        comp->complete();
        return;
      }
    }
  }
  QLineEdit::keyPressEvent(e);
}

} // namespace Utils
