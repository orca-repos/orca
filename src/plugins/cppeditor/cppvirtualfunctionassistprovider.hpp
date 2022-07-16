// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <texteditor/codeassist/genericproposal.hpp>
#include <texteditor/codeassist/iassistprovider.hpp>

#include <cplusplus/CppDocument.h>
#include <cplusplus/Symbols.h>
#include <cplusplus/TypeOfExpression.h>

#include <QSharedPointer>
#include <QTextCursor>

namespace TextEditor {
class AssistProposalItemInterface;
class IAssistProposalWidget;
}

namespace CppEditor {

class CPPEDITOR_EXPORT VirtualFunctionProposal : public TextEditor::GenericProposal {
public:
  VirtualFunctionProposal(int cursorPos, const QList<TextEditor::AssistProposalItemInterface*> &items, bool openInSplit);

private:
  auto createWidget() const -> TextEditor::IAssistProposalWidget* override;

  bool m_openInSplit;
};

class CPPEDITOR_EXPORT VirtualFunctionAssistProvider : public TextEditor::IAssistProvider {
  Q_OBJECT

public:
  VirtualFunctionAssistProvider();

  struct Parameters {
    CPlusPlus::Function *function = nullptr;
    CPlusPlus::Class *staticClass = nullptr;
    QSharedPointer<CPlusPlus::TypeOfExpression> typeOfExpression; // Keeps instantiated symbols.
    CPlusPlus::Snapshot snapshot;
    int cursorPosition = -1;
    bool openInNextSplit = false;
  };

  virtual auto configure(const Parameters &parameters) -> bool;
  auto params() const -> Parameters { return m_params; }
  auto clearParams() -> void { m_params = Parameters(); }

  auto runType() const -> IAssistProvider::RunType override;
  auto createProcessor(const TextEditor::AssistInterface *) const -> TextEditor::IAssistProcessor* override;

private:
  Parameters m_params;
};

} // namespace CppEditor
