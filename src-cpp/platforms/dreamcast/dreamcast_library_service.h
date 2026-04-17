#ifndef DREAMCAST_LIBRARY_SERVICE_H
#define DREAMCAST_LIBRARY_SERVICE_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <atomic>

class DreamcastLibraryService : public QObject {
    Q_OBJECT

public:
    explicit DreamcastLibraryService(QObject *parent = nullptr);

    /**
     * @brief Formats a generic QML file URL into a standardized local path.
     * @param urlStr The QUrl string provided by the QML FileDialog.
     * @return Cleaned absolute file path string.
     */
    Q_INVOKABLE QString urlToLocalFile(const QString &urlStr);
    
    // Core API Mapped against Implementation Plan

    /**
     * @brief Validates if the selected folder is a valid Dreamcast/GDEMU structure.
     * @param rootPath Absolute path to the GDEMU root directory.
     * @return QVariantMap containing validation flags ("success", "isSetup", etc).
     */
    Q_INVOKABLE QVariantMap checkDreamcastFolder(const QString &rootPath);

    /**
     * @brief Asynchronously scans a directory and emits gamesFilesLoaded with discovered GDEMU slots.
     * @param dirPath Absolute path to the root directory.
     */
    Q_INVOKABLE void startGetGamesFilesAsync(const QString &dirPath);

    /**
     * @brief Imports raw games (.GDI, .CDI) asynchronously into sequential GDEMU slots.
     * @param sourcePaths List of absolute paths pointing to the source games.
     * @param targetLibraryRoot The GDEMU target root directory.
     */
    Q_INVOKABLE void startImportGameAsync(const QStringList &sourcePaths, const QString &targetLibraryRoot);

    /**
     * @brief Commits re-ordered GDEMU slot directories mathematically into the SD card.
     * @param targetLibraryRoot The GDEMU target root directory.
     * @param orderedOriginalPaths The new order of paths mathematically validated by QML.
     */
    Q_INVOKABLE void commitLibraryOrderAsync(const QString &targetLibraryRoot, const QStringList &orderedOriginalPaths);
    
    /**
     * @brief Validates if an OpenMenu update is available via remote GitHub versioning.
     * @param rootPath Absolute path to the GDEMU root directory.
     */
    Q_INVOKABLE void checkOpenMenuUpdateAsync(const QString &rootPath);

    /**
     * @brief Installs or updates the OpenMenu GD-ROM environment synchronously on the SD Card.
     * @param rootPath Absolute path to the GDEMU root directory.
     */
    Q_INVOKABLE void startInstallMenuAsync(const QString &rootPath);

    /**
     * @brief Triggers a partial rebuild of OpenMenu databases without full bootloader validation.
     * @param rootPath Absolute path to the GDEMU root directory.
     */
    Q_INVOKABLE void startInstallMenuDbAsync(const QString &rootPath);
    
    /**
     * @brief Scans all slots and attempts to download missing 128x128 boxarts.
     * @param rootPath Absolute path to the GDEMU root directory.
     */
    Q_INVOKABLE void startFetchMissingArtworkAsync(const QString &rootPath);

    /**
     * @brief Deploys native CodeBreaker cheats payload into the OpenMenu hierarchy.
     * @param rootPath Absolute path to the GDEMU root directory.
     */
    Q_INVOKABLE void startSyncCheatsAsync(const QString &rootPath);

    /**
     * @brief Triggers immediate cancellation of any active asynchronous import jobs.
     */
    Q_INVOKABLE void cancelAllImports();

    /**
     * @brief Resets the cancellation flag for future async operations.
     */
    Q_INVOKABLE void resetCancelFlag();
    
    /**
     * @brief Safely removes a specific game slot folder and its contents.
     * @param folderPath Absolute path of the GDEMU slot to be deleted.
     * @return QVariantMap confirming status.
     */
    Q_INVOKABLE QVariantMap deleteGameFolder(const QString &folderPath);
    
    /**
     * @brief Reads the IP.BIN to extract the product ID (e.g. T-12345).
     * @param filepath The .GDI or .CDI file path.
     * @return QVariantMap containing parsed metadata.
     */
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &filepath);

    /**
     * @brief Downloads custom boxart for a specific game and saves it directly to its slot folder.
     * @param dirPath The absolute path to the game slot directory.
     * @param gameId The alphanumeric Game ID.
     * @param callbackSourceKey Optional key for referencing the async response in QML.
     */
    Q_INVOKABLE void startDownloadArtAsync(const QString &dirPath, const QString &gameId, const QString &callbackSourceKey);

    /**
     * @brief Scans external URLs for valid Dreamcast game files (GDI, CDI, ISO, CCD).
     * @param fileUrls List of paths to scan
     * @param dummy unused parameter to match signature
     */
    Q_INVOKABLE void scanExternalFilesAsync(const QStringList &fileUrls, bool dummy = false);
    
    /**
     * @brief Fully parses the GDEMU directory and builds track01.iso and track05.iso dynamically.
     * Made public specifically for Google Test / CTest integration of transient asset lifecycle.
     * @param rootPath Absolute path to the GDEMU root directory.
     * @return True if the ISO generation pipeline succeeds and validates.
     */
    bool buildAndDeployMenuGdrom(const QString &rootPath);

signals:
    void libraryScanProgress(int current, int total);
    void gamesFilesLoaded(QString dirPath, QVariantMap data);

    void importProgress(QString sourcePath, int percent, double MBps);
    void importFinished(QString sourcePath, bool success, QString destIsoPath, QString message);

    void menuUpdateCheckFinished(bool updateAvailable, QString localVersion, QString remoteVersion);
    void setupMenuProgress(int percent, QString statusText);
    void setupMenuFinished(bool success, QString message);

    void reorderProgress(int current, int total);
    void reorderFinished(bool success, QString message);

    void fetchArtworkProgress(int percent, QString statusText);
    void fetchArtworkFinished(bool success, QString message);

    void batchArtDownloadProgress(int current, int total);
    void batchArtDownloadFinished(bool success);

    void externalFilesScanProgress(int current, int total);
    void externalFilesScanFinished(bool success, const QVariantList &files);
    void artDownloadProgress(QString sourcePath, int percent);
    void artDownloadFinished(QString sourcePath, bool success, QString message);
    
    void syncCheatsProgress(int percent);
    void syncCheatsFinished(bool success, QString message);

private:
     std::atomic<bool> m_cancelRequested{false};
     QVariantMap parseMetadataFromMedia(const QString &mediaPath);
     QString generateOpenMenuIni(const QString &rootPath);
    QVariantList getExternalGameFilesData(const QStringList &fileUrls);
};

#endif // DREAMCAST_LIBRARY_SERVICE_H
