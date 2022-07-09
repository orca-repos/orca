// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppvirtualfunctionproposalitem.hpp"

#include "cppeditorconstants.hpp"

#include <core/editormanager/editormanager.hpp>

namespace CppEditor {

VirtualFunctionProposalItem::VirtualFunctionProposalItem(const Utils::Link &link, bool openInSplit) : m_link(link), m_openInSplit(openInSplit) {}

auto VirtualFunctionProposalItem::apply(TextEditor::TextDocumentManipulatorInterface &, int) const -> void
{
  if (!m_link.hasValidTarget())
    return;

  Core::EditorManager::OpenEditorFlags flags = Core::EditorManager::NoFlags;
  if (m_openInSplit)
    flags |= Core::EditorManager::OpenInOtherSplit;
  Core::EditorManager::openEditorAt(m_link, CppEditor::Constants::CPPEDITOR_ID, flags);
}

} // namespace CppEditor
