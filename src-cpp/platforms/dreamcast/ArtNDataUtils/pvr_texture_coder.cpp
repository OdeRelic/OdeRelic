#include "pvr_texture_coder.h"
#include <QDataStream>
#include <QIODevice>
#include <QDebug>

PvrTextureCoder::PvrTextureCoder(QObject *parent) : QObject(parent) {}

void PvrTextureCoder::detwiddle(uint32_t v, uint32_t &x, uint32_t &y) {
    x = 0; y = 0;
    for (int i = 0; i < 16; i++) {
        x |= ((v & (1 << (2 * i))) >> i);
        y |= ((v & (1 << (2 * i + 1))) >> (i + 1));
    }
}

QImage PvrTextureCoder::decode(const QByteArray &pvrData) {
    if (pvrData.size() < 16) {
        qWarning() << "PVR payload too small to contain valid headers.";
        return QImage();
    }

    QDataStream stream(pvrData);
    stream.setByteOrder(QDataStream::LittleEndian);

    char magic[4];
    stream.readRawData(magic, 4);

    int dataOffset = 0;

    // Check GBIX
    if (qstrncmp(magic, "GBIX", 4) == 0) {
        uint32_t gbixLen;
        stream >> gbixLen;
        dataOffset = 8 + gbixLen;
        stream.device()->seek(dataOffset);
        stream.readRawData(magic, 4);
    }

    if (qstrncmp(magic, "PVRT", 4) != 0) {
        qWarning() << "Missing PVRT header.";
        return QImage();
    }

    uint32_t pvrtLen;
    stream >> pvrtLen;

    uint8_t pixelFormat, dataFormat;
    stream >> pixelFormat >> dataFormat;

    stream.device()->seek(stream.device()->pos() + 2); // skip 2 padding bytes
    uint16_t width, height;
    stream >> width >> height;

    if (width == 0 || height == 0) {
        return QImage();
    }

    QImage img(width, height, QImage::Format_ARGB32);
    img.fill(Qt::transparent);

    // Standard Dreamcast format approximations for OpenMenu defaults:
    // PixelFormat 0 = ARGB1555, 1 = RGB565
    // DataFormat 1 = SquareTwiddled
    
    // Locate pixel payload
    int payloadStart = stream.device()->pos();
    const uint8_t *rawPixels = reinterpret_cast<const uint8_t*>(pvrData.constData() + payloadStart);
    int rawSize = pvrData.size() - payloadStart;

    if (dataFormat == 1) { // Square Twiddled
        int bpp = 2; // 16-bit
        if (rawSize < width * height * bpp) {
            qWarning() << "PVR payload truncated.";
            return img;
        }

        uint16_t *src = (uint16_t*)rawPixels;
        for (int i = 0; i < (width * height); ++i) {
            uint32_t x, y;
            detwiddle(i, x, y);
            if (x < width && y < height) {
                uint16_t pixel = src[i];
                if (pixelFormat == 0) { // ARGB1555
                    int a = (pixel & 0x8000) ? 255 : 0;
                    int r = ((pixel >> 10) & 0x1F) * 255 / 31;
                    int g = ((pixel >> 5) & 0x1F) * 255 / 31;
                    int b = (pixel & 0x1F) * 255 / 31;
                    img.setPixelColor(x, y, QColor(r, g, b, a));
                } else if (pixelFormat == 1) { // RGB565
                    int r = ((pixel >> 11) & 0x1F) * 255 / 31;
                    int g = ((pixel >> 5) & 0x3F) * 255 / 63;
                    int b = (pixel & 0x1F) * 255 / 31;
                    img.setPixelColor(x, y, QColor(r, g, b));
                } else if (pixelFormat == 2) { // ARGB4444
                    int a = ((pixel >> 12) & 0xF) * 255 / 15;
                    int r = ((pixel >> 8) & 0xF) * 255 / 15;
                    int g = ((pixel >> 4) & 0xF) * 255 / 15;
                    int b = (pixel & 0xF) * 255 / 15;
                    img.setPixelColor(x, y, QColor(r, g, b, a));
                }
            }
        }
    } else {
        qWarning() << "Unsupported Data Format:" << dataFormat << "For advanced VQ compression, external extraction recommended.";
    }

    return img;
}
