// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <texteditor/texteditor_global.hpp>

#include <QObject>

namespace TextEditor {

class AssistInterface;
class IAssistProcessor;

class TEXTEDITOR_EXPORT IAssistProvider : public QObject {
  Q_OBJECT

public:
  IAssistProvider(QObject *parent = nullptr) : QObject(parent) {}

  enum RunType {
    Synchronous,
    Asynchronous,
    AsynchronousWithThread
  };

  virtual auto runType() const -> RunType = 0;
  virtual auto createProcessor(const AssistInterface *assistInterface) const -> IAssistProcessor* = 0;
};

} // TextEditor
