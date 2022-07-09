// SPDX-License-Identifier: GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "qtprojectimporter.hpp"

#include "qtkitinformation.hpp"
#include "qtversionfactory.hpp"
#include "qtversionmanager.hpp"

#include <projectexplorer/kit.hpp>
#include <projectexplorer/kitmanager.hpp>

#include <utils/algorithm.hpp>
#include <utils/filepath.hpp>
#include <utils/hostosinfo.hpp>
#include <utils/qtcassert.hpp>
#include <utils/temporarydirectory.hpp>

#include <QFileInfo>
#include <QList>

using namespace ProjectExplorer;
using namespace Utils;

namespace QtSupport {

QtProjectImporter::QtProjectImporter(const FilePath &path) : ProjectImporter(path)
{
  useTemporaryKitAspect(QtKitAspect::id(), [this](Kit *k, const QVariantList &vl) { cleanupTemporaryQt(k, vl); }, [this](Kit *k, const QVariantList &vl) { persistTemporaryQt(k, vl); });
}

auto QtProjectImporter::findOrCreateQtVersion(const FilePath &qmakePath) const -> QtVersionData
{
  QtVersionData result;
  result.qt = QtVersionManager::version(equal(&QtVersion::qmakeFilePath, qmakePath));
  if (result.qt) {
    // Check if version is a temporary qt
    const auto qtId = result.qt->uniqueId();
    result.isTemporary = hasKitWithTemporaryData(QtKitAspect::id(), qtId);
    return result;
  }

  // Create a new version if not found:
  // Do not use the canonical path here...
  result.qt = QtVersionFactory::createQtVersionFromQMakePath(qmakePath);
  result.isTemporary = true;
  if (result.qt) {
    UpdateGuard guard(*this);
    QtVersionManager::addVersion(result.qt);
  }

  return result;
}

auto QtProjectImporter::createTemporaryKit(const QtVersionData &versionData, const KitSetupFunction &additionalSetup) const -> Kit*
{
  return ProjectImporter::createTemporaryKit([&additionalSetup, &versionData, this](Kit *k) -> void {
    QtKitAspect::setQtVersion(k, versionData.qt);
    if (versionData.qt) {
      if (versionData.isTemporary)
        addTemporaryData(QtKitAspect::id(), versionData.qt->uniqueId(), k);

      k->setUnexpandedDisplayName(versionData.qt->displayName());;
    }

    additionalSetup(k);
  });
}

static auto versionFromVariant(const QVariant &v) -> QtVersion*
{
  bool ok;
  const auto qtId = v.toInt(&ok);
  QTC_ASSERT(ok, return nullptr);
  return QtVersionManager::version(qtId);
}

auto QtProjectImporter::cleanupTemporaryQt(Kit *k, const QVariantList &vl) -> void
{
  if (vl.isEmpty())
    return; // No temporary Qt
  QTC_ASSERT(vl.count() == 1, return);
  const auto version = versionFromVariant(vl.at(0));
  QTC_ASSERT(version, return);
  QtVersionManager::removeVersion(version);
  QtKitAspect::setQtVersion(k, nullptr); // Always mark Kit as not using this Qt
}

auto QtProjectImporter::persistTemporaryQt(Kit *k, const QVariantList &vl) -> void
{
  if (vl.isEmpty())
    return; // No temporary Qt
  QTC_ASSERT(vl.count() == 1, return);
  const auto data = vl.at(0);
  const auto tmpVersion = versionFromVariant(data);
  const auto actualVersion = QtKitAspect::qtVersion(k);

  // User changed Kit away from temporary Qt that was set up:
  if (tmpVersion && actualVersion != tmpVersion)
    QtVersionManager::removeVersion(tmpVersion);
}

#if WITH_TESTS
} // namespace QtSupport

#include "qtsupportplugin.hpp"
#include "qtversions.hpp"

#include <projectexplorer/buildconfiguration.hpp>
#include <projectexplorer/buildinfo.hpp>

#include <cassert>

#include <QTest>

namespace QtSupport {
namespace Internal {

struct DirectoryData {
    DirectoryData(const QString &ip,
                  Kit *k = nullptr, bool ink = false,
                  const Utils::FilePath &qp = Utils::FilePath(), bool inq = false) :
        isNewKit(ink), isNewQt(inq),
        importPath(Utils::FilePath::fromString(ip)),
        kit(k), qmakePath(qp)
    { }

