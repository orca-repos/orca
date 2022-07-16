// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <texteditor/codeassist/assistenums.hpp>
#include <texteditor/codeassist/completionassistprovider.hpp>

QT_BEGIN_NAMESPACE
class QTextDocument;
QT_END_NAMESPACE

namespace CPlusPlus {
struct LanguageFeatures;
}

namespace TextEditor {
class TextEditorWidget;
class AssistInterface;
}

namespace Utils {
class FilePath;
}

namespace CppEditor {

class CPPEDITOR_EXPORT CppCompletionAssistProvider : public TextEditor::CompletionAssistProvider {
  Q_OBJECT

public:
  CppCompletionAssistProvider(QObject *parent = nullptr);

  auto activationCharSequenceLength() const -> int override;
  auto isActivationCharSequence(const QString &sequence) const -> bool override;
  auto isContinuationChar(const QChar &c) const -> bool override;

  virtual auto createAssistInterface(const Utils::FilePath &filePath, const TextEditor::TextEditorWidget *textEditorWidget, const CPlusPlus::LanguageFeatures &languageFeatures, int position, TextEditor::AssistReason reason) const -> TextEditor::AssistInterface* = 0;
  static auto activationSequenceChar(const QChar &ch, const QChar &ch2, const QChar &ch3, unsigned *kind, bool wantFunctionCall, bool wantQt5SignalSlots) -> int;
};

} // namespace CppEditor
