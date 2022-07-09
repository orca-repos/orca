// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"

#include <texteditor/texteditor.hpp>
#include <texteditor/codeassist/assistproposalitem.hpp>

namespace CppEditor {

class CPPEDITOR_EXPORT VirtualFunctionProposalItem final : public TextEditor::AssistProposalItem {
public:
  VirtualFunctionProposalItem(const Utils::Link &link, bool openInSplit = true);
  ~VirtualFunctionProposalItem() noexcept override = default;

  auto apply(TextEditor::TextDocumentManipulatorInterface &manipulator, int basePosition) const -> void override;
  auto link() const -> Utils::Link { return m_link; } // Exposed for tests

private:
  Utils::Link m_link;
  bool m_openInSplit;
};

} // namespace CppEditor
