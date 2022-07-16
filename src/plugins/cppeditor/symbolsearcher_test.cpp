// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "symbolsearcher_test.hpp"

#include "builtinindexingsupport.hpp"
#include "cppmodelmanager.hpp"
#include "cpptoolstestcase.hpp"
#include "searchsymbols.hpp"

#include <core/testdatadir.hpp>
#include <core/core-search-result-window.hpp>
#include <utils/runextensions.hpp>

#include <QtTest>

namespace {

QTC_DECLARE_MYTESTDATADIR("../../../tests/cppsymbolsearcher/")

inline QString _(const QByteArray &ba) { return QString::fromLatin1(ba, ba.size()); }

class ResultData;
using ResultDataList = QList<ResultData>;

class ResultData
{
public:
    ResultData() = default;
    ResultData(const QString &symbolName, const QString &scope)
        : m_symbolName(symbolName), m_scope(scope) {}

    bool operator==(const ResultData &other) const
    {
        return m_symbolName == other.m_symbolName && m_scope == other.m_scope;
    }

    static ResultDataList fromSearchResultList(const QList<Orca::Plugin::Core::SearchResultItem> &entries)
    {
        ResultDataList result;
        for (const Orca::Plugin::Core::SearchResultItem &entry : entries)
            result << ResultData(entry.lineText(), entry.path().join(QLatin1String("::")));
        return result;
    }

    /// For debugging and creating reference data
    static void printFilterEntries(const ResultDataList &entries)
    {
        QTextStream out(stdout);
        for (const ResultData &entry : entries) {
            out << "<< ResultData(_(\"" << entry.m_symbolName << "\"), _(\""
                << entry.m_scope <<  "\"))" << '\n';
        }
    }

public:
    QString m_symbolName;
    QString m_scope;
};

} // anonymous namespace

Q_DECLARE_METATYPE(ResultData)

QT_BEGIN_NAMESPACE
namespace QTest {

template<> char *toString(const ResultData &data)
{
    QByteArray ba = "\"" + data.m_symbolName.toUtf8() + "\", \"" + data.m_scope.toUtf8() + "\"";
    return qstrdup(ba.data());
}

} // namespace QTest
QT_END_NAMESPACE

