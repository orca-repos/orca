// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#pragma once

#include <QObject>

namespace CppEditor::Internal {

class LocatorFilterTest : public QObject
{
    Q_OBJECT

private slots:
    void testLocatorFilter();
    void testLocatorFilter_data();
    void testCurrentDocumentFilter();
    void testCurrentDocumentHighlighting();
    void testFunctionsFilterHighlighting();
};

} // namespace CppEditor::Internal
