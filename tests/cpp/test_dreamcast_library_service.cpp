#include <QTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QFile>
#include <QProcess>
#include "../../src-cpp/platforms/dreamcast/dreamcast_library_service.h"
#include "../../src-cpp/platforms/dreamcast/ArtNDataUtils/openmenu_dat_manager.h"

class TestDreamcastLibraryService : public QObject {
    Q_OBJECT

private slots:
    void testCheckDreamcastFolder();
    void testParseMetadataFromMedia();
    void testStartGetGamesFilesAsync();
    void testStartImportGameAsync();
    void testCommitLibraryOrderAsync();
    void testFileSystemCacheUsage();
    void testOpenMenuParser();
    void testMultiDiscImportSorting();
    void testMultiDiscLibraryAggregation();
    void testBuildAndDeployMenuGdromTransientAssets();
};

void TestDreamcastLibraryService::testCheckDreamcastFolder() {
    DreamcastLibraryService service;
    QTemporaryDir dir;
    QVariantMap res = service.checkDreamcastFolder(dir.path());
    // Fallback checks because temp dir is usually APFS/EXT4
    QVERIFY(res.contains("isFormatCorrect"));
}

void TestDreamcastLibraryService::testParseMetadataFromMedia() {
    DreamcastLibraryService service;
    QTemporaryDir dir;
    
    // Helper lambda to test a specific game ID
    auto testGameId = [&](const QString& gameId, const QString& expectedRegion) {
        QString path = dir.path() + "/dummy_" + gameId + ".ip.bin";
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        QByteArray data(1024 * 10, ' ');
        // Inject Sega Segakatana at offset 0
        memcpy(data.data(), "SEGA SEGAKATANA", 15);
        memcpy(data.data() + 0x80, "SONIC", 5);
        
        QByteArray gidBytes = gameId.toLatin1();
        memcpy(data.data() + 0x40, gidBytes.data(), qMin((int)gidBytes.size(), 10));
        f.write(data);
        f.close();
        
        QVariantMap meta = service.tryDetermineGameIdFromHex(path);
        QVERIFY(meta["success"].toBool() == true);
        QVERIFY(meta["title"].toString() == "SONIC");
        QVERIFY(meta["gameId"].toString() == gameId);
        QCOMPARE(meta["region"].toString(), expectedRegion);
    };

    testGameId("T-12345M", "NTSC-U");
    testGameId("T-12345N", "NTSC-U");
    testGameId("MK-51000", "NTSC-U");
    testGameId("T-1234E", "PAL");
    testGameId("MK-123-50", "PAL");
    testGameId("T-12345J", "NTSC-J");
    testGameId("HDR-0118", "NTSC-J");
}

void TestDreamcastLibraryService::testStartGetGamesFilesAsync() {
    DreamcastLibraryService service;
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("02");
    QFile f(dir.path() + "/02/disc.gdi"); f.open(QIODevice::WriteOnly); f.write("dummy"); f.close();
    
    QSignalSpy spy(&service, SIGNAL(gamesFilesLoaded(QString, QVariantMap)));
    service.startGetGamesFilesAsync(dir.path());
    spy.wait(2000);
    QCOMPARE(spy.count(), 1);
    
    QVariantMap result = spy.takeFirst().at(1).toMap();
    QVERIFY(result.contains("data"));
    
    QVariantList dataList = result["data"].toList();
    QCOMPARE(dataList.size(), 1);
    
    QVariantMap item = dataList.first().toMap();
    QVERIFY(item.contains("isRenamed"));
    QCOMPARE(item["isRenamed"].toBool(), true); // GDEMU folders are cleanly structurally mapped
    
    QVERIFY(item.contains("extension"));
    QCOMPARE(item["extension"].toString(), ".gdi"); // Extension safely inferred
}

