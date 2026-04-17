#include <QtQuickTest/quicktest.h>
#include <QQmlEngine>
#include <QQmlContext>
#include <QObject>
#include <QVariantMap>
#include <QTimer>

class MockSystemUtils : public QObject {
    Q_OBJECT
public:
    explicit MockSystemUtils(QObject *parent = nullptr) : QObject(parent), m_deleteGameCalled(false) {
        setProperty("mock", true);
    }
    
    Q_INVOKABLE bool wasDeleteGameCalled() const { return m_deleteGameCalled; }

    Q_INVOKABLE QVariantMap getStorageSpace(const QString &path) {
        Q_UNUSED(path)
        QVariantMap map;
        map["free"] = 1000.0;
        map["total"] = 5000.0;
        return map;
    }
    Q_INVOKABLE double getStorageMultiplier() { return 1.0; }
    Q_INVOKABLE QString formatSize(double size) { return QString::number(size) + " B"; }
    Q_INVOKABLE bool isOnSameDrive(const QString &path1, const QString &path2) {
        Q_UNUSED(path1)
        Q_UNUSED(path2)
        return true;
    }
    Q_INVOKABLE void deleteGame(const QString &path, bool isSource) {
        Q_UNUSED(path)
        Q_UNUSED(isSource)
        m_deleteGameCalled = true;
    }

private:
    bool m_deleteGameCalled;
};

class MockSwissLibraryService : public QObject {
    Q_OBJECT
public:
    explicit MockSwissLibraryService(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantMap checkSwissFolder(const QString &path) {
        Q_UNUSED(path)
        QVariantMap map;
        map["isValid"] = true;
        map["hasSwissFile"] = true;
        map["hasSwiss"] = true;
        map["savedOde"] = "Standalone";
        return map;
    }

    Q_INVOKABLE QString urlToLocalFile(const QString &url) { return url; }
    Q_INVOKABLE void startSwissSetupAsync(const QString &path, const QString &ode) {
        Q_UNUSED(path)
        Q_UNUSED(ode)
    }
    Q_INVOKABLE void checkSwissUpdateAsync(const QString &path) { Q_UNUSED(path) }
    Q_INVOKABLE void scanExternalFilesAsync(const QVariantList &urls, bool flag) {
        Q_UNUSED(urls)
        Q_UNUSED(flag)
    }
    Q_INVOKABLE void startGetGamesFilesAsync(const QString &path) { Q_UNUSED(path) }
    Q_INVOKABLE void startConvertBinToIso(const QString &isoPath, const QString &temp) {
        Q_UNUSED(isoPath)
        Q_UNUSED(temp)
    }
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &path) {
        Q_UNUSED(path)
        QVariantMap map;
        map["success"] = true;
        map["gameId"] = "GMCE01";
        return map;
    }
    Q_INVOKABLE void startImportIsoAsync(const QString &isoPath, const QString &dest, const QString &gameId, const QString &name) {
        Q_UNUSED(isoPath)
        Q_UNUSED(dest)
        Q_UNUSED(gameId)
        Q_UNUSED(name)
    }
    Q_INVOKABLE void startDownloadArtAsync(const QString &artFolder, const QString &id, const QString &path) {
        Q_UNUSED(artFolder)
        Q_UNUSED(id)
        Q_UNUSED(path)
    }
    Q_INVOKABLE void startSyncCheatsAsync(const QString &path) { 
        Q_UNUSED(path) 
        emit syncCheatsFinished(10);
    }
    
signals:
    void syncCheatsFinished(int syncedCount);
public:
    Q_INVOKABLE void resetCancelFlag() {}
    Q_INVOKABLE void cancelAllImports() {}
    Q_INVOKABLE QVariantMap renameGamefile(const QString &isoPath, const QString &dest, const QString &gameId, const QString &name) {
        Q_UNUSED(isoPath)
        Q_UNUSED(dest)
        Q_UNUSED(gameId)
        Q_UNUSED(name)
        QVariantMap map;
        map["success"] = true;
        return map;
    }
    Q_INVOKABLE void deleteFileAndCue(const QString &path) { Q_UNUSED(path) }
};

