// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "iassistprovider.hpp"

namespace TextEditor {

class TEXTEDITOR_EXPORT CompletionAssistProvider : public IAssistProvider {
  Q_OBJECT

public:
  CompletionAssistProvider(QObject *parent = nullptr);
  ~CompletionAssistProvider() override;

  auto runType() const -> RunType override;
  virtual auto activationCharSequenceLength() const -> int;
  virtual auto isActivationCharSequence(const QString &sequence) const -> bool;
  virtual auto isContinuationChar(const QChar &c) const -> bool;
};

} // TextEditor
