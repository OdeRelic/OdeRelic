#ifndef PS1_XSTATION_LIBRARY_SERVICE_H
#define PS1_XSTATION_LIBRARY_SERVICE_H

#include <QObject>
#include <QVariantMap>
#include <QVariantList>
#include <QStringList>
#include <atomic>

class PS1XstationLibraryService : public QObject {
    Q_OBJECT

public:
    explicit PS1XstationLibraryService(QObject *parent = nullptr);

    Q_INVOKABLE QString urlToLocalFile(const QString &urlStr);
    Q_INVOKABLE void startGetGamesFilesAsync(const QString &dirPath);
    Q_INVOKABLE QVariantMap renameGamefile(const QString &dirpath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName);
    Q_INVOKABLE QVariantMap moveFile(const QString &sourcePath, const QString &destPath);
    
    Q_INVOKABLE void startImportIsoAsync(const QString &sourceIsoPath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName);
    Q_INVOKABLE void startDownloadArtAsync(const QString &dirPath, const QString &gameId, const QString &callbackSourceKey);
    Q_INVOKABLE void startBatchArtDownloadAsync(const QVariantList &gamesList);
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &filepath);
    Q_INVOKABLE QVariantList getExternalGameFilesData(const QStringList &fileUrls);
    Q_INVOKABLE void scanExternalFilesAsync(const QStringList &fileUrls, bool dummy = false);

    Q_INVOKABLE QVariantMap checkXStationFolder(const QString &rootPath);
    Q_INVOKABLE QVariantMap createXStationFolder(const QString &rootPath);
    Q_INVOKABLE void startXStationSetupAsync(const QString &rootPath, const QString &odeType);
    Q_INVOKABLE void checkXStationUpdateAsync(const QString &rootPath);

    Q_INVOKABLE void cancelAllImports();
    Q_INVOKABLE void resetCancelFlag();

signals:
    void libraryScanProgress(int current, int total);
    void importIsoProgress(QString sourcePath, int percent, double MBps);
    void importIsoFinished(QString sourcePath, bool success, QString destIsoPath, QString message);
    void setupXStationProgress(int percent, QString statusText);
    void setupXStationFinished(bool success, QString message);
    void xstationUpdateCheckFinished(bool updateAvailable, QString localVersion, QString remoteVersion, QString savedOde);
    void gamesFilesLoaded(QString dirPath, QVariantMap data);
    void externalFilesScanProgress(int currentFolder, int totalFolders);
    void externalFilesScanFinished(bool isPs1, QVariantList files);
    void artDownloadProgress(QString sourcePath, int percent);
    void artDownloadFinished(QString sourcePath, bool success, QString message);
    void batchArtDownloadProgress(int current, int total);
    void batchArtDownloadFinished(bool success);

private:
    std::atomic<bool> cancelRequested{false};
};

#endif // PS1_XSTATION_LIBRARY_SERVICE_H
