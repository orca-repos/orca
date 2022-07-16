// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "core-base-file-filter.hpp"
#include "core-locator-filter-test.hpp"
#include "core-plugin.hpp"
#include "core-test-data-dir.hpp"

#include <utils/algorithm.hpp>
#include <utils/fileutils.hpp>

#include <QDir>
#include <QTextStream>
#include <QtTest>

namespace Orca::Plugin::Core {
namespace {

QTC_DECLARE_MYTESTDATADIR("../../../tests/locators/")

class MyBaseFileFilter : public Orca::Plugin::Core::BaseFileFilter
{
public:
    MyBaseFileFilter(const Utils::FilePaths &theFiles)
    {
        setFileIterator(new BaseFileFilter::ListIterator(theFiles));
    }

    void refresh(QFutureInterface<void> &) override {}
};

class ReferenceData
{
public:
    ReferenceData() = default;
    ReferenceData(const QString &searchText, const ResultDataList &results)
        : searchText(searchText), results(results) {}

    QString searchText;
    ResultDataList results;
};

} // anonymous namespace

Q_DECLARE_METATYPE(ReferenceData)
Q_DECLARE_METATYPE(QList<ReferenceData>)

void Orca::Plugin::Core::CorePlugin::test_basefilefilter()
{
    QFETCH(QStringList, testFiles);
    QFETCH(QList<ReferenceData>, referenceDataList);

    MyBaseFileFilter filter(Utils::transform(testFiles, &Utils::FilePath::fromString));
    BasicLocatorFilterTest test(&filter);

    for (const ReferenceData &reference : qAsConst(referenceDataList)) {
        const QList<LocatorFilterEntry> filterEntries = test.matchesFor(reference.searchText);
        const ResultDataList results = ResultData::fromFilterEntryList(filterEntries);
//        QTextStream(stdout) << "----" << endl;
//        ResultData::printFilterEntries(results);
        QCOMPARE(results, reference.results);
    }
}

void Orca::Plugin::Core::CorePlugin::test_basefilefilter_data()
{
    auto shortNativePath = [](const QString &file) {
        return Utils::FilePath::fromString(file).shortNativePath();
    };

    QTest::addColumn<QStringList>("testFiles");
    QTest::addColumn<QList<ReferenceData> >("referenceDataList");

    const QChar pathSeparator = QDir::separator();
    const MyTestDataDir testDir("testdata_basic");
    const QStringList testFiles({QDir::fromNativeSeparators(testDir.file("file.cpp")),
                                 QDir::fromNativeSeparators(testDir.file("main.cpp")),
                                 QDir::fromNativeSeparators(testDir.file("subdir/main.cpp"))});
    const QStringList testFilesShort = Utils::transform(testFiles, shortNativePath);

    QTest::newRow("BaseFileFilter-EmptyInput")
        << testFiles
        << (QList<ReferenceData>()
            << ReferenceData(
                QString(),
                (QList<ResultData>()
                    << ResultData("file.cpp", testFilesShort.at(0))
                    << ResultData("main.cpp", testFilesShort.at(1))
                    << ResultData("main.cpp", testFilesShort.at(2))))
            );

    QTest::newRow("BaseFileFilter-InputIsFileName")
        << testFiles
        << (QList<ReferenceData>()
            << ReferenceData(
                "main.cpp",
                (QList<ResultData>()
                    << ResultData("main.cpp", testFilesShort.at(1))
                    << ResultData("main.cpp", testFilesShort.at(2))))
           );

    QTest::newRow("BaseFileFilter-InputIsFilePath")
        << testFiles
        << (QList<ReferenceData>()
            << ReferenceData(
                QString("subdir" + pathSeparator + "main.cpp"),
                (QList<ResultData>()
                     << ResultData("main.cpp", testFilesShort.at(2))))
            );

    QTest::newRow("BaseFileFilter-InputIsDirIsPath")
        << testFiles
        << (QList<ReferenceData>()
            << ReferenceData( "subdir", QList<ResultData>())
            << ReferenceData(
                QString("subdir" + pathSeparator + "main.cpp"),
                (QList<ResultData>()
                     << ResultData("main.cpp", testFilesShort.at(2))))
            );

    QTest::newRow("BaseFileFilter-InputIsFileNameFilePathFileName")
        << testFiles
        << (QList<ReferenceData>()
            << ReferenceData(
                "main.cpp",
                (QList<ResultData>()
                    << ResultData("main.cpp", testFilesShort.at(1))
                    << ResultData("main.cpp", testFilesShort.at(2))))
            << ReferenceData(
                QString("subdir" + pathSeparator + "main.cpp"),
                (QList<ResultData>()
                     << ResultData("main.cpp", testFilesShort.at(2))))
            << ReferenceData(
                "main.cpp",
                (QList<ResultData>()
                    << ResultData("main.cpp", testFilesShort.at(1))
                    << ResultData("main.cpp", testFilesShort.at(2))))
            );

    const QStringList priorityTestFiles({testDir.file("qmap.cpp"),
                                         testDir.file("mid_qcore_mac_p.hpp"),
                                         testDir.file("qcore_mac_p.hpp"),
                                         testDir.file("foo_qmap.hpp"),
                                         testDir.file("qmap.hpp"),
                                         testDir.file("bar.hpp")});
    const QStringList priorityTestFilesShort = Utils::transform(priorityTestFiles, shortNativePath);

    QTest::newRow("BaseFileFilter-InputPriorizeFullOverFuzzy")
        << priorityTestFiles
        << (QList<ReferenceData>()
            << ReferenceData(
                "qmap.hpp",
                (QList<ResultData>()
                     << ResultData("qmap.hpp", priorityTestFilesShort.at(4))
                     << ResultData("foo_qmap.hpp", priorityTestFilesShort.at(3))
                     << ResultData("qcore_mac_p.hpp", priorityTestFilesShort.at(2))
                     << ResultData("mid_qcore_mac_p.hpp", priorityTestFilesShort.at(1))))
           );

    const QStringList sortingTestFiles({QDir::fromNativeSeparators(testDir.file("aaa/zfile.cpp")),
                                        QDir::fromNativeSeparators(testDir.file("bbb/yfile.cpp")),
                                        QDir::fromNativeSeparators(testDir.file("ccc/xfile.cpp"))});
    const QStringList sortingTestFilesShort = Utils::transform(sortingTestFiles, shortNativePath);

    QTest::newRow("BaseFileFilter-SortByDisplayName")
        << sortingTestFiles
        << (QList<ReferenceData>()
            << ReferenceData(
                "file",
                (QList<ResultData>()
                    << ResultData("xfile.cpp", sortingTestFilesShort.at(2))
                    << ResultData("yfile.cpp", sortingTestFilesShort.at(1))
                    << ResultData("zfile.cpp", sortingTestFilesShort.at(0))))
            );
}

} // namespace Orca::Plugin::Core
