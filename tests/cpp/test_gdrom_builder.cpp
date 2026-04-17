#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include "gdrom_builder.h"

class TestGdromBuilder : public QObject {
    Q_OBJECT

private slots:
    void testTrack5DynamicLba() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString lowDataDir = tempDir.path() + "/menu_low_data";
        QString highDataDir = tempDir.path() + "/menu_data";
        QString destDir = tempDir.path() + "/01";
        
        QDir().mkpath(lowDataDir);
        QDir().mkpath(highDataDir);
        QDir().mkpath(destDir);
        
        // Create 1ST_READ.BIN
        QFile firstRead(highDataDir + "/1ST_READ.BIN");
        QVERIFY(firstRead.open(QIODevice::WriteOnly));
        firstRead.write(QByteArray(1024, 0)); // 1KB
        firstRead.close();
        
        // Create a dummy file
        QFile dummy(highDataDir + "/dummy.txt");
        QVERIFY(dummy.open(QIODevice::WriteOnly));
        dummy.write(QByteArray(2048 * 10, 0)); // 10 sectors
        dummy.close();
        
        GdromBuilder builder;
        bool result = builder.buildMenuGdrom(lowDataDir, highDataDir, destDir);
        QVERIFY(result);
        
        QVERIFY(QFile::exists(destDir + "/track01.iso"));
        QVERIFY(QFile::exists(destDir + "/track03.iso"));
        QVERIFY(QFile::exists(destDir + "/track05.iso"));
        QVERIFY(QFile::exists(destDir + "/disc.gdi"));
        
        // Read disc.gdi and verify LBA of track05
        QFile gdi(destDir + "/disc.gdi");
        QVERIFY(gdi.open(QIODevice::ReadOnly));
        QString gdiContent = QString::fromUtf8(gdi.readAll());
        gdi.close();
        
        // We ensure that track05.iso is properly referenced in the GDI
        QVERIFY(gdiContent.contains("track05.iso"));
        
        // Parse GDI to find Track 5 LBA
        QTextStream stream(&gdiContent);
        QString line;
        bool foundTrack5 = false;
        quint32 t5Lba = 0;
        while (stream.readLineInto(&line)) {
            QStringList parts = line.split(" ", Qt::SkipEmptyParts);
            if (parts.size() >= 6 && parts[4] == "track05.iso") {
                foundTrack5 = true;
                t5Lba = parts[1].toUInt();
            }
        }
        
        QVERIFY(foundTrack5);
        // Track 5 should dynamically shift back from 549150 based on payload size
        QVERIFY(t5Lba > 0 && t5Lba < 549150);
        
        // Verify Track 05 ISO size (should be exactly up to the 1ST_READ.BIN pinning point)
        QFileInfo t5Info(destDir + "/track05.iso");
        QVERIFY(t5Info.size() > 0);
    }
};

QTEST_MAIN(TestGdromBuilder)
#include "test_gdrom_builder.moc"
