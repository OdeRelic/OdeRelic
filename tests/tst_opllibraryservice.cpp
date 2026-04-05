#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include "opl_library_service.h"

class TestOplLibraryService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testGameIdExtraction();
    void testIntraDirectoryRename();
    void testCrossDirectoryRename();
    void testCueFileParsing();
    
    void testMoveFile();
    void testDeleteLegacyBinFile();
    void testGetArtFolder();
    void testGetExternalGameFilesData();
    void testStartImportIsoAsync();
    void testStartConvertBinToIso();

    void testPs1GameIdExtraction();
    void testPopsDirectoryCreation();
    void testCueFileRealSizeCalculation();
    void testRecursiveScannerAndCueDeduplication();
    void testJsonMetadataCaching();

private:
    OplLibraryService* m_service;
    QTemporaryDir* m_tempDir;
};

void TestOplLibraryService::initTestCase()
{
    m_service = new OplLibraryService();
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
}

void TestOplLibraryService::cleanupTestCase()
{
    delete m_service;
    delete m_tempDir;
}

void TestOplLibraryService::testGameIdExtraction()
{
    // Write a dummy ISO file natively containing a fake PS2 header sector structure
    QString dummyIsoPath = m_tempDir->path() + "/dummy_game.iso";
    QFile file(dummyIsoPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    
    // Fill with random garbage
    QByteArray garbage(1024 * 1024, (char)0);
    file.write(garbage);
    
    // Inject the target regex ID "SLUS_123.45;1" alongside the Version natively inside typical SYSTEM.CNF chunk
    QByteArray payload = "BOOT2 = cdrom0:\\SLUS_123.45;1\nVER = 1.05\nVMODE = NTSC\n";
    file.write(payload);
    
    file.close();

    QVariantMap result = m_service->tryDetermineGameIdFromHex(dummyIsoPath);
    QVERIFY(result["success"].toBool() == true);
    QCOMPARE(result["gameId"].toString(), QString("SLUS_123.45"));
    QCOMPARE(result["formattedGameId"].toString(), QString("SLUS-12345"));
    QCOMPARE(result["version"].toString(), QString("1.05"));
}

void TestOplLibraryService::testIntraDirectoryRename()
{
    QString dvdPath = m_tempDir->path() + "/DVD";
    QDir().mkpath(dvdPath);
    
    // Create 'Game.iso' natively inside /DVD/
    QString originalPath = dvdPath + "/Game.iso";
    QFile file(originalPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("test");
    file.close();
    
    // With the new file extension-based rules, .iso always goes to DVD.
    // Let's use the initially created originalPath in /DVD/.
    QVariantMap result = m_service->renameGamefile(originalPath, m_tempDir->path(), "SLUS_999.99", "Game.iso");
    QVERIFY(result["success"].toBool() == true);
    
    QString expectedNewPath = dvdPath + "/SLUS_999.99.Game.iso.iso"; 
    QVERIFY(QFile::exists(expectedNewPath));
}

void TestOplLibraryService::testCrossDirectoryRename()
{
    QString rootPath = m_tempDir->path();
    
    // Place file in root folder explicitly
    QString originalPath = rootPath + "/CrossGame.iso";
    QFile file(originalPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("test");
    file.close();
    
    // Since the file is .iso, it implicitly bounds to DVD targeting natively
    QVariantMap result = m_service->renameGamefile(originalPath, rootPath, "SLES_123.45", "CrossGame");
    QVERIFY(result["success"].toBool() == true);
    
    QString expectedNewPath = rootPath + "/DVD/SLES_123.45.CrossGame.iso";
    QVERIFY(QFile::exists(expectedNewPath));
    QVERIFY(!QFile::exists(originalPath)); // Source absolutely must be purged dynamically
}

void TestOplLibraryService::testCueFileParsing()
{
    QString rootPath = m_tempDir->path();
    QString cuePath = rootPath + "/Game.cue";
    QString binPath = rootPath + "/Game_Track1.bin";
    
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write("FILE \"Game_Track1.bin\" BINARY\n");
    cueFile.write("  TRACK 01 MODE2/2352\n");
    cueFile.close();
    
    QFile binFile(binPath);
    QVERIFY(binFile.open(QIODevice::WriteOnly));
    binFile.write("binary data");
    binFile.close();
    
    QVariantMap res = m_service->deleteFileAndCue(cuePath);
    QVERIFY(res["success"].toBool() == true);
    
    QVERIFY(!QFile::exists(cuePath));
    QVERIFY(!QFile::exists(binPath));
}

void TestOplLibraryService::testMoveFile()
{
    QString rootPath = m_tempDir->path();
    QString sourcePath = rootPath + "/SourceFile.txt";
    QFile file(sourcePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("testdata");
    file.close();
    
    QString destDir = rootPath + "/TargetDir";
    QDir().mkpath(destDir);
    
    QVariantMap result = m_service->moveFile(sourcePath, destDir);
    QVERIFY(result["success"].toBool() == true);
    
    QString expectedNewPath = destDir + "/SourceFile.txt";
    QVERIFY(QFile::exists(expectedNewPath));
    QVERIFY(!QFile::exists(sourcePath));
}

void TestOplLibraryService::testDeleteLegacyBinFile()
{
    QString rootPath = m_tempDir->path();
    QString binPath = rootPath + "/Game.bin";
    QString cuePath = rootPath + "/Game.cue";
    QString cueUpperPath = rootPath + "/Game.CUE";
    
    QFile fileBin(binPath);
    QVERIFY(fileBin.open(QIODevice::WriteOnly)); fileBin.close();
    QFile fileCue(cuePath);
    QVERIFY(fileCue.open(QIODevice::WriteOnly)); fileCue.close();
    QFile fileCueUpper(cueUpperPath);
    QVERIFY(fileCueUpper.open(QIODevice::WriteOnly)); fileCueUpper.close();
    
    QVariantMap result = m_service->deleteFileAndCue(binPath);
    QVERIFY(result["success"].toBool() == true);
    
    QVERIFY(!QFile::exists(binPath));
    QVERIFY(!QFile::exists(cuePath));
    QVERIFY(!QFile::exists(cueUpperPath));
}

void TestOplLibraryService::testGetArtFolder()
{
    QString rootPath = m_tempDir->path();
    QString artDir = rootPath + "/ART";
    QDir().mkpath(artDir);
    
    QString imgPath = artDir + "/SLUS_123.45_COV.png";
    QFile file(imgPath);
    QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
    
    QVariantMap result = m_service->getArtFolder(rootPath);
    QVERIFY(result["success"].toBool() == true);
    
    QVariantList data = result["data"].toList();
    QVERIFY(data.size() == 1);
    
    QVariantMap item = data[0].toMap();
    QCOMPARE(item["name"].toString(), QString("SLUS_123.45_COV"));
    QCOMPARE(item["extension"].toString(), QString(".png"));
    QCOMPARE(item["gameId"].toString(), QString("SLUS_123.45"));
    QCOMPARE(item["type"].toString(), QString("COV"));
}

void TestOplLibraryService::testGetExternalGameFilesData()
{
    QString rootPath = m_tempDir->path();
    QString validIso = rootPath + "/SLUS_123.45.MyGame.iso";
    QFile file(validIso);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.close();
    
    QStringList urls;
    urls << validIso;
    
    QVariantList result = m_service->getExternalGameFilesData(urls);
    QVERIFY(result.size() == 1);
    
    QVariantMap item = result[0].toMap();
    QCOMPARE(item["extension"].toString(), QString(".iso"));
    QCOMPARE(item["name"].toString(), QString("SLUS_123.45.MyGame"));
    QVERIFY(item["isRenamed"].toBool() == true);
}

void TestOplLibraryService::testStartImportIsoAsync()
{
    QString rootPath = m_tempDir->path();
    QString sourceIso = rootPath + "/SourceGame.iso";
    
    QFile file(sourceIso);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray garbage(1024, 'A');
    file.write(garbage);
    file.close();
    
    QString libraryRoot = rootPath + "/Lib";
    QDir().mkpath(libraryRoot);
    
    QSignalSpy spyProgress(m_service, &OplLibraryService::importIsoProgress);
    QSignalSpy spyFinished(m_service, &OplLibraryService::importIsoFinished);
    
    m_service->startImportIsoAsync(sourceIso, libraryRoot, "SLUS_999.99", "AsyncGame", false);
    
    QVERIFY(spyFinished.wait(10000));
    
    QVERIFY(spyFinished.count() == 1);
    QList<QVariant> arguments = spyFinished.takeFirst();
    QVERIFY(arguments.at(1).toBool() == true); // success
    
    QString destIso = arguments.at(2).toString();
    QCOMPARE(destIso, libraryRoot + "/DVD/SLUS_999.99.AsyncGame.iso");
    QVERIFY(QFile::exists(destIso));
    QVERIFY(!QFile::exists(sourceIso)); 
}

void TestOplLibraryService::testStartConvertBinToIso()
{
    QString rootPath = m_tempDir->path();
    QString sourceBin = rootPath + "/Track.bin";
    
    QFile file(sourceBin);
    QVERIFY(file.open(QIODevice::WriteOnly));
    
    const qint64 RAW_SECTOR_SIZE = 2352;
    const qint64 ISO_SECTOR_SIZE = 2048;
    const qint64 SECTOR_HEADER_OFFSET = 24;
    
    QByteArray sector(RAW_SECTOR_SIZE, 'A');
    for(int i = 0; i < ISO_SECTOR_SIZE; i++) {
        sector[SECTOR_HEADER_OFFSET + i] = 'B'; // The payload
    }
    
    for(int i = 0; i < 10; i++) {
        file.write(sector);
    }
    file.close();
    
    QString destIso = rootPath + "/Converted.iso";
    
    QSignalSpy spyProgress(m_service, &OplLibraryService::conversionProgress);
    QSignalSpy spyFinished(m_service, &OplLibraryService::conversionFinished);
    
    m_service->startConvertBinToIso(sourceBin, destIso);
    
    QVERIFY(spyFinished.wait(5000));
    
    QVERIFY(spyFinished.count() == 1);
    QList<QVariant> arguments = spyFinished.takeFirst();
    QVERIFY(arguments.at(1).toBool() == true); // success
    
    QVERIFY(QFile::exists(destIso));
    QFile isoFile(destIso);
    QVERIFY(isoFile.open(QIODevice::ReadOnly));
    QVERIFY(isoFile.size() == 10 * ISO_SECTOR_SIZE);
    
    QByteArray isoData = isoFile.readAll();
    for (char c : isoData) {
        QVERIFY(c == 'B');
    }
}

void TestOplLibraryService::testPs1GameIdExtraction()
{
    QString dummyVcdPath = m_tempDir->path() + "/dummy_ps1.vcd";
    QFile file(dummyVcdPath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    
    QByteArray garbage(2048, (char)0);
    file.write(garbage);
    
    // Inject the PS1 target regex ID explicitly using the BOOT parameter priority alongside its SYSTEM.CNF VER layer
    QByteArray payload = "GARBAGE... BOOT=cdrom:\\SLES_888.88;1 \n VER = 2.01 \n ...GARBAGE";
    file.write(payload);
    
    file.close();

    QVariantMap result = m_service->tryDeterminePs1GameIdFromHex(dummyVcdPath);
    QVERIFY(result["success"].toBool() == true);
    QCOMPARE(result["gameId"].toString(), QString("SLES_888.88"));
    QCOMPARE(result["formattedGameId"].toString(), QString("SLES-88888"));
    QCOMPARE(result["version"].toString(), QString("2.01"));
}

void TestOplLibraryService::testPopsDirectoryCreation()
{
    QString baseLibrary = m_tempDir->path() + "/NewLib";
    QDir().mkpath(baseLibrary);
    
    QVERIFY(!QDir(baseLibrary + "/POPS").exists());
    
    QVariantMap res1 = m_service->checkPopsFolder(baseLibrary);
    QVERIFY(res1["hasPopsFolder"].toBool() == false);
    
    QDir().mkpath(baseLibrary + "/POPS");
    QVariantMap res2 = m_service->checkPopsFolder(baseLibrary);
    QVERIFY(res2["hasPopsFolder"].toBool() == true);
}

void TestOplLibraryService::testCueFileRealSizeCalculation()
{
    QString rootPath = m_tempDir->path() + "/SizesTest";
    QDir().mkpath(rootPath);
    
    QString cuePath = rootPath + "/MultiTrack.cue";
    QString bin1Path = rootPath + "/MultiTrack_Track1.bin";
    QString bin2Path = rootPath + "/MultiTrack_Track2.bin";
    
    QFile bin1(bin1Path);
    QVERIFY(bin1.open(QIODevice::WriteOnly));
    QByteArray data1(1024, 'A');
    bin1.write(data1);
    bin1.close();
    
    QFile bin2(bin2Path);
    QVERIFY(bin2.open(QIODevice::WriteOnly));
    QByteArray data2(512, 'B');
    bin2.write(data2);
    bin2.close();
    
    QFile cueFile(cuePath);
    QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
    cueFile.write("FILE \"MultiTrack_Track1.bin\" BINARY\n");
    cueFile.write("  TRACK 01 MODE2/2352\n");
    cueFile.write("FILE \"MultiTrack_Track2.bin\" BINARY\n");
    cueFile.write("  TRACK 02 AUDIO\n");
    cueFile.close();
    
    
    QStringList paths;
    paths << rootPath;
    QVariantList items = m_service->getExternalPs1FilesData(paths);
    
    QVERIFY(items.size() == 1); // Ensures the bins were successfully compressed abstractly
    QVariantMap fileData = items[0].toMap();
    
    // The "stats" sub-object carries the physical size payload
    QVariantMap stats = fileData["stats"].toMap();
    qint64 totalSize = stats["size"].toLongLong();
    
    QVERIFY(totalSize == (1024 + 512));
}

void TestOplLibraryService::testJsonMetadataCaching()
{
    QString rootPath = m_tempDir->path() + "/CacheTest";
    QDir().mkpath(rootPath + "/DVD");
    
    // Create a dummy game
    QString dummyIso = rootPath + "/DVD/SLUS_111.11.CacheGame.iso";
    QFile file(dummyIso);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QByteArray payload = "BOOT2 = cdrom0:\\SLUS_111.11;1\nVER = 3.99\n";
    file.write(payload);
    file.close();
    
    // Assert cache doesn't exist yet
    QString cachePath = rootPath + "/.ps2_oderelic_cache.json";
    QVERIFY(!QFile::exists(cachePath));
    
    QSignalSpy spyLoaded(m_service, &OplLibraryService::gamesFilesLoaded);
    
    // Trigger initial scan (Cold Boot)
    m_service->startGetGamesFilesAsync(rootPath);
    QVERIFY(spyLoaded.wait(5000));
    
    // Verify cache was explicitly generated by the backend thread
    QVERIFY(QFile::exists(cachePath));
    
    QFile cache(cachePath);
    QVERIFY(cache.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(cache.readAll());
    cache.close();
    
    QVERIFY(doc.isObject());
    QJsonObject obj = doc.object();
    QVERIFY(obj.contains("SLUS_111.11.CacheGame.iso"));
    
    QJsonObject item = obj["SLUS_111.11.CacheGame.iso"].toObject();
    QVERIFY(item.contains("version"));
    QCOMPARE(item["version"].toString(), QString("3.99"));
}

void TestOplLibraryService::testRecursiveScannerAndCueDeduplication()
{
    QString rootPath = m_tempDir->path() + "/DeepFolder";
    QDir().mkpath(rootPath + "/Nested1");
    QDir().mkpath(rootPath + "/Nested2");
    
    // Nested1 gets a purely standalone .img file
    QString imgPath = rootPath + "/Nested1/StandaloneGame.img";
    QFile fileImg(imgPath);
    QVERIFY(fileImg.open(QIODevice::WriteOnly)); fileImg.close();
    
    // Nested2 gets a .cue, .bin, and an unrelated .vcd 
    QString cuePath = rootPath + "/Nested2/LinkedGame.cue";
    QString binPath = rootPath + "/Nested2/LinkedGame_Track1.bin";
    QString noiseVcd = rootPath + "/Nested2/UnrelatedGame.vcd";
    
    QFile fileCue(cuePath); QVERIFY(fileCue.open(QIODevice::WriteOnly)); fileCue.close();
    QFile fileBin(binPath); QVERIFY(fileBin.open(QIODevice::WriteOnly)); fileBin.close();
    QFile fileNoiseVcd(noiseVcd); QVERIFY(fileNoiseVcd.open(QIODevice::WriteOnly)); fileNoiseVcd.close();
    
    QStringList pathsToScan;
    pathsToScan << rootPath;
    
    QVariantList result = m_service->getExternalPs1FilesData(pathsToScan);
    
    // We expect:
    // 1 from Nested1 (StandaloneGame.img)
    // 2 from Nested2 (LinkedGame.cue, UnrelatedGame.vcd) -- the .bin should be actively deduplicated and silently dropped!
    QVERIFY(result.size() == 3);
    
    bool foundBin = false;
    for (const QVariant& itemVariant : result) {
        QVariantMap item = itemVariant.toMap();
        if (item["extension"].toString() == ".bin") foundBin = true;
    }
    QVERIFY(foundBin == false); // Asserts successful CUE abstraction logic completely filtering .bin files dynamically
}


QTEST_MAIN(TestOplLibraryService)
#include "tst_opllibraryservice.moc"
