// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>

namespace CppEditor::Internal::Tests {

class IncludeHierarchyTest : public QObject
{
    Q_OBJECT

private slots:
    void test_data();
    void test();
};

} // namespace CppEditor::Internal::Tests
