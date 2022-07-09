// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppquickfix.hpp"

#include "cppquickfixassistant.hpp"
#include "cpprefactoringchanges.hpp"

using namespace CPlusPlus;
using namespace TextEditor;

namespace CppEditor::Internal {

auto magicQObjectFunctions() -> const QStringList
{
  static QStringList list{"metaObject", "qt_metacast", "qt_metacall", "qt_static_metacall"};
  return list;
}

CppQuickFixOperation::CppQuickFixOperation(const CppQuickFixInterface &interface, int priority) : QuickFixOperation(priority), CppQuickFixInterface(interface) {}

CppQuickFixOperation::~CppQuickFixOperation() = default;

} // namespace CppEditor::Internal
