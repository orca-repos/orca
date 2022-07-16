// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "texteditor_global.hpp"

#include "command.hpp"

#include <QPlainTextEdit>
#include <QPointer>

namespace TextEditor {

class TextEditorWidget;

class TEXTEDITOR_EXPORT FormatTask {
public:
  FormatTask(QPlainTextEdit *_editor, const QString &_filePath, const QString &_sourceData, const Command &_command, int _startPos = -1, int _endPos = 0) : editor(_editor), filePath(_filePath), sourceData(_sourceData), command(_command), startPos(_startPos), endPos(_endPos) {}

  QPointer<QPlainTextEdit> editor;
  QString filePath;
  QString sourceData;
  Command command;
  int startPos = -1;
  int endPos = 0;
  QString formattedData;
  QString error;
};

TEXTEDITOR_EXPORT auto formatCurrentFile(const Command &command, int startPos = -1, int endPos = 0) -> void;
TEXTEDITOR_EXPORT auto formatEditor(TextEditorWidget *editor, const Command &command, int startPos = -1, int endPos = 0) -> void;
TEXTEDITOR_EXPORT auto formatEditorAsync(TextEditorWidget *editor, const Command &command, int startPos = -1, int endPos = 0) -> void;

} // namespace TextEditor
