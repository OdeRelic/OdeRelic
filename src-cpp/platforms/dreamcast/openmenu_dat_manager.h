#ifndef OPENMENU_DAT_MANAGER_H
#define OPENMENU_DAT_MANAGER_H

#include <QObject>
#include <QString>
#include <QImage>

class OpenMenuDatManager : public QObject {
    Q_OBJECT
public:
    explicit OpenMenuDatManager(QObject *parent = nullptr);

    // Reads BOX.DAT or ICON.DAT and locates the specific Game's PVR image
    Q_INVOKABLE QImage extractArtwork(const QString &datPath, const QString &gameId, bool isIcon);

    // Replaces or appends a game's artwork into the DAT file leveraging external ImageMagick / datpack binaries
    Q_INVOKABLE bool updateArtwork(const QString &datPath, const QString &gameId, const QString &sourceImagePath);
};

#endif // OPENMENU_DAT_MANAGER_H