namespace CppEditor::Internal {

namespace  {
class SymbolSearcherTestCase : public CppEditor::Tests::TestCase
{
public:
    SymbolSearcherTestCase(const QString &testFile,
                           const SymbolSearcher::Parameters &searchParameters,
                           const ResultDataList &expectedResults)
    {
        QVERIFY(succeededSoFar());
        QVERIFY(parseFiles(testFile));

        CppIndexingSupport *indexingSupport = m_modelManager->indexingSupport();
        const QScopedPointer<SymbolSearcher> symbolSearcher(
            indexingSupport->createSymbolSearcher(searchParameters, QSet<QString>() << testFile));
        QFuture<Orca::Plugin::Core::SearchResultItem> search
            = Utils::runAsync(&SymbolSearcher::runSearch, symbolSearcher.data());
        search.waitForFinished();
        ResultDataList results = ResultData::fromSearchResultList(search.results());
        QCOMPARE(results, expectedResults);
    }
};
} // anonymous namespace

void SymbolSearcherTest::test()
{
    QFETCH(QString, testFile);
    QFETCH(SymbolSearcher::Parameters, searchParameters);
    QFETCH(ResultDataList, expectedResults);

    SymbolSearcherTestCase(testFile, searchParameters, expectedResults);
}

void SymbolSearcherTest::test_data()
{
    QTest::addColumn<QString>("testFile");
    QTest::addColumn<SymbolSearcher::Parameters>("searchParameters");
    QTest::addColumn<ResultDataList>("expectedResults");

    MyTestDataDir testDirectory(QLatin1String("testdata_basic"));
    const QString testFile = testDirectory.file(QLatin1String("file1.cpp"));

    SymbolSearcher::Parameters searchParameters;

    // Check All Symbol Types
    searchParameters = SymbolSearcher::Parameters();
    searchParameters.text = _("");
    searchParameters.flags = {};
    searchParameters.types = SearchSymbols::AllTypes;
    searchParameters.scope = SymbolSearcher::SearchGlobal;
    QTest::newRow("BuiltinSymbolSearcher::AllTypes")
        << testFile
        << searchParameters
        << (ResultDataList()
            << ResultData(_("int myVariable"), _(""))
            << ResultData(_("myFunction(bool, int)"), _(""))
            << ResultData(_("MyEnum"), _(""))
            << ResultData(_("int V1"), _("MyEnum"))
            << ResultData(_("int V2"), _("MyEnum"))
            << ResultData(_("MyClass"), _(""))
            << ResultData(_("MyClass()"), _("MyClass"))
            << ResultData(_("functionDeclaredOnly()"), _("MyClass"))
            << ResultData(_("functionDefinedInClass(bool, int)"), _("MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"), _("MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"), _("MyClass"))
            << ResultData(_("int myVariable"), _("MyNamespace"))
            << ResultData(_("myFunction(bool, int)"), _("MyNamespace"))
            << ResultData(_("MyEnum"), _("MyNamespace"))
            << ResultData(_("int V1"), _("MyNamespace::MyEnum"))
            << ResultData(_("int V2"), _("MyNamespace::MyEnum"))
            << ResultData(_("MyClass"), _("MyNamespace"))
            << ResultData(_("MyClass()"), _("MyNamespace::MyClass"))
            << ResultData(_("functionDeclaredOnly()"), _("MyNamespace::MyClass"))
            << ResultData(_("functionDefinedInClass(bool, int)"), _("MyNamespace::MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"), _("MyNamespace::MyClass"))
            << ResultData(_("functionDefinedOutSideClassAndNamespace(float)"),
                          _("MyNamespace::MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"), _("MyNamespace::MyClass"))
            << ResultData(_("functionDefinedOutSideClassAndNamespace(float)"),
                          _("MyNamespace::MyClass"))
            << ResultData(_("MyNamespace::MyClass MY_CLASS"), _(""))
            << ResultData(_("int myVariable"), _("<anonymous namespace>"))
            << ResultData(_("myFunction(bool, int)"), _("<anonymous namespace>"))
            << ResultData(_("MyEnum"), _("<anonymous namespace>"))
            << ResultData(_("int V1"), _("<anonymous namespace>::MyEnum"))
            << ResultData(_("int V2"), _("<anonymous namespace>::MyEnum"))
            << ResultData(_("MyClass"), _("<anonymous namespace>"))
            << ResultData(_("MyClass()"), _("<anonymous namespace>::MyClass"))
            << ResultData(_("functionDeclaredOnly()"), _("<anonymous namespace>::MyClass"))
            << ResultData(_("functionDefinedInClass(bool, int)"),
                          _("<anonymous namespace>::MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"),
                          _("<anonymous namespace>::MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"),
                          _("<anonymous namespace>::MyClass"))
            << ResultData(_("MyClass MY_OTHER_CLASS"), _(""))
            << ResultData(_("main()"), _(""))

        );

    // Check Classes
    searchParameters = SymbolSearcher::Parameters();
    searchParameters.text = _("myclass");
    searchParameters.flags = {};
    searchParameters.types = SymbolSearcher::Classes;
    searchParameters.scope = SymbolSearcher::SearchGlobal;
    QTest::newRow("BuiltinSymbolSearcher::Classes")
        << testFile
        << searchParameters
        << (ResultDataList()
            << ResultData(_("MyClass"), _(""))
            << ResultData(_("MyClass"), _("MyNamespace"))
            << ResultData(_("MyClass"), _("<anonymous namespace>"))
        );

    // Check Functions
    searchParameters = SymbolSearcher::Parameters();
    searchParameters.text = _("fun");
    searchParameters.flags = {};
    searchParameters.types = SymbolSearcher::Functions;
    searchParameters.scope = SymbolSearcher::SearchGlobal;
    QTest::newRow("BuiltinSymbolSearcher::Functions")
        << testFile
        << searchParameters
        << (ResultDataList()
            << ResultData(_("myFunction(bool, int)"), _(""))
            << ResultData(_("functionDefinedInClass(bool, int)"), _("MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"), _("MyClass"))
            << ResultData(_("myFunction(bool, int)"), _("MyNamespace"))
            << ResultData(_("functionDefinedInClass(bool, int)"), _("MyNamespace::MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"), _("MyNamespace::MyClass"))
            << ResultData(_("functionDefinedOutSideClassAndNamespace(float)"),
                          _("MyNamespace::MyClass"))
            << ResultData(_("myFunction(bool, int)"), _("<anonymous namespace>"))
            << ResultData(_("functionDefinedInClass(bool, int)"),
                          _("<anonymous namespace>::MyClass"))
            << ResultData(_("functionDefinedOutSideClass(char)"),
                          _("<anonymous namespace>::MyClass"))
        );

    // Check Enums
    searchParameters = SymbolSearcher::Parameters();
    searchParameters.text = _("enum");
    searchParameters.flags = {};
    searchParameters.types = SymbolSearcher::Enums;
    searchParameters.scope = SymbolSearcher::SearchGlobal;
    QTest::newRow("BuiltinSymbolSearcher::Enums")
        << testFile
        << searchParameters
        << (ResultDataList()
            << ResultData(_("MyEnum"), _(""))
            << ResultData(_("MyEnum"), _("MyNamespace"))
            << ResultData(_("MyEnum"), _("<anonymous namespace>"))
        );

    // Check Declarations
    searchParameters = SymbolSearcher::Parameters();
    searchParameters.text = _("myvar");
    searchParameters.flags = {};
    searchParameters.types = SymbolSearcher::Declarations;
    searchParameters.scope = SymbolSearcher::SearchGlobal;
    QTest::newRow("BuiltinSymbolSearcher::Declarations")
        << testFile
        << searchParameters
        << (ResultDataList()
            << ResultData(_("int myVariable"), _(""))
            << ResultData(_("int myVariable"), _("MyNamespace"))
            << ResultData(_("int myVariable"), _("<anonymous namespace>"))
        );
}

} // namespace CppEditor::Internal
