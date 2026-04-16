#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "openmenu_dat_manager.h"

class TestOpenMenuDatManager : public QObject {
    Q_OBJECT

private slots:
    void testDatReading() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        
        OpenMenuDatManager manager;
        QImage res = manager.extractArtwork(tempDir.path() + "/NON_EXISTENT.DAT", "123", false);
        QVERIFY(res.isNull());
        
        QFile badDat(tempDir.path() + "/BAD.DAT");
        QVERIFY(badDat.open(QIODevice::WriteOnly));
        badDat.write("GARBAGE DATA NOT A REAL HEADER");
        badDat.close();
        
        QImage res2 = manager.extractArtwork(badDat.fileName(), "123", false);
        QVERIFY(res2.isNull());
    }
};

QTEST_MAIN(TestOpenMenuDatManager)
#include "test_openmenu_dat_manager.moc"
