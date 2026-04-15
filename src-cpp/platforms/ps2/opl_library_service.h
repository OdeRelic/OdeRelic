#ifndef LIBRARY_SERVICE_H
#define LIBRARY_SERVICE_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QVariantList>

class OplLibraryService : public QObject {
    Q_OBJECT

public:
    explicit OplLibraryService(QObject *parent = nullptr);

    Q_INVOKABLE void startGetGamesFilesAsync(const QString &dirPath);
    Q_INVOKABLE QVariantMap getArtFolder(const QString &dirPath);
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &filepath);
    Q_INVOKABLE QString urlToLocalFile(const QString &urlStr);
    
    Q_INVOKABLE QVariantMap checkOplFolder(const QString &rootPath);
    
    Q_INVOKABLE QVariantMap renameGamefile(const QString &dirpath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName, bool forceCD = false);

    Q_INVOKABLE QVariantMap moveFile(const QString &sourcePath, const QString &destPath);

    // Concurrency Conversion API (PS2)
    Q_INVOKABLE void startConvertBinToIso(const QString &sourceBinPath, const QString &destIsoPath);
    Q_INVOKABLE void startImportIsoAsync(const QString &sourceIsoPath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName, bool forceCD = false);
    Q_INVOKABLE void startDownloadArtAsync(const QString &system, const QString &dirPath, const QString &gameId, const QString &callbackSourceKey);
    Q_INVOKABLE QVariantList getExternalGameFilesData(const QStringList &fileUrls);
    Q_INVOKABLE void scanExternalFilesAsync(const QStringList &fileUrls, bool isPs1);
    
    Q_INVOKABLE void startBatchArtDownloadAsync(const QString &system, const QVariantList &gamesList, const QString &artFolder);

    // ── PS1 / POPStarter API ──────────────────────────────────────────────────
    Q_INVOKABLE void startGetPs1GamesAsync(const QString &dirPath);
    Q_INVOKABLE QVariantMap tryDeterminePs1GameIdFromHex(const QString &filepath);
    Q_INVOKABLE void startConvertBinToVcd(const QString &sourcePath, const QString &destVcdPath);
    Q_INVOKABLE void startImportVcdAsync(const QString &sourceVcdPath, const QString &targetLibraryRoot, const QString &gameId, const QString &gameName);
    Q_INVOKABLE QVariantList getExternalPs1FilesData(const QStringList &fileUrls);
    Q_INVOKABLE QVariantMap checkPopsFolder(const QString &libraryRoot);
    Q_INVOKABLE QVariantMap copyFileToPopsFolder(const QString &sourcePath, const QString &libraryRoot);

signals:
    // PS2 signals
    void libraryScanProgress(int current, int total);
    void conversionFinished(QString sourcePath, bool success, QString newPath, QString message);
    void conversionProgress(QString sourcePath, int percent, double MBps);
    void importIsoFinished(QString sourcePath, bool success, QString destIsoPath, QString message);
    void importIsoProgress(QString sourcePath, int percent, double MBps);
    void artDownloadFinished(QString sourcePath, bool success, QString message);
    void artDownloadProgress(QString sourcePath, int percent);
    void gamesFilesLoaded(QString dirPath, QVariantMap data);
    
    void batchArtDownloadProgress(int current, int total);
    void batchArtDownloadFinished(bool success);

    // PS1 signals
    void ps1ConversionFinished(QString sourcePath, bool success, QString destVcdPath, QString gameId, QString message);
    void ps1ConversionProgress(QString sourcePath, int percent, double MBps);
    void ps1ImportFinished(QString sourcePath, bool success, QString destVcdPath, QString gameId, QString message);
    void ps1ImportProgress(QString sourcePath, int percent, double MBps);
    void ps1ArtDownloadFinished(QString sourcePath, bool success, QString message);
    void ps1ArtDownloadProgress(QString sourcePath, int percent);
    void ps1GamesLoaded(QString dirPath, QVariantMap data);

    void externalFilesScanFinished(bool isPs1, QVariantList files);
    void externalFilesScanProgress(int currentFolder, int totalFolders);

private:
    QNetworkAccessManager m_networkManager;
};

#endif // LIBRARY_SERVICE_H
