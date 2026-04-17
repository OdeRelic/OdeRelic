#ifndef PVR_TEXTURE_CODER_H
#define PVR_TEXTURE_CODER_H

#include <QObject>
#include <QImage>
#include <QByteArray>

/**
 * @brief Native decoder for Dreamcast PVR textures, specialized for OpenMenu artwork formats.
 */
class PvrTextureCoder : public QObject {
    Q_OBJECT
public:
    explicit PvrTextureCoder(QObject *parent = nullptr);

    /**
     * @brief Decodes a standard PVR byte payload into a usable ARGB QImage natively.
     * @param pvrData The raw byte array extracted from a DAT container.
     * @return QImage containing the un-twiddled ARGB565/RGB565 graphic, or a null QImage on failure.
     */
    Q_INVOKABLE QImage decode(const QByteArray &pvrData);

private:
    /**
     * @brief Untwiddles the Morton Z-order curve coordinate mapped in PVR memory.
     * @param v Twiddled morton code offset.
     * @param x Output X pixel coordinate.
     * @param y Output Y pixel coordinate.
     */
    void detwiddle(uint32_t v, uint32_t &x, uint32_t &y);
};

#endif // PVR_TEXTURE_CODER_H