void TestDreamcastLibraryService::testStartImportGameAsync() {
    DreamcastLibraryService service;
    QTemporaryDir src, dst;
    QFile sf(src.path() + "/game.cdi"); sf.open(QIODevice::WriteOnly); sf.write("data"); sf.close();
    
    QSignalSpy spy(&service, SIGNAL(importFinished(QString, bool, QString, QString)));
    service.startImportGameAsync(QStringList() << sf.fileName(), dst.path());
    spy.wait(2000);
    QVERIFY(spy.count() >= 1);
    QVERIFY(QFile::exists(dst.path() + "/02/game.cdi"));
    
    QList<QVariant> args = spy.takeFirst();
    QCOMPARE(args.at(1).toBool(), true);
    
    QString targetPath = args.at(2).toString();
    QVERIFY(targetPath.endsWith("/02")); // The physically mapped dest path must track the slot isolating the boxart bounds.
}

void TestDreamcastLibraryService::testCommitLibraryOrderAsync() {
    DreamcastLibraryService service;
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("05");
    QFile(dir.path() + "/05/disc.gdi").open(QIODevice::WriteOnly);
    
    QSignalSpy spy(&service, SIGNAL(reorderFinished(bool, QString)));
    service.commitLibraryOrderAsync(dir.path(), QStringList() << (dir.path() + "/05"));
    spy.wait(2000);
    QCOMPARE(spy.count(), 1);
    QVERIFY(QFile::exists(dir.path() + "/02/disc.gdi"));
}

void TestDreamcastLibraryService::testFileSystemCacheUsage() {
    DreamcastLibraryService service;
    QTemporaryDir dir;
    QDir(dir.path()).mkpath("02");
    QFile f(dir.path() + "/02/disc.gdi"); f.open(QIODevice::WriteOnly); f.write("dummy data here for sizing"); f.close();
    
    QSignalSpy spy(&service, SIGNAL(gamesFilesLoaded(QString, QVariantMap)));
    
    // First run (no cache)
    service.startGetGamesFilesAsync(dir.path());
    spy.wait(2000);
    
    // Validate cache is dumped correctly to the root path using the isolated namespace prefix natively
    QString cachePath = dir.path() + "/.dc_oderelic_cache.json";
    QVERIFY(QFile::exists(cachePath));
}

void TestDreamcastLibraryService::testOpenMenuParser() {
    OpenMenuDatManager datManager;
    QTemporaryDir dir;
    QString datPath = dir.path() + "/BOX.DAT";

    QFile f(datPath);
    QVERIFY(f.open(QIODevice::WriteOnly));
    QDataStream stream(&f);
    stream.setByteOrder(QDataStream::LittleEndian);

    f.write("DAT\x01", 4);
    stream << (uint32_t)131104;  // Unknown/SlotSize component? 0x00020020
    stream << (uint32_t)2;       // numEntries
    stream << (uint32_t)0;       // Buffer padding aligning to offset 16

    // Index mappings
    char buf1[12] = "TARGET_ID\0\0";
    f.write(buf1, 12);
    stream << (uint32_t)1;       // PVR slot 1

    char buf2[12] = "DUMMY_ID\0\0\0";
    f.write(buf2, 12);
    stream << (uint32_t)2;       // PVR slot 2
    
    // Fill remainder of slot 0 to the 131104 table padded ceiling
    f.write(QByteArray(131104 - f.pos(), 0));

    // Fill slot 1 with valid PVRT mockup payload (offset 131104) -> Width: 16 Height: 16
    f.write("GBIX", 4);
    stream << (uint32_t)8;
    f.write("\0\0\0\0\0\0\0\0", 8);
    
    f.write("PVRT", 4);
    stream << (uint32_t)16;
    stream << (uint8_t)1 << (uint8_t)1; // RGB565 Twiddled
    stream << (uint16_t)0;
    stream << (uint16_t)16 << (uint16_t)16;
    f.write(QByteArray(16 * 16 * 2, (char)0xFF)); // Fill with white
    
    f.close();

    // Verify index mapping routing extracts valid QImage
    QImage result = datManager.extractArtwork(datPath, "TARGET_ID", false);
    QVERIFY(!result.isNull());
    QCOMPARE(result.width(), 16);
    QCOMPARE(result.height(), 16);
}

