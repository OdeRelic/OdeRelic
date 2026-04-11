#include <QtTest>
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include "swiss_library_service.h"

class SwissLibraryServiceTests : public QObject {
    Q_OBJECT

private slots:
    void test_urlToLocalFile();
    void test_tryDetermineGameIdFromHex();
    void test_swissFolderCreation();
    void test_swissFolderValidation();
    void test_renameGamefile_preservesSourceFile();
    void test_syncCheats();
};

void SwissLibraryServiceTests::test_urlToLocalFile() {
    SwissLibraryService service;
    
    // Valid standard formatted paths
    QString result1 = service.urlToLocalFile("file:///tmp/OdeRelic");
    QCOMPARE(result1, QString("/tmp/OdeRelic"));
    
    // Unformatted strings fallback securely
    QString result2 = service.urlToLocalFile("/home/user/Data");
    QCOMPARE(result2, QString("/home/user/Data"));
}

void SwissLibraryServiceTests::test_tryDetermineGameIdFromHex() {
    SwissLibraryService service;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    QString filePath = tempDir.path() + "/dummy.iso";
    QFile file(filePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    
    // Gamecube games feature a 6-byte ascii root ID signature
    QByteArray signature("GMPE01"); 
    file.write(signature);
    file.write(QByteArray(100, '\0')); // Payload noise
    file.close();
    
    QVariantMap res = service.tryDetermineGameIdFromHex(filePath);
    QVERIFY(res["success"].toBool() == true);
    QCOMPARE(res["gameId"].toString(), QString("GMPE01"));
}

void SwissLibraryServiceTests::test_swissFolderCreation() {
    SwissLibraryService service;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    
    QString root = tempDir.path();
    QVariantMap res = service.createSwissFolder(root);
    
    QVERIFY(res["success"].toBool() == true);
    QVERIFY(QDir(root).exists("games"));
    QVERIFY(QDir(root).exists("apps"));
    QVERIFY(QDir(root).exists("dol"));
    QVERIFY(QDir(root).exists("swiss"));
}

void SwissLibraryServiceTests::test_swissFolderValidation() {
    SwissLibraryService service;
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString root = tempDir.path();
    
    // Initial: Missing everything
    QVariantMap res1 = service.checkSwissFolder(root);
    QVERIFY(res1["isValid"].toBool() == false);
    
    // Scaffold structural paths
    QDir d(root);
    d.mkdir("games");
    d.mkdir("apps");
    d.mkdir("dol");
    d.mkdir("swiss");
    
    // Missing payload binaries entirely
    QVariantMap res2 = service.checkSwissFolder(root);
    QVERIFY(res2["isValid"].toBool() == false);
    
    // Inject a Swiss runtime binary mask
    QFile file(root + "/ipl.dol");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("dummy");
    file.close();
    
    QVariantMap res3 = service.checkSwissFolder(root);
    QVERIFY(res3["isValid"].toBool() == true);
    QVERIFY(res3["hasGames"].toBool() == true);
    QVERIFY(res3["hasDol"].toBool() == true);
    QVERIFY(res3["hasSwiss"].toBool() == true);
    
    // Assert structural properties deployed for security checks exist natively in QVariant payload
    QVERIFY(res3.contains("isFormatCorrect"));
    QVERIFY(res3.contains("isPartitionCorrect"));
}

void SwissLibraryServiceTests::test_renameGamefile_preservesSourceFile() {
    SwissLibraryService service;
    QTemporaryDir tempSourceDir;
    QTemporaryDir tempDestDir;
    QVERIFY(tempSourceDir.isValid());
    QVERIFY(tempDestDir.isValid());

    QString sourceIso = tempSourceDir.path() + "/mock_source.iso";
    QFile file(sourceIso);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("mock_payload");
    file.close();

    QString destLib = tempDestDir.path();
    
    QVariantMap res = service.renameGamefile(sourceIso, destLib, "GNPE01", "Mock Game");
    QVERIFY(res["success"].toBool() == true);
    
    // Validate Preservation
    QVERIFY(QFile::exists(sourceIso) == true);
    
    // Validate Destination format
    QString expectedTarget = destLib + "/games/Mock Game [GNPE01]/game.iso";
    QVERIFY(QFile::exists(expectedTarget) == true);
}

void SwissLibraryServiceTests::test_syncCheats() {
    SwissLibraryService service;
    QTemporaryDir tempDestDir;
    QVERIFY(tempDestDir.isValid());
    QString destLib = tempDestDir.path();
    
    // Scaffold Mock Library Structure
    QDir().mkpath(destLib + "/games/Mock Game [XYZE01]");
    
    // Scaffold Mock Cheat Database in application executable fallback path
    QString localCheatDir = QDir::currentPath() + "/assets/cheats/gamecube - wii";
    QDir().mkpath(localCheatDir);
    
    QString mockCheatFile = localCheatDir + "/XYZE01.txt";
    QFile cheatFile(mockCheatFile);
    QVERIFY(cheatFile.open(QIODevice::WriteOnly));
    cheatFile.write("MOCK_CHEAT_DATA");
    cheatFile.close();
    
    // Perform test
    int syncCount = service.syncCheats(destLib);
    QCOMPARE(syncCount, 1);
    
    // Verify results
    QString expectedTarget = destLib + "/cheats/XYZE01.txt";
    QVERIFY(QFile::exists(expectedTarget) == true);
    
    // Cleanup mock asset structure to not pollute workspace
    QFile::remove(mockCheatFile);
    QDir().rmdir(localCheatDir);
}

QTEST_MAIN(SwissLibraryServiceTests)
#include "tst_swisslibraryservice.moc"
