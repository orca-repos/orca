// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "completionassistprovider.hpp"

using namespace TextEditor;

CompletionAssistProvider::CompletionAssistProvider(QObject *parent) : IAssistProvider(parent) {}
CompletionAssistProvider::~CompletionAssistProvider() = default;

auto CompletionAssistProvider::runType() const -> RunType
{
  return AsynchronousWithThread;
}

auto CompletionAssistProvider::activationCharSequenceLength() const -> int
{
  return 0;
}

auto CompletionAssistProvider::isActivationCharSequence(const QString &sequence) const -> bool
{
  Q_UNUSED(sequence)
  return false;
}

auto CompletionAssistProvider::isContinuationChar(const QChar &c) const -> bool
{
  return c.isLetterOrNumber() || c == QLatin1Char('_');
}
