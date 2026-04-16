#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "dreamcast_iso_builder.h"

class TestDreamcastIsoBuilder : public QObject {
    Q_OBJECT

private slots:
    void testLbaOverrides() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString sourceDir = tempDir.path() + "/data";
        QDir().mkpath(sourceDir);
        
        QFile file1(sourceDir + "/FILE1.TXT");
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write(QByteArray(1024, 'A'));
        file1.close();
        
        DreamcastIsoBuilder builder;
        builder.setBaseLba(45000);
        
        // Override FILE1.TXT to start at a high LBA physically
        builder.overrideFileLba(file1.fileName(), 500000);
        
        QString outIso = tempDir.path() + "/out.iso";
        QVERIFY(builder.build(sourceDir, outIso, ""));
        
        // Verify ISO exists and size is relatively small (no payload inside, just PVD/Records)
        QFileInfo isoInfo(outIso);
        QVERIFY(isoInfo.exists());
        // Since we explicitly overrode the LBA, the ISO builder drops the payload and only authors headers.
        // Expect small ISO (~40-70KB)
        QVERIFY(isoInfo.size() < 100 * 1024);
    }
    
    void testDatInjection() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        QString sourceDir = tempDir.path() + "/data";
        QDir().mkpath(sourceDir);
        
        DreamcastIsoBuilder builder;
        builder.setBaseLba(45000);
        
        // Inject a virtual database file into the root of the ISO
        builder.injectFile("BOX.DAT", 600000, 1024 * 1024 * 50); // 50MB virtual payload at LBA 600000
        
        QString outIso = tempDir.path() + "/out.iso";
        QVERIFY(builder.build(sourceDir, outIso, ""));
        
        QFileInfo isoInfo(outIso);
        QVERIFY(isoInfo.exists());
        
        // The physical size of out.iso should remain small, because we only injected the metadata record, 
        // not the 50MB payload.
        QVERIFY(isoInfo.size() < 100 * 1024);
    }
};

QTEST_MAIN(TestDreamcastIsoBuilder)
#include "test_dreamcast_iso_builder.moc"
