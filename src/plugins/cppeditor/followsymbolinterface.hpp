// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppeditor_global.hpp"
#include "cursorineditor.hpp"

#include <cplusplus/CppDocument.h>

#include <texteditor/texteditor.hpp>

namespace CppEditor {

class SymbolFinder;

class CPPEDITOR_EXPORT FollowSymbolInterface {
public:
  using Link = Utils::Link;

  virtual ~FollowSymbolInterface() = default;
  virtual auto findLink(const CursorInEditor &data, Utils::ProcessLinkCallback &&processLinkCallback, bool resolveTarget, const CPlusPlus::Snapshot &snapshot, const CPlusPlus::Document::Ptr &documentFromSemanticInfo, SymbolFinder *symbolFinder, bool inNextSplit) -> void = 0;
  virtual auto switchDeclDef(const CursorInEditor &data, Utils::ProcessLinkCallback &&processLinkCallback, const CPlusPlus::Snapshot &snapshot, const CPlusPlus::Document::Ptr &documentFromSemanticInfo, SymbolFinder *symbolFinder) -> void = 0;
};

} // namespace CppEditor