    DirectoryData(const DirectoryData &other) :
        isNewKit(other.isNewKit),
        isNewQt(other.isNewQt),
        importPath(other.importPath),
        kit(other.kit),
        qmakePath(other.qmakePath)
    { }

    const bool isNewKit = false;
    const bool isNewQt = false;
    const Utils::FilePath importPath;
    Kit *const kit = nullptr;
    const Utils::FilePath qmakePath;
};

class TestQtProjectImporter : public QtProjectImporter
{
public:
    TestQtProjectImporter(const Utils::FilePath &pp, const QList<void *> &testData) :
        QtProjectImporter(pp),
        m_testData(testData)
    { }

    QStringList importCandidates() override;

    bool allDeleted() const { return m_deletedTestData.count() == m_testData.count();}

protected:
    QList<void *> examineDirectory(const Utils::FilePath &importPath,
                                   QString *warningMessage) const override;
    bool matchKit(void *directoryData, const Kit *k) const override;
    Kit *createKit(void *directoryData) const override;
    const QList<BuildInfo> buildInfoList(void *directoryData) const override;
    void deleteDirectoryData(void *directoryData) const override;

private:
    const QList<void *> m_testData;
    mutable Utils::FilePath m_path;
    mutable QVector<void*> m_deletedTestData;