class MockOplLibraryService : public QObject {
    Q_OBJECT
public:
    explicit MockOplLibraryService(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantMap checkOplFolder(const QString &path) {
        Q_UNUSED(path)
        QVariantMap map;
        map["isValid"] = true;
        map["hasAppsFolder"] = true;
        map["hasArtFolder"] = true;
        map["hasCdFolder"] = true;
        map["hasDvdFolder"] = true;
        map["hasVmcFolder"] = true;
        map["hasThemFolder"] = true;
        return map;
    }
    
    Q_INVOKABLE QVariantMap checkPopsFolder(const QString &path) {
        Q_UNUSED(path)
        QVariantMap map;
        map["isValid"] = true;
        map["hasPopsDir"] = true;
        map["hasPopstarterElf"] = true;
        map["hasPopsPak"] = true;
        map["hasIoprpImg"] = true;
        return map;
    }
    
    Q_INVOKABLE QString urlToLocalFile(const QString &url) { return url; }
    Q_INVOKABLE void scanExternalFilesAsync(const QVariantList &urls, bool flag) { Q_UNUSED(urls) Q_UNUSED(flag) }
    Q_INVOKABLE void startGetGamesFilesAsync(const QString &path) { Q_UNUSED(path) }
    Q_INVOKABLE void startGetPs1GamesAsync(const QString &path) { Q_UNUSED(path) }
    Q_INVOKABLE void startConvertBinToIso(const QString &iso, const QString &temp) { Q_UNUSED(iso) Q_UNUSED(temp) }
    Q_INVOKABLE void startConvertBinToVcd(const QString &bin, const QString &vcd) { Q_UNUSED(bin) Q_UNUSED(vcd) }
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &path) { 
        Q_UNUSED(path) 
        QVariantMap map; map["success"] = true; map["gameId"] = "SLUS_200.71"; return map; 
    }
    Q_INVOKABLE QVariantMap renameGamefile(const QString &isoPath, const QString &dest, const QString &gameId, const QString &name, bool isCd) {
        Q_UNUSED(isoPath) Q_UNUSED(dest) Q_UNUSED(gameId) Q_UNUSED(name) Q_UNUSED(isCd)
        QVariantMap map; map["success"] = true; return map;
    }
    Q_INVOKABLE void startDownloadArtAsync(const QString &f, const QString &id, const QString &p) { Q_UNUSED(f) Q_UNUSED(id) Q_UNUSED(p) }
    Q_INVOKABLE void startDownloadPs1ArtAsync(const QString &f, const QString &id, const QString &p) { Q_UNUSED(f) Q_UNUSED(id) Q_UNUSED(p) }
    Q_INVOKABLE void startImportVcdAsync(const QString &vcd, const QString &dest, const QString &id, const QString &name) { Q_UNUSED(vcd) Q_UNUSED(dest) Q_UNUSED(id) Q_UNUSED(name) }
    Q_INVOKABLE void cancelAllImports() {}
    Q_INVOKABLE void startImportIsoAsync(const QString &iso, const QString &dest, const QString &id, const QString &name, bool isCd) { Q_UNUSED(iso) Q_UNUSED(dest) Q_UNUSED(id) Q_UNUSED(name) Q_UNUSED(isCd) }
    Q_INVOKABLE void startBatchArtDownloadAsync(const QVariantList &games, const QString &path) { 
        Q_UNUSED(games) Q_UNUSED(path) 
        QTimer::singleShot(20, this, [this]() { emit batchArtDownloadFinished(true); });
    }
    Q_INVOKABLE void startArtDownloadAsync(const QString &p, const QString &id, const QString &dp) { Q_UNUSED(p) Q_UNUSED(id) Q_UNUSED(dp) }
    Q_INVOKABLE QVariantMap copyFileToPopsFolder(const QString &src, const QString &dest) { 
        Q_UNUSED(src) Q_UNUSED(dest) 
        QVariantMap m; m["success"] = true; return m; 
    }
    
signals:
    void batchArtDownloadFinished(bool success);
};

class MockTranslationManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentLanguage READ currentLanguage NOTIFY currentLanguageChanged)
public:
    explicit MockTranslationManager(QObject *parent = nullptr) : QObject(parent), m_lang("en") {}
    
    QString currentLanguage() const { return m_lang; }
    
    Q_INVOKABLE void setLanguage(const QString &lang) {
        if (m_lang != lang) {
            m_lang = lang;
            emit currentLanguageChanged();
        }
    }
signals:
    void currentLanguageChanged();
private:
    QString m_lang;
};

class MockPs1XStationLibraryService : public QObject {
    Q_OBJECT
public:
    explicit MockPs1XStationLibraryService(QObject *parent = nullptr) : QObject(parent) {}
    
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &sourcePath) {
        Q_UNUSED(sourcePath)
        QVariantMap m; m["success"] = true; m["gameId"] = "SLUS-12345"; return m;
    }
    
    Q_INVOKABLE void startDownloadArtAsync(const QString &artFolder, const QString &extractedId, const QString &sourcePath) {
        Q_UNUSED(artFolder) Q_UNUSED(extractedId) Q_UNUSED(sourcePath)
    }
    
    Q_INVOKABLE void cancelAllImports() {}
    
    Q_INVOKABLE void startXStationSetupAsync(const QString &libraryPath, const QString &selectedOde) {
        Q_UNUSED(libraryPath) Q_UNUSED(selectedOde)
    }
    
    Q_INVOKABLE QString urlToLocalFile(const QString &url) { return url; }
    
    Q_INVOKABLE QVariantMap checkXStationFolder(const QString &cleanPath) {
        Q_UNUSED(cleanPath)
        QVariantMap m; m["isSetup"] = true; return m;
    }
    
    Q_INVOKABLE void checkXStationUpdateAsync(const QString &cleanPath) { Q_UNUSED(cleanPath) }
    
    Q_INVOKABLE void scanExternalFilesAsync(const QVariantList &urls, bool recursive) { Q_UNUSED(urls) Q_UNUSED(recursive) }
    
    Q_INVOKABLE void startGetGamesFilesAsync(const QString &currentLibraryPath) { Q_UNUSED(currentLibraryPath) }
    
    Q_INVOKABLE void startImportIsoAsync(const QString &filePath, const QString &currentLibraryPath, const QString &gameId, const QString &name) {
        Q_UNUSED(filePath) Q_UNUSED(currentLibraryPath) Q_UNUSED(gameId) Q_UNUSED(name)
    }
    
    Q_INVOKABLE void resetCancelFlag() {}
    
    Q_INVOKABLE void startBatchArtDownloadAsync(const QVariantList &payload) {
        Q_UNUSED(payload)
        QTimer::singleShot(20, this, [this]() { emit batchArtDownloadFinished(true); });
    }

