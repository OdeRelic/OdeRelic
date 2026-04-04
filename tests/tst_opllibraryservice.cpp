#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSignalSpy>
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
    
    // Inject the target regex ID "SLUS_123.45;1" at a predictable payload location (like offset ~1MB)
    QByteArray payload = "RANDOMDATA...SLUS_123.45;1...MOREGARBAGE";
    file.write(payload);
    
    file.close();

    QVariantMap result = m_service->tryDetermineGameIdFromHex(dummyIsoPath);
    QVERIFY(result["success"].toBool() == true);
    QCOMPARE(result["gameId"].toString(), QString("SLUS_123.45"));
    QCOMPARE(result["formattedGameId"].toString(), QString("SLUS-12345"));
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

QTEST_MAIN(TestOplLibraryService)
#include "tst_opllibraryservice.moc"
