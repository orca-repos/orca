// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "followsymbolinterface.hpp"

namespace CppEditor {

class VirtualFunctionAssistProvider;

class CPPEDITOR_EXPORT FollowSymbolUnderCursor : public FollowSymbolInterface {
public:
  FollowSymbolUnderCursor();

  auto findLink(const CursorInEditor &data, Utils::ProcessLinkCallback &&processLinkCallback, bool resolveTarget, const CPlusPlus::Snapshot &snapshot, const CPlusPlus::Document::Ptr &documentFromSemanticInfo, SymbolFinder *symbolFinder, bool inNextSplit) -> void override;
  auto switchDeclDef(const CursorInEditor &data, Utils::ProcessLinkCallback &&processLinkCallback, const CPlusPlus::Snapshot &snapshot, const CPlusPlus::Document::Ptr &documentFromSemanticInfo, SymbolFinder *symbolFinder) -> void override;
  auto virtualFunctionAssistProvider() -> QSharedPointer<VirtualFunctionAssistProvider>;
  auto setVirtualFunctionAssistProvider(const QSharedPointer<VirtualFunctionAssistProvider> &provider) -> void;

private:
  QSharedPointer<VirtualFunctionAssistProvider> m_virtualFunctionAssistProvider;
};

} // namespace CppEditor
