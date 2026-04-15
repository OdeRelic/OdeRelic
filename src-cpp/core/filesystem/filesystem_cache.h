#ifndef FILESYSTEM_CACHE_H
#define FILESYSTEM_CACHE_H

#include <QString>
#include <QVariantMap>

class FileSystemCache {
public:
    static QVariantMap loadCache(const QString &dirPath, const QString &cacheType);
    static void saveCache(const QString &dirPath, const QVariantMap &cacheData, const QString &cacheType);
};

#endif // FILESYSTEM_CACHE_H