signals:
    void batchArtDownloadFinished(bool success);
    void batchArtDownloadProgress(int current, int total);
};

class MockDreamcastLibraryService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool importGameCalled READ importGameCalled NOTIFY importGameCalledChanged)
public:
    explicit MockDreamcastLibraryService(QObject *parent = nullptr) : QObject(parent), m_importGameCalled(false), m_installMenuCalled(false), m_syncCheatsCalled(false) {}

    bool importGameCalled() const { return m_importGameCalled; }
    Q_INVOKABLE bool wasInstallMenuCalled() const { return m_installMenuCalled; }
    Q_INVOKABLE bool wasSyncCheatsCalled() const { return m_syncCheatsCalled; }

    Q_INVOKABLE QVariantMap checkDreamcastFolder(const QString &cleanPath) {
        Q_UNUSED(cleanPath)
        QVariantMap map;
        map["isValid"] = true;
        map["isFormatCorrect"] = true;
        map["isPartitionCorrect"] = true;
        map["hasOpenMenuDb"] = true;
        return map;
    }
    Q_INVOKABLE void startInstallMenuAsync(const QString &cleanPath) { Q_UNUSED(cleanPath) m_installMenuCalled = true; }
    Q_INVOKABLE void startInstallMenuDbAsync(const QString &cleanPath) { Q_UNUSED(cleanPath) }
    Q_INVOKABLE void checkOpenMenuUpdateAsync(const QString &cleanPath) { Q_UNUSED(cleanPath) }
    Q_INVOKABLE void scanExternalFilesAsync(const QStringList &urls, bool flag) { Q_UNUSED(urls) Q_UNUSED(flag) }
    Q_INVOKABLE void startGetGamesFilesAsync(const QString &path) { Q_UNUSED(path) }
    Q_INVOKABLE QVariantMap tryDetermineGameIdFromHex(const QString &path) { 
        Q_UNUSED(path) 
        QVariantMap map; map["success"] = true; map["gameId"] = "T-12345"; return map; 
    }
    Q_INVOKABLE void startDownloadArtAsync(const QString &artFolder, const QString &id, const QString &path) {
        Q_UNUSED(artFolder) Q_UNUSED(id) Q_UNUSED(path)
    }
    Q_INVOKABLE void startImportGameAsync(const QStringList &sourcePaths, const QString &targetLibraryRoot) {
        Q_UNUSED(sourcePaths)
        Q_UNUSED(targetLibraryRoot)
        m_importGameCalled = true;
        emit importGameCalledChanged();
    }
    Q_INVOKABLE void cancelAllImports() {}
    Q_INVOKABLE void resetCancelFlag() {}
    Q_INVOKABLE QString urlToLocalFile(const QString &url) { return url; }
    
    Q_INVOKABLE void startSyncCheatsAsync(const QString &path) { 
        Q_UNUSED(path) 
        m_syncCheatsCalled = true;
    }

    Q_INVOKABLE bool wasImportGameCalled() const {
        return m_importGameCalled;
    }

signals:
    void importGameCalledChanged();

private:
    bool m_importGameCalled;
    bool m_installMenuCalled;
    bool m_syncCheatsCalled;
};

class TestSetup : public QObject {
    Q_OBJECT
public:
    explicit TestSetup(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    void qmlEngineAvailable(QQmlEngine *engine) {
        QObject* systemUtilsMock = new MockSystemUtils(engine);
        engine->rootContext()->setContextProperty("systemUtils", systemUtilsMock);
        
        QObject* swissMock = new MockSwissLibraryService(engine);
        engine->rootContext()->setContextProperty("swissLibraryService", swissMock);
        
        QObject* oplMock = new MockOplLibraryService(engine);
        engine->rootContext()->setContextProperty("oplLibraryService", oplMock);
        
        QObject* ps1Mock = new MockPs1XStationLibraryService(engine);
        engine->rootContext()->setContextProperty("ps1XstationLibraryService", ps1Mock);
        
        QObject* translationMock = new MockTranslationManager(engine);
        engine->rootContext()->setContextProperty("translationManager", translationMock);
        
        QObject* dreamcastMock = new MockDreamcastLibraryService(engine);
        engine->rootContext()->setContextProperty("dreamcastLibraryService", dreamcastMock);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(OdeRelicUITests, TestSetup)

#include "tst_qmlui.moc"
