#include "filesystem_cache.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

QVariantMap FileSystemCache::loadCache(const QString &dirPath, const QString &cacheType) {
    QVariantMap result;
    QFile file(dirPath + "/." + cacheType + "_oderelic_cache.json");
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (doc.isObject()) {
            return doc.object().toVariantMap();
        }
    }
    return result;
}

void FileSystemCache::saveCache(const QString &dirPath, const QVariantMap &cacheData, const QString &cacheType) {
    QFile file(dirPath + "/." + cacheType + "_oderelic_cache.json");
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(QJsonObject::fromVariantMap(cacheData));
        file.write(doc.toJson(QJsonDocument::Compact));
        file.close();
    }
}
