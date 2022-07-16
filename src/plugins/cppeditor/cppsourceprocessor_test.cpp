// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppsourceprocessor_test.hpp"

#include "baseeditordocumentprocessor.hpp"
#include "cppmodelmanager.hpp"
#include "cppsourceprocessertesthelper.hpp"
#include "cppsourceprocessor.hpp"
#include "cpptoolstestcase.hpp"
#include "editordocumenthandle.hpp"

#include <core/testdatadir.hpp>
#include <texteditor/texteditor.hpp>

#include <cplusplus/CppDocument.h>
#include <utils/fileutils.hpp>

#include <QFile>
#include <QFileInfo>
#include <QtTest>

using namespace CPlusPlus;
using ProjectExplorer::HeaderPathType;

using Include = Document::Include;
using CppEditor::Tests::TestCase;
using CppEditor::Tests::Internal::TestIncludePaths;

namespace CppEditor::Internal {

class SourcePreprocessor
{
public:
    SourcePreprocessor()
        : m_cmm(CppModelManager::instance())
    {
        cleanUp();
    }

    Document::Ptr run(const QString &filePath) const
    {
        QScopedPointer<CppSourceProcessor> sourceProcessor(
                    CppModelManager::createSourceProcessor());
        sourceProcessor->setHeaderPaths({ProjectExplorer::HeaderPath::makeUser(
                                         TestIncludePaths::directoryOfTestFile())});
        sourceProcessor->run(filePath);

        Document::Ptr document = m_cmm->document(filePath);
        return document;
    }