    QList<Kit *> m_deletedKits;
};

QStringList TestQtProjectImporter::importCandidates()
{
    return QStringList();
}

QList<void *> TestQtProjectImporter::examineDirectory(const Utils::FilePath &importPath,
                                                      QString *warningMessage) const
{
    Q_UNUSED(warningMessage)
    m_path = importPath;

    assert(m_deletedTestData.isEmpty());

    return m_testData;
}

bool TestQtProjectImporter::matchKit(void *directoryData, const Kit *k) const
{
    assert(m_testData.contains(directoryData));
    assert(!m_deletedTestData.contains(directoryData));
    const DirectoryData *dd = static_cast<const DirectoryData *>(directoryData);
    assert(dd->importPath == m_path);
    return dd->kit->displayName() == k->displayName();
}

Kit *TestQtProjectImporter::createKit(void *directoryData) const
{
    assert(m_testData.contains(directoryData));
    assert(!m_deletedTestData.contains(directoryData));
    const DirectoryData *dd = static_cast<const DirectoryData *>(directoryData);
    assert(dd->importPath == m_path);

    if (KitManager::kit(dd->kit->id())) // known kit
        return dd->kit;

    // New temporary kit:
    return createTemporaryKit(findOrCreateQtVersion(dd->qmakePath),
                              [dd](Kit *k) {
        QtVersion *qt = QtKitAspect::qtVersion(k);
        QMap<Utils::Id, QVariant> toKeep;
        for (const Utils::Id &key : k->allKeys()) {
            if (key.toString().startsWith("PE.tmp."))
                toKeep.insert(key, k->value(key));
        }
        k->copyFrom(dd->kit);
        for (auto i = toKeep.constBegin(); i != toKeep.constEnd(); ++i)
            k->setValue(i.key(), i.value());
        QtKitAspect::setQtVersion(k, qt);
    });
}

const QList<BuildInfo> TestQtProjectImporter::buildInfoList(void *directoryData) const
{
    Q_UNUSED(directoryData)
    assert(m_testData.contains(directoryData));
    assert(!m_deletedTestData.contains(directoryData));
    assert(static_cast<const DirectoryData *>(directoryData)->importPath == m_path);

    BuildInfo info;
    info.displayName = "Test Build info";
    info.typeName = "Debug";
    info.buildDirectory = m_path;
    info.buildType = BuildConfiguration::Debug;
    return {info};
}

void TestQtProjectImporter::deleteDirectoryData(void *directoryData) const
{
    assert(m_testData.contains(directoryData));
    assert(!m_deletedTestData.contains(directoryData));
    assert(static_cast<const DirectoryData *>(directoryData)->importPath == m_path);

    // Clean up in-the-wild
    m_deletedTestData.append(directoryData);
    delete static_cast<DirectoryData *>(directoryData);
}

static QStringList additionalFilesToCopy(const QtVersion *qt)
{
    // This is a hack and only works with local, "standard" installations of Qt
    const int major = qt->qtVersion().majorVersion;
    if (major >= 6) {
        if (HostOsInfo::isMacHost()) {
            return {"lib/QtCore.framework/Versions/A/QtCore"};
        } else if (HostOsInfo::isWindowsHost()) {
            const QString release = QString("bin/Qt%1Core.dll").arg(major);
            const QString debug = QString("bin/Qt%1Cored.dll").arg(major);
            const FilePath base = qt->qmakeFilePath().parentDir().parentDir();
            if (base.pathAppended(release).exists())
                return {release};
            if (base.pathAppended(debug).exists())
                return {debug};
            return {release};
        } else if (HostOsInfo::isLinuxHost()) {
            const QString core = QString("lib/libQt%1Core.so.%1").arg(major);
            const QDir base(qt->qmakeFilePath().parentDir().parentDir().pathAppended("lib").toString());
            const QStringList icuLibs = Utils::transform(base.entryList({"libicu*.so.*"}), [](const QString &lib) { return QString("lib/" + lib); });
            return QStringList(core) + icuLibs;
        }
    }
    return {};
}

static Utils::FilePath setupQmake(const QtVersion *qt, const QString &path)
{
    // This is a hack and only works with local, "standard" installations of Qt
    const FilePath qmake = qt->qmakeFilePath().canonicalPath();
    const QString qmakeFile = "bin/" + qmake.fileName();
    const FilePath source = qmake.parentDir().parentDir();
    const FilePath target = FilePath::fromString(path);

    const QStringList filesToCopy = QStringList(qmakeFile) + additionalFilesToCopy(qt);
    for (const QString &file : filesToCopy) {
        const FilePath sourceFile = source.pathAppended(file);
        const FilePath targetFile = target.pathAppended(file);
        if (!targetFile.parentDir().ensureWritableDir() || !sourceFile.copyFile(targetFile)) {
            qDebug() << "Failed to copy" << sourceFile.toString() << "to" << targetFile.toString();
            return {};
        }
    }

    return target.pathAppended(qmakeFile);
}

void QtSupportPlugin::testQtProjectImporter_oneProject_data()
{
    // In the next two lists: 0 is the defaultKit/Qt, anything > 0 is a new kit/Qt
    QTest::addColumn<QList<int>>("kitIndexList"); // List of indices from the kitTemplate below.
    QTest::addColumn<QList<int>>("qtIndexList"); // List of indices from the qmakePaths below.

    QTest::addColumn<QList<bool>>("operationList"); // Persist (true) or cleanup (false) the result.
    QTest::addColumn<QList<bool>>("kitIsPersistentList"); // Is the Kit still there after operation?
    QTest::addColumn<QList<bool>>("qtIsPersistentList"); // Is the Qt still there after operation?

    QTest::newRow("nothing to import")
            << QList<int>() << QList<int>() << QList<bool>()
            << QList<bool>() << QList<bool>();

    QTest::newRow("existing kit, cleanup")
            << QList<int>({0}) << QList<int>({0}) << QList<bool>({false})
            << QList<bool>({true}) << QList<bool>({true});
    QTest::newRow("existing kit, persist")
            << QList<int>({0}) << QList<int>({0}) << QList<bool>({true})
            << QList<bool>({true}) << QList<bool>({true});

    QTest::newRow("new kit, existing Qt, cleanup")
            << QList<int>({1}) << QList<int>({0}) << QList<bool>({false})
            << QList<bool>({false}) << QList<bool>({true});
    QTest::newRow("new kit, existing Qt, persist")
            << QList<int>({1}) << QList<int>({0}) << QList<bool>({true})
            << QList<bool>({true}) << QList<bool>({true});

    QTest::newRow("new kit, new Qt, cleanup")
            << QList<int>({1}) << QList<int>({1}) << QList<bool>({false})
            << QList<bool>({false}) << QList<bool>({false});
    QTest::newRow("new kit, new Qt, persist")
            << QList<int>({1}) << QList<int>({1}) << QList<bool>({true})
            << QList<bool>({true}) << QList<bool>({true});

    QTest::newRow("2 new kit, same existing Qt, cleanup-cleanup")
            << QList<int>({1, 2}) << QList<int>({0, 0}) << QList<bool>({false, false})
            << QList<bool>({false, false}) << QList<bool>({true, true});
    QTest::newRow("2 new kit, same existing Qt, persist-cleanup")
            << QList<int>({1, 2}) << QList<int>({0, 0}) << QList<bool>({true, false})
            << QList<bool>({true, false}) << QList<bool>({true, true});
    QTest::newRow("2 new kit, same existing Qt, cleanup-persist")
            << QList<int>({1, 2}) << QList<int>({0, 0}) << QList<bool>({false, true})
            << QList<bool>({false, true}) << QList<bool>({true, true});
    QTest::newRow("2 new kit, same existing Qt, persist-persist")
            << QList<int>({1, 2}) << QList<int>({0, 0}) << QList<bool>({true, true})
            << QList<bool>({true, true}) << QList<bool>({true, true});

    QTest::newRow("2 new kit, same new Qt, cleanup-cleanup")
            << QList<int>({1, 2}) << QList<int>({1, 1}) << QList<bool>({false, false})
            << QList<bool>({false, false}) << QList<bool>({true, false});
    QTest::newRow("2 new kit, same new Qt, persist-cleanup")
            << QList<int>({1, 2}) << QList<int>({1, 1}) << QList<bool>({true, false})
            << QList<bool>({true, false}) << QList<bool>({true, true});
    QTest::newRow("2 new kit, same new Qt, cleanup-persist")
            << QList<int>({1, 2}) << QList<int>({1, 1}) << QList<bool>({false, true})
            << QList<bool>({false, true}) << QList<bool>({true, true});
    QTest::newRow("2 new kit, same new Qt, persist-persist")
            << QList<int>({1, 2}) << QList<int>({1, 1}) << QList<bool>({true, true})
            << QList<bool>({true, true}) << QList<bool>({true, true});

    QTest::newRow("2 new kit, 2 new Qt, cleanup-cleanup")
            << QList<int>({1, 2}) << QList<int>({1, 2}) << QList<bool>({false, false})
            << QList<bool>({false, false}) << QList<bool>({false, false});
    QTest::newRow("2 new kit, 2 new Qt, persist-cleanup")
            << QList<int>({1, 2}) << QList<int>({1, 2}) << QList<bool>({true, false})
            << QList<bool>({true, false}) << QList<bool>({true, false});
    QTest::newRow("2 new kit, 2 new Qt, cleanup-persist")
            << QList<int>({1, 2}) << QList<int>({1, 2}) << QList<bool>({false, true})
            << QList<bool>({false, true}) << QList<bool>({false, true});
    QTest::newRow("2 new kit, 2 new Qt, persist-persist")
            << QList<int>({1, 2}) << QList<int>({1, 2}) << QList<bool>({true, true})
            << QList<bool>({true, true}) << QList<bool>({true, true});
}

void QtSupportPlugin::testQtProjectImporter_oneProject()
{
    // --------------------------------------------------------------------
    // Setup:
    // --------------------------------------------------------------------

    Kit *defaultKit = KitManager::defaultKit();
    QVERIFY(defaultKit);

    QtVersion *defaultQt = QtKitAspect::qtVersion(defaultKit);
    QVERIFY(defaultQt);

    const Utils::TemporaryDirectory tempDir1("tmp1");
    const Utils::TemporaryDirectory tempDir2("tmp2");

    const QString appDir = QCoreApplication::applicationDirPath();

    // Templates referrenced by test data:
    QVector<Kit *> kitTemplates = {defaultKit, defaultKit->clone(), defaultKit->clone()};
    // Customize kit numbers 1 and 2:
    QtKitAspect::setQtVersion(kitTemplates[1], nullptr);
    QtKitAspect::setQtVersion(kitTemplates[2], nullptr);
    SysRootKitAspect::setSysRoot(kitTemplates[1], "/some/path");
    SysRootKitAspect::setSysRoot(kitTemplates[2], "/some/other/path");

    QVector<Utils::FilePath> qmakePaths = {defaultQt->qmakeFilePath(),
                                           setupQmake(defaultQt, tempDir1.path().path()),
                                           setupQmake(defaultQt, tempDir2.path().path())};

    for (int i = 1; i < qmakePaths.count(); ++i)
        QVERIFY(!QtVersionManager::version(Utils::equal(&QtVersion::qmakeFilePath, qmakePaths.at(i))));

    QList<DirectoryData *> testData;

    QFETCH(QList<int>, kitIndexList);
    QFETCH(QList<int>, qtIndexList);
    QFETCH(QList<bool>, operationList);
    QFETCH(QList<bool>, kitIsPersistentList);
    QFETCH(QList<bool>, qtIsPersistentList);

    QCOMPARE(kitIndexList.count(), qtIndexList.count());
    QCOMPARE(kitIndexList.count(), operationList.count());
    QCOMPARE(kitIndexList.count(), kitIsPersistentList.count());
    QCOMPARE(kitIndexList.count(), qtIsPersistentList.count());

    for (int i = 0; i < kitIndexList.count(); ++i) {
        const int kitIndex = kitIndexList.at(i);
        const int qtIndex = qtIndexList.at(i);

        testData.append(new DirectoryData(appDir,
                                          (kitIndex < 0) ? nullptr : kitTemplates.at(kitIndex),
                                          (kitIndex > 0), /* new Kit */
                                          (qtIndex < 0) ? Utils::FilePath() : qmakePaths.at(qtIndex),
                                          (qtIndex > 0) /* new Qt */));
    }

    // Finally set up importer:
    // Copy the directoryData so that importer is free to delete it later.
    TestQtProjectImporter importer(tempDir1.path(),
                                   Utils::transform(testData, [](DirectoryData *i) {
                                       return static_cast<void *>(new DirectoryData(*i));
                                   }));

    // --------------------------------------------------------------------
    // Test: Import:
    // --------------------------------------------------------------------

    // choose an existing directory to "import"
    const QList<BuildInfo> buildInfo = importer.import(Utils::FilePath::fromString(appDir), true);

    // VALIDATE: Basic TestImporter state:
    QCOMPARE(importer.projectFilePath(), tempDir1.path());
    QCOMPARE(importer.allDeleted(), true);

    // VALIDATE: Result looks reasonable:
    QCOMPARE(buildInfo.count(), testData.count());

    QList<Kit *> newKits;

    // VALIDATE: Validate result:
    for (int i = 0; i < buildInfo.count(); ++i) {
        const DirectoryData *dd = testData.at(i);
        const BuildInfo &bi = buildInfo.at(i);

        // VALIDATE: Kit id is unchanged (unless it is a new kit)
        if (!dd->isNewKit)
            QCOMPARE(bi.kitId, defaultKit->id());

        // VALIDATE: Kit is registered with the KitManager
        Kit *newKit = KitManager::kit(bi.kitId);
        QVERIFY(newKit);

        const int newQtId = QtKitAspect::qtVersionId(newKit);

        // VALIDATE: Qt id is unchanged (unless it is a new Qt)
        if (!dd->isNewQt)
            QCOMPARE(newQtId, defaultQt->uniqueId());

        // VALIDATE: Qt is known to QtVersionManager
        QtVersion *newQt = QtVersionManager::version(newQtId);
        QVERIFY(newQt);

        // VALIDATE: Qt has the expected qmakePath
        QCOMPARE(dd->qmakePath, newQt->qmakeFilePath());

        // VALIDATE: All keys are unchanged:
        QList<Utils::Id> newKitKeys = newKit->allKeys();
        const QList<Utils::Id> templateKeys = dd->kit->allKeys();

        if (dd->isNewKit)
            QVERIFY(templateKeys.count() < newKitKeys.count()); // new kit will have extra keys!
        else
            QCOMPARE(templateKeys.count(), newKitKeys.count()); // existing kit needs to be unchanged!

        for (Utils::Id id : templateKeys) {
            if (id == QtKitAspect::id())
                continue; // with the exception of the Qt one...
            QVERIFY(newKit->hasValue(id));
            QVERIFY(dd->kit->value(id) == newKit->value(id));
        }

        newKits.append(newKit);
    }

    // VALIDATE: No kit got lost;-)
    QCOMPARE(newKits.count(), buildInfo.count());

    QList<Kit *> toUnregisterLater;

    for (int i = 0; i < operationList.count(); ++i) {
        Kit *newKit = newKits.at(i);

        const bool toPersist = operationList.at(i);
        const bool kitIsPersistent = kitIsPersistentList.at(i);
        const bool qtIsPersistent = qtIsPersistentList.at(i);

        DirectoryData *dd = testData.at(i);

        // Create a templateKit with the expected data:
        Kit *templateKit = nullptr;
        if (newKit == defaultKit) {
            templateKit = defaultKit;
        } else {
            templateKit = dd->kit->clone(true);
            QtKitAspect::setQtVersionId(templateKit, QtKitAspect::qtVersionId(newKit));
        }
        const QList<Utils::Id> templateKitKeys = templateKit->allKeys();

        if (newKit != defaultKit)
            toUnregisterLater.append(newKit);

        const Utils::Id newKitIdAfterImport = newKit->id();

        if (toPersist) {
            // --------------------------------------------------------------------
            // Test: persist kit
            // --------------------------------------------------------------------

            importer.makePersistent(newKit);
        } else {
            // --------------------------------------------------------------------
            // Test: cleanup kit
            // --------------------------------------------------------------------

            importer.cleanupKit(newKit);
        }

        const QList<Utils::Id> newKitKeys = newKit->allKeys();
        const Utils::Id newKitId = newKit->id();
        const int qtId = QtKitAspect::qtVersionId(newKit);

        // VALIDATE: Kit Id has not changed
        QCOMPARE(newKitId, newKitIdAfterImport);

        // VALIDATE: Importer state
        QCOMPARE(importer.projectFilePath(), tempDir1.path());
        QCOMPARE(importer.allDeleted(), true);

        if (kitIsPersistent) {
            // The kit was persistet. This can happen after makePersistent, but
            // cleanup can also end up here (provided the kit was persistet earlier
            // in the test run)

            // VALIDATE: All the kit values are as set up in the template before
            QCOMPARE(newKitKeys.count(), templateKitKeys.count());
            for (Utils::Id id : templateKitKeys) {
                if (id == QtKitAspect::id())
                    continue;
                QVERIFY(newKit->hasValue(id));
                QVERIFY(newKit->value(id) == templateKit->value(id));
            }

            // VALIDATE: DefaultKit is still visible in KitManager
            QVERIFY(KitManager::kit(newKit->id()));
        } else {
            // Validate that the kit was cleaned up.

            // VALIDATE: All keys that got added during import are gone
            QCOMPARE(newKitKeys.count(), templateKitKeys.count());
            for (Utils::Id id : newKitKeys) {
                if (id == QtKitAspect::id())
                    continue; // Will be checked by Qt version later
                QVERIFY(templateKit->hasValue(id));
                QVERIFY(newKit->value(id) == templateKit->value(id));
            }
        }

        if (qtIsPersistent) {
            // VALIDATE: Qt is used in the Kit:
            QVERIFY(QtKitAspect::qtVersionId(newKit) == qtId);

            // VALIDATE: Qt is still in QtVersionManager
            QVERIFY(QtVersionManager::version(qtId));

            // VALIDATE: Qt points to the expected qmake path:
            QCOMPARE(QtVersionManager::version(qtId)->qmakeFilePath(), dd->qmakePath);

            // VALIDATE: Kit uses the expected Qt
            QCOMPARE(QtKitAspect::qtVersionId(newKit), qtId);
        } else {
            // VALIDATE: Qt was reset in the kit
            QVERIFY(QtKitAspect::qtVersionId(newKit) == -1);

            // VALIDATE: New kit is still visible in KitManager
            QVERIFY(KitManager::kit(newKitId)); // Cleanup Kit does not unregister Kits, so it does
                                                // not matter here whether the kit is new or not.

            // VALIDATE: Qt was cleaned up (new Qt!)
            QVERIFY(!QtVersionManager::version(qtId));

            // VALIDATE: Qt version was reset on the kit
            QVERIFY(newKit->value(QtKitAspect::id()).toInt() == -1); // new Qt will be reset to invalid!
        }

        if (templateKit != defaultKit)
            delete templateKit;
    }

    // --------------------------------------------------------------------
    // Teardown:
    // --------------------------------------------------------------------

    qDeleteAll(testData);

    foreach (Kit *k, toUnregisterLater)
        KitManager::deregisterKit(k);

    // Delete kit templates:
    QVERIFY(kitTemplates.removeOne(defaultKit));
    qDeleteAll(kitTemplates);
}

} // namespace Internal
#endif // WITH_TESTS

} // namespace QtSupport