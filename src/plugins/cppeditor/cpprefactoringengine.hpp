// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "refactoringengineinterface.hpp"

namespace CppEditor::Internal {

class CppRefactoringEngine : public RefactoringEngineInterface {
public:
  auto startLocalRenaming(const CursorInEditor &data, const ProjectPart *projectPart, RenameCallback &&renameSymbolsCallback) -> void override;
  auto globalRename(const CursorInEditor &data, UsagesCallback &&, const QString &replacement) -> void override;
  auto findUsages(const CursorInEditor &data, UsagesCallback &&) const -> void override;
  auto globalFollowSymbol(const CursorInEditor &data, Utils::ProcessLinkCallback &&processLinkCallback, const CPlusPlus::Snapshot &snapshot, const CPlusPlus::Document::Ptr &documentFromSemanticInfo, SymbolFinder *symbolFinder, bool inNextSplit) const -> void override;
};

} // namespace CppEditor::Internal
