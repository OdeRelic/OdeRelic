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

    Q_INVOKABLE QString urlToLocalFile(const QString &urlStr);
    
    // Core API Mapped against Implementation Plan
    Q_INVOKABLE QVariantMap checkDreamcastFolder(const QString &rootPath);
    Q_INVOKABLE void startGetGamesFilesAsync(const QString &dirPath);
    Q_INVOKABLE void startImportGameAsync(const QStringList &sourcePaths, const QString &targetLibraryRoot);
    Q_INVOKABLE void commitLibraryOrderAsync(const QString &targetLibraryRoot, const QStringList &orderedOriginalPaths);
    
    Q_INVOKABLE void checkOpenMenuUpdateAsync(const QString &rootPath);
    Q_INVOKABLE void startInstallMenuAsync(const QString &rootPath);
    Q_INVOKABLE void startInstallMenuDbAsync(const QString &rootPath);
    
    Q_INVOKABLE void startFetchMissingArtworkAsync(const QString &rootPath);
    Q_INVOKABLE void startSyncCheatsAsync(const QString &rootPath);

    Q_INVOKABLE void cancelAllImports();
    Q_INVOKABLE void resetCancelFlag();
    
    Q_INVOKABLE QVariantMap deleteGameFolder(const QString &folderPath);
    
    Q_INVOKABLE void scanExternalFilesAsync(const QStringList &fileUrls, bool dummy);
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &filepath);
    Q_INVOKABLE void startDownloadArtAsync(const QString &dirPath, const QString &gameId, const QString &callbackSourceKey);
    Q_INVOKABLE void startImportIsoAsync(const QString &sourceIsoPath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName);
    Q_INVOKABLE void startConvertBinToIso(const QString &binPath, const QString &destIsoPath);

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

    void externalFilesScanProgress(int currentFolder, int totalFolders);
    void externalFilesScanFinished(bool isSetup, QVariantList files);
    void batchArtDownloadProgress(int current, int total);
    void batchArtDownloadFinished(bool success);
    void artDownloadProgress(QString sourcePath, int percent);
    void artDownloadFinished(QString sourcePath, bool success, QString message);
    
    void syncCheatsProgress(int percent);
    void syncCheatsFinished(bool success, QString message);

private:
     std::atomic<bool> m_cancelRequested{false};
     QVariantMap parseMetadataFromMedia(const QString &mediaPath);
};

#endif // DREAMCAST_LIBRARY_SERVICE_H
