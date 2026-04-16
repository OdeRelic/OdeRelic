#ifndef SWISS_LIBRARY_SERVICE_H
#define SWISS_LIBRARY_SERVICE_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <atomic>

class SwissLibraryService : public QObject {
    Q_OBJECT

public:
    explicit SwissLibraryService(QObject *parent = nullptr);

    Q_INVOKABLE QString urlToLocalFile(const QString &urlStr);
    Q_INVOKABLE void startGetGamesFilesAsync(const QString &dirPath);
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &filepath);
    Q_INVOKABLE QVariantMap renameGamefile(const QString &dirpath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName);
    Q_INVOKABLE QVariantMap moveFile(const QString &sourcePath, const QString &destPath);
    Q_INVOKABLE QVariantMap deleteFile(const QString &sourceRawPath);
    
    Q_INVOKABLE void startImportIsoAsync(const QString &sourceIsoPath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName);
    Q_INVOKABLE void startDownloadArtAsync(const QString &dirPath, const QString &gameId, const QString &callbackSourceKey);
    Q_INVOKABLE QVariantList getExternalGameFilesData(const QStringList &fileUrls);
    Q_INVOKABLE void scanExternalFilesAsync(const QStringList &fileUrls, bool dummy = false);

    Q_INVOKABLE QVariantMap checkSwissFolder(const QString &rootPath);
    Q_INVOKABLE QVariantMap createSwissFolder(const QString &rootPath);
    Q_INVOKABLE void startSwissSetupAsync(const QString &rootPath, const QString &odeType);
    Q_INVOKABLE void checkSwissUpdateAsync(const QString &rootPath);

    Q_INVOKABLE void cancelAllImports();
    Q_INVOKABLE void resetCancelFlag();

    Q_INVOKABLE void startSyncCheatsAsync(const QString &libraryRoot);
    // Dummies to satisfy OplManager-based UI signals
    Q_INVOKABLE void startConvertBinToIso(const QString &, const QString &) {}
    Q_INVOKABLE QVariantMap checkDummyFolder(const QString &) { return QVariantMap{{"hasPopsFolder", false}, {"hasPopstarter", false}, {"hasPopsIox", false}, {"popsPath", ""}}; }
    Q_INVOKABLE void startConvertBinToVcd(const QString &, const QString &) {}
    Q_INVOKABLE void startImportVcdAsync(const QString &, const QString &, const QString &, const QString &) {}
    Q_INVOKABLE void startGetPs1GamesAsync(const QString &) {}
    Q_INVOKABLE void startDownloadPs1ArtAsync(const QString &, const QString &, const QString &) {}
    Q_INVOKABLE QVariantMap deleteFileAndCue(const QString &) { return QVariantMap(); }

signals:
    void libraryScanProgress(int current, int total);
    void conversionFinished(QString sourcePath, bool success, QString newPath, QString message);
    void conversionProgress(QString sourcePath, int percent, double MBps);
    void importIsoProgress(QString sourcePath, int percent, double MBps);
    void importIsoFinished(QString sourcePath, bool success, QString destIsoPath, QString message);
    void setupSwissProgress(int percent, QString statusText);
    void setupSwissFinished(bool success, QString message);
    void swissUpdateCheckFinished(bool updateAvailable, QString localVersion, QString remoteVersion, QString savedOde);
    void gamesFilesLoaded(QString dirPath, QVariantMap data);
    void externalFilesScanProgress(int currentFolder, int totalFolders);
    void externalFilesScanFinished(bool isGc, QVariantList files);
    
    void artDownloadProgress(QString sourcePath, int percent);
    void artDownloadFinished(QString sourcePath, bool success, QString message);

    void syncCheatsProgress(int current, int total);
    void syncCheatsFinished(int syncedCount);

    // Dummy signals for multi-purpose parent QML compatibility
    void ps1ConversionProgress(QString sourcePath, int percent, double MBps);
    void ps1ImportProgress(QString sourcePath, int percent);
    void ps1ArtDownloadProgress(QString sourcePath, int percent);
    void ps1GamesLoaded(QString dirPath, QVariantMap data);

private:
    bool provisionCheat(const QString& gameId, const QString& libraryRoot);
    std::atomic<bool> m_cancelRequested{false};
};

#endif // SWISS_LIBRARY_SERVICE_H
