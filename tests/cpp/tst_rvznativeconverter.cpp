#include <QtTest>
#include <QTemporaryFile>
#include <QSignalSpy>
#include "rvz_native_converter.h"

class TestRvzNativeConverter : public QObject
{
    Q_OBJECT

private slots:
    void testInvalidMagic();
    void testMissingSourceFile();
    void testValidRvzExtraction();
};

void TestRvzNativeConverter::testInvalidMagic()
{
    RvzNativeConverter converter;
    
    QTemporaryFile tempSrc;
    QVERIFY(tempSrc.open());
    tempSrc.write("BAD_MAGIC_HERE");
    tempSrc.flush();
    tempSrc.close();

    QTemporaryFile tempDst;
    tempDst.open();
    tempDst.close();

    QString outError;
    bool result = converter.convertRvzToIso(tempSrc.fileName(), tempDst.fileName(), outError);

    QVERIFY(!result);
    QCOMPARE(outError, QString("File is not a valid RVZ image (missing RVZ magic)."));
}

void TestRvzNativeConverter::testMissingSourceFile()
{
    RvzNativeConverter converter;
    QString outError;
    bool result = converter.convertRvzToIso("/some/fake/missing/path.rvz", "/tmp/dest.iso", outError);

    QVERIFY(!result);
    QCOMPARE(outError, QString("Native C++ bridge could not open source RVZ payload."));
}

void TestRvzNativeConverter::testValidRvzExtraction()
{
    RvzNativeConverter converter;
    QSignalSpy spy(&converter, &RvzNativeConverter::progressUpdated);
    
    QTemporaryFile tempSrc;
    QVERIFY(tempSrc.open());
    
    // Write valid RVZ magic + pad to 0x58
    tempSrc.write("RVZ\x01");
    // Pad to 0x58 (88 bytes total)
    tempSrc.write(QByteArray(84, '\0'));
    // Write gamecube header (1024 bytes)
    tempSrc.write(QByteArray(1024, 'A')); // Faux GC header
    tempSrc.flush();
    tempSrc.close();

    QTemporaryFile tempDst;
    tempDst.open();
    tempDst.close();
    
    QString outError;
    bool result = converter.convertRvzToIso(tempSrc.fileName(), tempDst.fileName(), outError);
    
    QVERIFY(result);
    QCOMPARE(outError, QString("RVZ decompression executed gracefully."));
    
    QVERIFY(spy.count() > 0); // Check if progressUpdated was emitted
}

QTEST_MAIN(TestRvzNativeConverter)
#include "tst_rvznativeconverter.moc"
