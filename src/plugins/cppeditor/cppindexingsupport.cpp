// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppindexingsupport.hpp"

namespace CppEditor {

CppIndexingSupport::~CppIndexingSupport() = default;

SymbolSearcher::SymbolSearcher(QObject *parent) : QObject(parent) {}

SymbolSearcher::~SymbolSearcher() = default;

} // namespace CppEditor
