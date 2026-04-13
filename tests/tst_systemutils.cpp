#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include "system_utils.h"

class SystemUtilsTests : public QObject {
    Q_OBJECT

private slots:
    void testGetStorageMultiplier() {
        SystemUtils su;
        double mult = su.getStorageMultiplier();
#ifdef Q_OS_MAC
        QCOMPARE(mult, 1000.0);
#else
        QCOMPARE(mult, 1024.0);
#endif
    }

    void testFormatSize() {
        SystemUtils su;
        double mult = su.getStorageMultiplier();
        
        // 1 MB test
        QString mbSize = su.formatSize(mult * mult);
        QCOMPARE(mbSize, QString("1 MB"));
        
        // 1.5 GB test
        QString gbSize = su.formatSize(1.5 * mult * mult * mult);
        QCOMPARE(gbSize, QString("1.50 GB"));
    }

    void testCalculateCueRealSize() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        QString cuePath = dir.path() + "/game.cue";
        QString binPath1 = dir.path() + "/track1.bin";
        QString binPath2 = dir.path() + "/track2.bin";
        QString auxPath = dir.path() + "/game.sbi";

        // Create dummy files
        QFile bin1(binPath1);
        QVERIFY(bin1.open(QIODevice::WriteOnly));
        QByteArray dummyData1(1024, 'A');
        bin1.write(dummyData1);
        bin1.close();

        QFile bin2(binPath2);
        QVERIFY(bin2.open(QIODevice::WriteOnly));
        QByteArray dummyData2(512, 'B');
        bin2.write(dummyData2);
        bin2.close();
        
        QFile aux1(auxPath);
        QVERIFY(aux1.open(QIODevice::WriteOnly));
        QByteArray auxData(256, 'C');
        aux1.write(auxData);
        aux1.close();

        QFile cueFile(cuePath);
        QVERIFY(cueFile.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&cueFile);
        out << "FILE \"track1.bin\" BINARY\n";
        out << "  TRACK 01 MODE2/2352\n";
        out << "FILE \"track2.bin\" BINARY\n";
        out << "  TRACK 02 AUDIO\n";
        cueFile.close();

        QFileInfo cueInfo(cuePath);
        qint64 totalCalculated = SystemUtils::calculateCueRealSize(cueInfo);

        // Expected size = cue(bytes) + bin1(1024) + bin2(512) + sbi(256)
        qint64 expectedValue = cueInfo.size() + 1024 + 512 + 256;
        QCOMPARE(totalCalculated, expectedValue);

        // Also test duplicate bin reference prevention
        QFile badCue(dir.path() + "/bad.cue");
        QVERIFY(badCue.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream bout(&badCue);
        bout << "FILE \"track1.bin\" BINARY\n";
        bout << "FILE \"track1.bin\" BINARY\n"; // duplicate
        badCue.close();
        QFileInfo badInfo(badCue.fileName());
        
        // Expected size = cue + bin1 (no double counting)
        qint64 badExpectedValue = badInfo.size() + 1024;
        qint64 badCalculated = SystemUtils::calculateCueRealSize(badInfo);
        QCOMPARE(badCalculated, badExpectedValue);
    }
};

QTEST_MAIN(SystemUtilsTests)
#include "tst_systemutils.moc"
