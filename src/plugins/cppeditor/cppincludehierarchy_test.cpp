// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppincludehierarchy_test.hpp"

#include "cppeditorwidget.hpp"
#include "cppincludehierarchy.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolstestcase.hpp"

#include <core/core-editor-manager.hpp>
#include <texteditor/texteditor.hpp>
#include <utils/fileutils.hpp>

#include <QByteArray>
#include <QList>
#include <QtTest>

using namespace CPlusPlus;

using CppEditor::Tests::TemporaryDir;

namespace CppEditor::Internal::Tests {

namespace {

QString toString(CppIncludeHierarchyModel &model, const QModelIndex &index, int indent = 0)
{
    QString result = QString(indent, QLatin1Char(' ')) + index.data().toString()
            + QLatin1Char('\n');
    const int itemsCount = model.rowCount(index);
    for (int i = 0; i < itemsCount; ++i) {
        QModelIndex child = model.index(i, 0, index);
        if (model.canFetchMore(child))
            model.fetchMore(child);

        indent += 2;
        result += toString(model, child, indent);
    }
    return result;
}

QString toString(CppIncludeHierarchyModel &model)
{
    model.fetchMore(model.index(0, 0));
    model.fetchMore(model.index(1, 0));
    return toString(model, model.index(0, 0))
            + toString(model, model.index(1, 0));
}

class IncludeHierarchyTestCase : public CppEditor::Tests::TestCase
{
public:
    IncludeHierarchyTestCase(const QList<QByteArray> &sourceList,
                             const QString &expectedHierarchy)
    {
        QVERIFY(succeededSoFar());

        QSet<QString> filePaths;
        const int sourceListSize = sourceList.size();

        TemporaryDir temporaryDir;
        QVERIFY(temporaryDir.isValid());

        for (int i = 0; i < sourceListSize; ++i) {
            const QByteArray &source = sourceList.at(i);

            // Write source to file
            const QString fileName = QString::fromLatin1("file%1.hpp").arg(i+1);
            const QString absoluteFilePath = temporaryDir.createFile(fileName.toLatin1(), source);
            filePaths << absoluteFilePath;
        }

        // Open Editor
        const QString fileName = temporaryDir.path() + QLatin1String("/file1.hpp");
        TextEditor::BaseTextEditor *editor;
        QVERIFY(openCppEditor(fileName, &editor));
        closeEditorAtEndOfTestCase(editor);

        // Update Code Model
        QVERIFY(parseFiles(filePaths));

        // Test model
        CppIncludeHierarchyModel model;
        model.buildHierarchy(editor->document()->filePath().toString());
        const QString actualHierarchy = toString(model);
        QCOMPARE(actualHierarchy, expectedHierarchy);
    }
};

} // anonymous namespace

void IncludeHierarchyTest::test_data()
{
    QTest::addColumn<QList<QByteArray> >("documents");
    QTest::addColumn<QString>("expectedHierarchy");

    QTest::newRow("single-includes")
            << (QList<QByteArray>()
                << QByteArray("#include \"file2.h\"\n")
                << QByteArray())
            << QString::fromLatin1(
                   "Includes\n"
                   "  file2.h\n"
                   "Included by (none)\n");

    QTest::newRow("single-includedBy")
            << (QList<QByteArray>()
                << QByteArray()
                << QByteArray("#include \"file1.h\"\n"))
            << QString::fromLatin1(
                   "Includes (none)\n"
                   "Included by\n"
                   "  file2.h\n"
                   );

    QTest::newRow("both-includes-and-includedBy")
            << (QList<QByteArray>()
                << QByteArray("#include \"file2.h\"\n")
                << QByteArray()
                << QByteArray("#include \"file1.h\"\n"))
            << QString::fromLatin1(
                   "Includes\n"
                   "  file2.h\n"
                   "Included by\n"
                   "  file3.h\n"
                   );

    QTest::newRow("simple-cyclic")
            << (QList<QByteArray>()
                << QByteArray("#include \"file2.h\"\n")
                << QByteArray("#include \"file1.h\"\n"))
            << QString::fromLatin1(
                   "Includes\n"
                   "  file2.h\n"
                   "    file1.h (cyclic)\n"
                   "Included by\n"
                   "  file2.h\n"
                   "    file1.h (cyclic)\n"
                   );

    QTest::newRow("complex-cyclic")
            << (QList<QByteArray>()
                << QByteArray("#include \"file2.h\"\n")
                << QByteArray("#include \"file3.h\"\n")
                << QByteArray("#include \"file1.h\"\n"))
            << QString::fromLatin1(
                   "Includes\n"
                   "  file2.h\n"
                   "    file3.h\n"
                   "      file1.h (cyclic)\n"
                   "Included by\n"
                   "  file3.h\n"
                   "    file2.h\n"
                   "      file1.h (cyclic)\n"
                   );
}

void IncludeHierarchyTest::test()
{
    QFETCH(QList<QByteArray>, documents);
    QFETCH(QString, expectedHierarchy);

    IncludeHierarchyTestCase(documents, expectedHierarchy);
}

} // namespace CppEditor::Internal::Tests
