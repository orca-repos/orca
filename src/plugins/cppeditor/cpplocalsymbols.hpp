// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppsemanticinfo.hpp"

namespace CppEditor::Internal {

class LocalSymbols {
  Q_DISABLE_COPY(LocalSymbols)

public:
  LocalSymbols(CPlusPlus::Document::Ptr doc, CPlusPlus::DeclarationAST *ast);

  SemanticInfo::LocalUseMap uses;
};

} // namespace CppEditor::Internal
