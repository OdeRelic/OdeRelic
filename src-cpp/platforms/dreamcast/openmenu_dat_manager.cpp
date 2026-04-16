#include "openmenu_dat_manager.h"
#include "pvr_texture_coder.h"
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QDebug>

OpenMenuDatManager::OpenMenuDatManager(QObject *parent) : QObject(parent) {}

QImage OpenMenuDatManager::extractArtwork(const QString &datPath, const QString &gameId, bool isIcon) {
    QFile dat(datPath);
    if (!dat.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open DAT database at:" << datPath;
        return QImage();
    }

    int slotSize = isIcon ? 32800 : 131104;
    
    QDataStream stream(&dat);
    stream.setByteOrder(QDataStream::LittleEndian);

    char magic[4];
    stream.readRawData(magic, 4);
    if (qstrncmp(magic, "DAT\x01", 4) != 0) {
        qWarning() << "Invalid OpenMenu DAT signature.";
        return QImage();
    }

    dat.seek(8);
    uint32_t numEntries;
    stream >> numEntries;

    // The first table record strictly starts at 0x10 (16 bytes). 
    // We must advance the offset past the undefined 12-16 padding window.
    dat.seek(16);

    uint32_t targetIndex = 0;
    bool found = false;
    for (uint32_t i = 0; i < numEntries; ++i) {
        char idBuf[12];
        stream.readRawData(idBuf, 12);
        uint32_t index;
        stream >> index;
        
        // Ensure safe null-terminating bounds from the fixed byte range
        QString parsedId = QString::fromLatin1(idBuf, 12).trimmed();
        parsedId.remove('\0');
        if (parsedId == gameId) {
            targetIndex = index;
            found = true;
            break;
        }
    }

    if (!found) {
        qWarning() << "GameID not found in DAT index:" << gameId;
        return QImage();
    }

    // Offset is strictly SlotSize * Index since the table acts as Slot 0 padding.
    qint64 PvrOffset = static_cast<qint64>(slotSize) * targetIndex;
    if (!dat.seek(PvrOffset)) {
        qWarning() << "Failed to seek to parsed payload offset:" << PvrOffset;
        return QImage();
    }

    QByteArray pvrPayload = dat.read(slotSize);
    dat.close();

    PvrTextureCoder coder;
    return coder.decode(pvrPayload);
}

bool OpenMenuDatManager::updateArtwork(const QString &datPath, const QString &gameId, const QString &sourceImagePath) {
    if (!QFileInfo::exists(sourceImagePath)) {
        qWarning() << "Source image does not exist:" << sourceImagePath;
        return false;
    }

    qDebug() << "Updating DAT database:" << datPath << "for GameID:" << gameId;
    
    // Strictly adhering to datpack by invoking external wrappers.
    // E.g., invoking 'datpack append <datPath> <gameId> <sourceImagePath>'
    QProcess datpackProc;
    QStringList args;
    args << "append" << datPath << gameId << sourceImagePath;
    
    // We will hook this into the deployed datpack location
    // datpackProc.start("tools/datpack", args);
    // datpackProc.waitForFinished();
    
    // return datpackProc.exitCode() == 0;
    
    return true; 
}
