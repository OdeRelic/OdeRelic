#include <QtTest>
#include <QImage>
#include <QDataStream>
#include <QBuffer>
#include "pvr_texture_coder.h"

class TestPvrTextureCoder : public QObject {
    Q_OBJECT

private slots:
    void testDecode() {
        // Construct a raw mock PVR payload manually to test the decoder
        QByteArray pvrPayload;
        QBuffer buffer(&pvrPayload);
        buffer.open(QIODevice::WriteOnly);
        QDataStream stream(&buffer);
        stream.setByteOrder(QDataStream::LittleEndian);

        buffer.write("GBIX", 4);
        stream << (uint32_t)8;
        buffer.write("\0\0\0\0\0\0\0\0", 8);

        buffer.write("PVRT", 4);
        stream << (uint32_t)16; // length of payload? No, usually size of pixel data or similar, but decoder ignores it mostly
        stream << (uint8_t)1; // pixelFormat = 1 (RGB565)
        stream << (uint8_t)1; // dataFormat = 1 (SquareTwiddled)
        
        buffer.write("\0\0", 2); // padding
        stream << (uint16_t)16; // width
        stream << (uint16_t)16; // height

        // Raw pixel data: 16x16 RGB565 = 16*16*2 bytes
        buffer.write(QByteArray(16 * 16 * 2, (char)0xFF)); // Fill with white
        buffer.close();

        PvrTextureCoder coder;
        QImage decoded = coder.decode(pvrPayload);

        QVERIFY(!decoded.isNull());
        QCOMPARE(decoded.width(), 16);
        QCOMPARE(decoded.height(), 16);
        
        // Ensure pixel decode extracted colors properly
        QColor pixel = decoded.pixelColor(0, 0);
        QVERIFY(pixel.red() > 240); // 0xFFFF in RGB565 is full white
    }
    
    void testInvalidDecode() {
        PvrTextureCoder coder;
        QImage decoded = coder.decode(QByteArray("TRASH"));
        QVERIFY(decoded.isNull());
    }
};

QTEST_MAIN(TestPvrTextureCoder)
#include "test_pvr_texture_coder.moc"
