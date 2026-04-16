#ifndef PVR_TEXTURE_CODER_H
#define PVR_TEXTURE_CODER_H

#include <QObject>
#include <QImage>
#include <QByteArray>

// Basic wrapper for PVR texture reading focusing on standard Dreamcast formats
class PvrTextureCoder : public QObject {
    Q_OBJECT
public:
    explicit PvrTextureCoder(QObject *parent = nullptr);

    // Decodes a standard PVR byte payload into a usable ARGB QImage natively
    Q_INVOKABLE QImage decode(const QByteArray &pvrData);

private:
    void detwiddle(uint32_t v, uint32_t &x, uint32_t &y);
};

#endif // PVR_TEXTURE_CODER_H
