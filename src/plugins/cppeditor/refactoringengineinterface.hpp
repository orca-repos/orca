// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cursorineditor.hpp"
#include "usages.hpp"

#include <utils/link.hpp>
#include <utils/fileutils.hpp>

#include <cplusplus/CppDocument.h>

namespace ClangBackEnd {
class SourceLocationsContainer;
}

namespace TextEditor {
class TextEditorWidget;
}

namespace CppEditor {

class ProjectPart;
class SymbolFinder;

enum class CallType {
  Synchronous,
  Asynchronous
};

// NOTE: This interface is not supposed to be owned as an interface pointer
class CPPEDITOR_EXPORT RefactoringEngineInterface {
public:
  using RenameCallback = std::function<void(const QString &, const ClangBackEnd::SourceLocationsContainer &, int)>;
  using Link = Utils::Link;

  virtual ~RefactoringEngineInterface() = default;
  virtual auto startLocalRenaming(const CursorInEditor &data, const ProjectPart *projectPart, RenameCallback &&renameSymbolsCallback) -> void = 0;
  virtual auto globalRename(const CursorInEditor &data, UsagesCallback &&renameCallback, const QString &replacement) -> void = 0;
  virtual auto findUsages(const CursorInEditor &data, UsagesCallback &&showUsagesCallback) const -> void = 0;
  virtual auto globalFollowSymbol(const CursorInEditor &data, Utils::ProcessLinkCallback &&processLinkCallback, const CPlusPlus::Snapshot &snapshot, const CPlusPlus::Document::Ptr &documentFromSemanticInfo, SymbolFinder *symbolFinder, bool inNextSplit) const -> void = 0;
  virtual auto isRefactoringEngineAvailable() const -> bool { return true; }
};

} // namespace CppEditor
