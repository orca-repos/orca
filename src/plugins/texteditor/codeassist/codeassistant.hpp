// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "assistenums.hpp"

#include <QObject>

namespace TextEditor {

class CodeAssistantPrivate;
class IAssistProvider;
class TextEditorWidget;

class CodeAssistant : public QObject {
  Q_OBJECT

public:
  CodeAssistant();
  ~CodeAssistant() override;

  auto configure(TextEditorWidget *editorWidget) -> void;
  auto process() -> void;
  auto notifyChange() -> void;
  auto hasContext() const -> bool;
  auto destroyContext() -> void;
  auto userData() const -> QVariant;
  auto setUserData(const QVariant &data) -> void;
  auto invoke(AssistKind assistKind, IAssistProvider *provider = nullptr) -> void;

signals:
  auto finished() -> void;

private:
  CodeAssistantPrivate *d;
};

} //TextEditor
