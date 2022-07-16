// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "cppheadersource_test.hpp"

#include "cppeditorplugin.hpp"
#include "cpptoolsreuse.hpp"
#include "cpptoolstestcase.hpp"
#include "cppfilesettingspage.hpp"

#include <utils/fileutils.hpp>
#include <utils/temporarydirectory.hpp>

#include <QDir>
#include <QtTest>

static inline QString _(const QByteArray &ba) { return QString::fromLatin1(ba, ba.size()); }

static void createTempFile(const QString &fileName)
{
    QFile file(fileName);
    QDir(QFileInfo(fileName).absolutePath()).mkpath(_("."));
    file.open(QFile::WriteOnly);
    file.close();
}

static QString baseTestDir()
{
    return Utils::TemporaryDirectory::masterDirectoryPath() + "/qtc_cppheadersource/";
}

namespace CppEditor::Internal {

void HeaderSourceTest::test()
{
    QFETCH(QString, sourceFileName);
    QFETCH(QString, headerFileName);

    CppEditor::Tests::TemporaryDir temporaryDir;
    QVERIFY(temporaryDir.isValid());

    const QDir path = QDir(temporaryDir.path() + QLatin1Char('/') + _(QTest::currentDataTag()));
    const QString sourcePath = path.absoluteFilePath(sourceFileName);
    const QString headerPath = path.absoluteFilePath(headerFileName);
    createTempFile(sourcePath);
    createTempFile(headerPath);

    bool wasHeader;
    CppEditorPlugin::clearHeaderSourceCache();
    QCOMPARE(correspondingHeaderOrSource(sourcePath, &wasHeader), headerPath);
    QVERIFY(!wasHeader);
    CppEditorPlugin::clearHeaderSourceCache();
    QCOMPARE(correspondingHeaderOrSource(headerPath, &wasHeader), sourcePath);
    QVERIFY(wasHeader);
}

void HeaderSourceTest::test_data()
{
    QTest::addColumn<QString>("sourceFileName");
    QTest::addColumn<QString>("headerFileName");
    QTest::newRow("samedir") << _("foo.cpp") << _("foo.hpp");
    QTest::newRow("includesub") << _("foo.cpp") << _("include/foo.hpp");
    QTest::newRow("headerprefix") << _("foo.cpp") << _("testh_foo.hpp");
    QTest::newRow("sourceprefixwsub") << _("testc_foo.cpp") << _("include/foo.hpp");
    QTest::newRow("sourceAndHeaderPrefixWithBothsub") << _("src/testc_foo.cpp") << _("include/testh_foo.hpp");
}

void HeaderSourceTest::initTestCase()
{
    QDir(baseTestDir()).mkpath(_("."));
    CppFileSettings *fs = CppEditorPlugin::fileSettings();
    fs->headerSearchPaths.append(QLatin1String("include"));
    fs->headerSearchPaths.append(QLatin1String("../include"));
    fs->sourceSearchPaths.append(QLatin1String("src"));
    fs->sourceSearchPaths.append(QLatin1String("../src"));
    fs->headerPrefixes.append(QLatin1String("testh_"));
    fs->sourcePrefixes.append(QLatin1String("testc_"));
}

void HeaderSourceTest::cleanupTestCase()
{
    Utils::FilePath::fromString(baseTestDir()).removeRecursively();
    CppFileSettings *fs = CppEditorPlugin::fileSettings();
    fs->headerSearchPaths.removeLast();
    fs->headerSearchPaths.removeLast();
    fs->sourceSearchPaths.removeLast();
    fs->sourceSearchPaths.removeLast();
    fs->headerPrefixes.removeLast();
    fs->sourcePrefixes.removeLast();
}

} // namespace CppEditor::Internal
