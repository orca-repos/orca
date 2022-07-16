// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include "cppcursorinfo.hpp"
#include "cppeditor_global.hpp"

#include <cplusplus/CppDocument.h>

#include <QFuture>

namespace CppEditor {

class CPPEDITOR_EXPORT BuiltinCursorInfo {
public:
  static auto run(const CursorInfoParams &params) -> QFuture<CursorInfo>;
  static auto findLocalUses(const CPlusPlus::Document::Ptr &document, int line, int column) -> SemanticInfo::LocalUseMap;
};

} // namespace CppEditor
