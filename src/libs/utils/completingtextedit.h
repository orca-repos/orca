// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "utils_global.h"

#include <QTextEdit>

QT_BEGIN_NAMESPACE
class QCompleter;
class QEvent;
QT_END_NAMESPACE

namespace Utils {

class ORCA_UTILS_EXPORT CompletingTextEdit : public QTextEdit {
  Q_OBJECT
  Q_PROPERTY(int completionLengthThreshold READ completionLengthThreshold WRITE setCompletionLengthThreshold)

public:
  CompletingTextEdit(QWidget *parent = nullptr);
  ~CompletingTextEdit() override;

  auto setCompleter(QCompleter *c) -> void;
  auto completer() const -> QCompleter*;
  auto completionLengthThreshold() const -> int;
  auto setCompletionLengthThreshold(int len) -> void;

protected:
  auto keyPressEvent(QKeyEvent *e) -> void override;
  auto focusInEvent(QFocusEvent *e) -> void override;
  auto event(QEvent *e) -> bool override;

private:
  class CompletingTextEditPrivate *d;
};

} // namespace Utils
