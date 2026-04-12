#include "system_utils.h"
#include <QStorageInfo>
#include <QDir>

SystemUtils::SystemUtils(QObject* parent) : QObject(parent) {}

QVariantMap SystemUtils::getStorageSpace(const QString& targetPath) {
    if (targetPath.isEmpty()) {
        return {{"total", 0}, {"free", 0}};
    }

    QStorageInfo storage(targetPath);
    if (!storage.isValid() || !storage.isReady()) {
        return {{"total", 0}, {"free", 0}};
    }

    qint64 bytesTotal = storage.bytesTotal();
    qint64 bytesFree = storage.bytesAvailable();

    return {
        {"total", bytesTotal},
        {"free", bytesFree}
    };
}
