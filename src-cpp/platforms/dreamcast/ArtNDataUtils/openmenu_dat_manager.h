#ifndef OPENMENU_DAT_MANAGER_H
#define OPENMENU_DAT_MANAGER_H

#include <QObject>
#include <QString>
#include <QImage>

class OpenMenuDatManager : public QObject {
    Q_OBJECT
public:
    explicit OpenMenuDatManager(QObject *parent = nullptr);

    /**
     * @brief Reads BOX.DAT or ICON.DAT and locates the specific Game's PVR image.
     * @param datPath Absolute path to the .DAT database file.
     * @param gameId The alphanumeric Game ID (e.g., T-12345).
     * @param isIcon True if parsing an ICON.DAT (32x32), false if BOX.DAT (128x128).
     * @return QImage containing the decoded PVR artwork, or a null QImage if not found.
     */
    Q_INVOKABLE QImage extractArtwork(const QString &datPath, const QString &gameId, bool isIcon);

    /**
     * @brief Replaces or appends a game's artwork into the DAT file.
     * @param datPath Absolute path to the .DAT database file.
     * @param gameId The alphanumeric Game ID (e.g., T-12345).
     * @param sourceImagePath Absolute path to the source image to be converted and injected.
     * @return True if successfully updated, false otherwise.
     */
    Q_INVOKABLE bool updateArtwork(const QString &datPath, const QString &gameId, const QString &sourceImagePath);
};

#endif // OPENMENU_DAT_MANAGER_H
