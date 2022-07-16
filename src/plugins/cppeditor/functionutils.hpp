// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QList>
#include <QObject>

namespace CPlusPlus {
class Class;
class Function;
class LookupContext;
class Snapshot;
class Symbol;
} // namespace CPlusPlus

namespace CppEditor::Internal {

class FunctionUtils {
public:
  static auto isVirtualFunction(const CPlusPlus::Function *function, const CPlusPlus::LookupContext &context, QList<const CPlusPlus::Function*> *firstVirtuals = nullptr) -> bool;
  static auto isPureVirtualFunction(const CPlusPlus::Function *function, const CPlusPlus::LookupContext &context, QList<const CPlusPlus::Function*> *firstVirtuals = nullptr) -> bool;
  static auto overrides(CPlusPlus::Function *function, CPlusPlus::Class *functionsClass, CPlusPlus::Class *staticClass, const CPlusPlus::Snapshot &snapshot) -> QList<CPlusPlus::Function*>;
};

#ifdef WITH_TESTS
class FunctionUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void testVirtualFunctions();
    void testVirtualFunctions_data();
};
#endif // WITH_TESTS

} // namespace CppEditor::Internal
