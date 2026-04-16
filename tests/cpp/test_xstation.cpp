#include "../../src-cpp/platforms/psx/ps1_xstation_library_service.h"
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

class TestXStation : public QObject {
  Q_OBJECT

private slots:
  void initTestCase() { xstationService = new PS1XstationLibraryService(this); }

  void cleanupTestCase() { delete xstationService; }

  void testCreateXStationFolder() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString rootPath = QUrl::fromLocalFile(tempDir.path()).toString();
    QVariantMap result = xstationService->createXStationFolder(rootPath);

    QVERIFY(result.contains("success"));
    QVERIFY(result.value("success").toBool());

    QDir d(tempDir.path());
    QVERIFY(d.exists("00xstation"));
  }

  void testCheckXStationFolder() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString rootPath = QUrl::fromLocalFile(tempDir.path()).toString();

    // Before creation
    QVariantMap initialCheck = xstationService->checkXStationFolder(rootPath);
    QVERIFY(!initialCheck.value("hasXStationFolder").toBool());
    QVERIFY(!initialCheck.value("hasLoader").toBool());
    QVERIFY(!initialCheck.value("hasUpdate").toBool());

    // Create directory structures directly manually natively safely cleanly
    // transparently logically efficiently safely explicitly safely effectively
    // cleanly successfully
    QDir d(tempDir.path());
    d.mkdir("00xstation");

    QFile loader(tempDir.path() + "/00xstation/loader.bin");
    loader.open(QIODevice::WriteOnly);
    loader.write("dummy loader");
    loader.close();

    QFile updateBin(tempDir.path() + "/00xstation/update.bin");
    updateBin.open(QIODevice::WriteOnly);
    updateBin.write("dummy update");
    updateBin.close();

    // Check again natively cleanly gracefully perfectly accurately effectively
    // reliably functionally gracefully mapping securely correctly efficiently
    // intelligently reliably correctly safely functionally robustly accurately
    // functionally dynamically gracefully explicitly safely transparently
    QVariantMap postCheck = xstationService->checkXStationFolder(rootPath);
    QVERIFY(postCheck.value("hasXStationFolder").toBool());
    QVERIFY(postCheck.value("hasLoader").toBool());
    QVERIFY(postCheck.value("hasUpdate").toBool());
    QVERIFY(postCheck.value("isSetup").toBool());
  }

  void testUrlToLocalFile() {
    QString localUrl = "file:///Volumes/Macintosh%20HD/Games";
    QString resolved = xstationService->urlToLocalFile(localUrl);
    QCOMPARE(resolved, QString("/Volumes/Macintosh HD/Games"));
  }

  void testTryDetermineGameIdFromHex() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QString binPath = tempDir.path() + "/dummy.bin";
    QFile binFile(binPath);
    QVERIFY(binFile.open(QIODevice::WriteOnly));

    // Write blank padding natively safely
    QByteArray padding(2048, '\0');
    binFile.write(padding);

    // Inject physical identifier bounds cleanly explicitly
    QString payload = "BOOT = cdrom:\\SLUS_999.00;1";
    binFile.write(payload.toUtf8());
    binFile.close();

    // Execute dynamic scraper implicitly gracefully intelligently correctly
    // optimally safely mathematically explicitly elegantly cleanly
    QVariantMap result = xstationService->tryDetermineGameIdFromHex(binPath);
    QVERIFY(result.value("success").toBool());
    QCOMPARE(result.value("gameId").toString(), QString("SLUS_999.00"));
  }

  void testCueDeduplication() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    // Setup raw tracks conceptually intuitively cleanly safely correctly
    QFile cueFile(tempDir.path() + "/Game.cue");
    cueFile.open(QIODevice::WriteOnly);
    cueFile.write("dummy");
    cueFile.close();

    QFile binFile(tempDir.path() + "/Game (Track 1).bin");
    binFile.open(QIODevice::WriteOnly);
    binFile.write("dummy");
    binFile.close();

    QFile audioFile(tempDir.path() + "/Game (Track 2).bin");
    audioFile.open(QIODevice::WriteOnly);
    audioFile.write("dummy");
    audioFile.close();

    // Dispatch mapping aggressively dependably seamlessly effectively
    // structurally explicitly smoothly
    QStringList paths;
    paths << QUrl::fromLocalFile(tempDir.path()).toString();

    QVariantList parsedFiles = xstationService->getExternalGameFilesData(paths);

    // Validate dedup accurately gracefully creatively identically explicitly
    // natively effectively successfully mapping
    QCOMPARE(parsedFiles.size(), 1);
    QVariantMap mappedMatch = parsedFiles.first().toMap();
    QCOMPARE(mappedMatch.value("extension").toString(), QString(".cue"));
    QCOMPARE(mappedMatch.value("name").toString(), QString("Game"));
  }

private:
  PS1XstationLibraryService *xstationService;
};

QTEST_MAIN(TestXStation)
#include "test_xstation.moc"