void TestDreamcastLibraryService::testMultiDiscImportSorting() {
    DreamcastLibraryService service;
    QTemporaryDir src, dst;
    
    QFile f2(src.path() + "/Alone in the Dark - Disc 2.gdi"); f2.open(QIODevice::WriteOnly); f2.write("disc2"); f2.close();
    QFile f1(src.path() + "/Alone in the Dark - Disc 1.gdi"); f1.open(QIODevice::WriteOnly); f1.write("disc1"); f1.close();

    QStringList inputPaths = {src.path() + "/Alone in the Dark - Disc 2.gdi", src.path() + "/Alone in the Dark - Disc 1.gdi"};
    
    QSignalSpy spy(&service, SIGNAL(importFinished(QString, bool, QString, QString)));
    service.startImportGameAsync(inputPaths, dst.path());
    
    spy.wait(4000);
    
    QVERIFY(QFile::exists(dst.path() + "/02/Alone in the Dark - Disc 1.gdi"));
    QVERIFY(QFile::exists(dst.path() + "/03/Alone in the Dark - Disc 2.gdi"));
}

void TestDreamcastLibraryService::testMultiDiscLibraryAggregation() {
    DreamcastLibraryService service;
    QTemporaryDir dir;
    
    auto writeDummyWithGameId = [&](const QString& path, const QString& gameId) {
        QFile f(path);
        f.open(QIODevice::WriteOnly);
        QByteArray data(1024 * 10, ' ');
        memcpy(data.data(), "SEGA SEGAKATANA", 15);
        memcpy(data.data() + 0x80, "ALONE IN DARK", 13);
        QByteArray gidBytes = gameId.toLatin1();
        memcpy(data.data() + 0x40, gidBytes.data(), qMin((int)gidBytes.size(), 10));
        f.write(data);
        f.close();
    };

    QDir(dir.path()).mkpath("02");
    writeDummyWithGameId(dir.path() + "/02/disc.gdi", "T-12345M");
    
    QDir(dir.path()).mkpath("03");
    writeDummyWithGameId(dir.path() + "/03/disc.gdi", "T-12345M");

    QSignalSpy spy(&service, SIGNAL(gamesFilesLoaded(QString, QVariantMap)));
    service.startGetGamesFilesAsync(dir.path());
    spy.wait(4000);
    
    QCOMPARE(spy.count(), 1);
    QVariantMap result = spy.takeFirst().at(1).toMap();
    QVariantList dataList = result["data"].toList();
    
    QCOMPARE(dataList.size(), 1); 
    
    QVariantMap group = dataList.first().toMap();
    QVERIFY(group.contains("discs"));
    QVariantList discs = group["discs"].toList();
    QCOMPARE(discs.size(), 2);
}

void TestDreamcastLibraryService::testBuildAndDeployMenuGdromTransientAssets() {
    DreamcastLibraryService service;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QString rootPath = dir.path();
    
    // First run (e.g. menu generation)
    bool result = service.buildAndDeployMenuGdrom(rootPath);
    QVERIFY(result);
    
    // Ensure track01, track03, track05 were created
    QVERIFY(QFile::exists(rootPath + "/01/track01.iso"));
    QVERIFY(QFile::exists(rootPath + "/01/track03.iso"));
    QVERIFY(QFile::exists(rootPath + "/01/track05.iso"));
    
    // Ensure transient files were nuked from menu_data on FAT32
    QVERIFY(!QFile::exists(rootPath + "/01/menu_data/1ST_READ.BIN"));
    QVERIFY(!QDir(rootPath + "/01/menu_data/theme").exists());
    QVERIFY(!QFile::exists(rootPath + "/01/menu_data/OPENMENU.INI"));
    
    // Second run (simulate adding a game)
    // The ISO builder should re-copy the base assets from QRC and successfully rebuild
    bool result2 = service.buildAndDeployMenuGdrom(rootPath);
    QVERIFY(result2);
    
    // If the QRC assets were present, track03.iso would be > 60KB. 
    // In the test runner environment without QRC compiled, it will only contain OPENMENU.INI (~43KB).
    // We check that track03 is successfully generated and holds the bare minimum valid ISO geometry.
    QFileInfo t03Info(rootPath + "/01/track03.iso");
    QVERIFY(t03Info.size() > 30000); 
}

QTEST_MAIN(TestDreamcastLibraryService)
#include "test_dreamcast_library_service.moc"