    ~SourcePreprocessor()
    {
        cleanUp();
    }

private:
    void cleanUp()
    {
        m_cmm->GC();
        QVERIFY(m_cmm->snapshot().isEmpty());
    }

private:
    CppModelManager *m_cmm;
};

/// Check: Resolved and unresolved includes are properly tracked.
void SourceProcessorTest::testIncludesResolvedUnresolved()
{
    const QString testFilePath
            = TestIncludePaths::testFilePath(QLatin1String("test_main_resolvedUnresolved.cpp"));

    SourcePreprocessor processor;
    Document::Ptr document = processor.run(testFilePath);
    QVERIFY(document);

    const QList<Document::Include> resolvedIncludes = document->resolvedIncludes();
    QCOMPARE(resolvedIncludes.size(), 1);
    QCOMPARE(resolvedIncludes.at(0).type(), Client::IncludeLocal);
    QCOMPARE(resolvedIncludes.at(0).unresolvedFileName(), QLatin1String("header.hpp"));
    const QString expectedResolvedFileName
            = TestIncludePaths::testFilePath(QLatin1String("header.hpp"));
    QCOMPARE(resolvedIncludes.at(0).resolvedFileName(), expectedResolvedFileName);

    const QList<Document::Include> unresolvedIncludes = document->unresolvedIncludes();
    QCOMPARE(unresolvedIncludes.size(), 1);
    QCOMPARE(unresolvedIncludes.at(0).type(), Client::IncludeLocal);
    QCOMPARE(unresolvedIncludes.at(0).unresolvedFileName(), QLatin1String("notresolvable.hpp"));
    QVERIFY(unresolvedIncludes.at(0).resolvedFileName().isEmpty());
}

/// Check: Avoid self-include entries due to cyclic includes.
void SourceProcessorTest::testIncludesCyclic()
{
    const QString fileName1 = TestIncludePaths::testFilePath(QLatin1String("cyclic1.hpp"));
    const QString fileName2 = TestIncludePaths::testFilePath(QLatin1String("cyclic2.hpp"));
    const QSet<QString> sourceFiles = QSet<QString>() << fileName1 << fileName2;

    // Create global snapshot (needed in BuiltinEditorDocumentParser)
    TestCase testCase;
    testCase.parseFiles(sourceFiles);

    // Open editor
    TextEditor::BaseTextEditor *editor;
    QVERIFY(testCase.openCppEditor(fileName1, &editor));
    testCase.closeEditorAtEndOfTestCase(editor);

    // Check editor snapshot
    const QString filePath = editor->document()->filePath().toString();
    auto *processor = CppModelManager::cppEditorDocumentProcessor(filePath);
    QVERIFY(processor);
    QVERIFY(TestCase::waitForProcessedEditorDocument(filePath));
    Snapshot snapshot = processor->snapshot();
    QCOMPARE(snapshot.size(), 3); // Configuration file included

    // Check includes
    Document::Ptr doc1 = snapshot.document(fileName1);
    QVERIFY(doc1);
    Document::Ptr doc2 = snapshot.document(fileName2);
    QVERIFY(doc2);

    QCOMPARE(doc1->unresolvedIncludes().size(), 0);
    QCOMPARE(doc1->resolvedIncludes().size(), 1);
    QCOMPARE(doc1->resolvedIncludes().first().resolvedFileName(), fileName2);

    QCOMPARE(doc2->unresolvedIncludes().size(), 0);
    QCOMPARE(doc2->resolvedIncludes().size(), 1);
    QCOMPARE(doc2->resolvedIncludes().first().resolvedFileName(), fileName1);
}

/// Check: All include errors are reported as diagnostic messages.
void SourceProcessorTest::testIncludesAllDiagnostics()
{
    const QString testFilePath
            = TestIncludePaths::testFilePath(QLatin1String("test_main_allDiagnostics.cpp"));

    SourcePreprocessor processor;
    Document::Ptr document = processor.run(testFilePath);
    QVERIFY(document);

    QCOMPARE(document->resolvedIncludes().size(), 0);
    QCOMPARE(document->unresolvedIncludes().size(), 3);
    QCOMPARE(document->diagnosticMessages().size(), 3);
}

void SourceProcessorTest::testMacroUses()
{
    const QString testFilePath
            = TestIncludePaths::testFilePath(QLatin1String("test_main_macroUses.cpp"));

    SourcePreprocessor processor;
    Document::Ptr document = processor.run(testFilePath);
    QVERIFY(document);
    const QList<Document::MacroUse> macroUses = document->macroUses();
    QCOMPARE(macroUses.size(), 1);
    const Document::MacroUse macroUse = macroUses.at(0);
    QCOMPARE(macroUse.bytesBegin(), 25);
    QCOMPARE(macroUse.bytesEnd(), 35);
    QCOMPARE(macroUse.utf16charsBegin(), 25);
    QCOMPARE(macroUse.utf16charsEnd(), 35);
    QCOMPARE(macroUse.beginLine(), 2);
}

static bool isMacroDefinedInDocument(const QByteArray &macroName, const Document::Ptr &document)
{
    foreach (const Macro &macro, document->definedMacros()) {
        if (macro.name() == macroName)
            return true;
    }

    return false;
}

static inline QString _(const QByteArray &ba) { return QString::fromLatin1(ba, ba.size()); }

void SourceProcessorTest::testIncludeNext()
{
    const Orca::Plugin::Core::Tests::TestDataDir data(
        _(SRCDIR "/../../../tests/auto/cplusplus/preprocessor/data/include_next-data/"));
    const QString mainFilePath = data.file(QLatin1String("main.cpp"));
    const QString customHeaderPath = data.directory(QLatin1String("customIncludePath"));
    const QString systemHeaderPath = data.directory(QLatin1String("systemIncludePath"));

    CppSourceProcessor::DocumentCallback documentCallback = [](const Document::Ptr &){};
    CppSourceProcessor sourceProcessor(Snapshot(), documentCallback);
    sourceProcessor.setHeaderPaths(ProjectExplorer::toUserHeaderPaths(
                                       QStringList{customHeaderPath, systemHeaderPath}));

    sourceProcessor.run(mainFilePath);
    const Snapshot snapshot = sourceProcessor.snapshot();
    QVERIFY(!snapshot.isEmpty());
    const Document::Ptr mainDocument = snapshot.document(mainFilePath);
    QVERIFY(mainDocument);
    QVERIFY(isMacroDefinedInDocument("OK_FEATURE_X_ENABLED", mainDocument));
}

} // namespace CppEditor::Internal
