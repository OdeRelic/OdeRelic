#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QVariantMap>
#include "filesystem_cache.h"

class TestFileSystemCache : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    
    void testSaveAndLoadCache();
    void testCacheOverwrite();
    void testEmptyCache();

private:
    QTemporaryDir* m_tempDir;
};

void TestFileSystemCache::initTestCase()
{
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
}

void TestFileSystemCache::cleanupTestCase()
{
    delete m_tempDir;
}

void TestFileSystemCache::testSaveAndLoadCache()
{
    QString dir = m_tempDir->path() + "/TestDir";
    QDir().mkpath(dir);

    QVariantMap data;
    QVariantMap itemInfo;
    itemInfo["mtime"] = 123456789;
    itemInfo["size"] = 1024;
    itemInfo["version"] = "1.05";
    data["SLUS_123.45.iso"] = itemInfo;

    // Utilize generic console naming validating cross-compatibility
    FileSystemCache::saveCache(dir, data, "xbox");

    QVERIFY(QFile::exists(dir + "/.xbox_oderelic_cache.json"));

    QVariantMap loadedData = FileSystemCache::loadCache(dir, "xbox");
    
    QVERIFY(loadedData.contains("SLUS_123.45.iso"));
    QVariantMap loadedItem = loadedData["SLUS_123.45.iso"].toMap();
    
    QCOMPARE(loadedItem["mtime"].toLongLong(), 123456789LL);
    QCOMPARE(loadedItem["size"].toLongLong(), 1024LL);
    QCOMPARE(loadedItem["version"].toString(), QString("1.05"));
}

void TestFileSystemCache::testCacheOverwrite()
{
    QString dir = m_tempDir->path() + "/TestDir";
    
    // Test cache array mutation logic
    QVariantMap overwriteData;
    QVariantMap overwriteItem;
    overwriteItem["mtime"] = 999999999;
    overwriteItem["size"] = 2048;
    overwriteItem["version"] = "2.00";
    overwriteData["SLUS_123.45.iso"] = overwriteItem;

    FileSystemCache::saveCache(dir, overwriteData, "xbox");

    QVariantMap loadedData = FileSystemCache::loadCache(dir, "xbox");
    QVariantMap loadedItem = loadedData["SLUS_123.45.iso"].toMap();
    
    QCOMPARE(loadedItem["version"].toString(), QString("2.00"));
}

void TestFileSystemCache::testEmptyCache()
{
    QString dir = m_tempDir->path() + "/EmptyDir";
    QDir().mkpath(dir);

    QVariantMap loadedData = FileSystemCache::loadCache(dir, "gamecube");
    QVERIFY(loadedData.isEmpty());
}

QTEST_MAIN(TestFileSystemCache)
#include "tst_filesystemcache.moc"
