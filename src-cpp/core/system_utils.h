#pragma once

#include <QObject>
#include <QVariantMap>
#include <QString>

class SystemUtils : public QObject {
    Q_OBJECT
public:
    explicit SystemUtils(QObject* parent = nullptr);

    Q_INVOKABLE QVariantMap getStorageSpace(const QString& targetPath);
    Q_INVOKABLE QString formatSize(double bytes);
    Q_INVOKABLE double getStorageMultiplier();
    
    static qint64 calculateCueRealSize(const class QFileInfo &cueInfo);
};
