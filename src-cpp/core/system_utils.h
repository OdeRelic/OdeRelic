#pragma once

#include <QObject>
#include <QVariantMap>
#include <QString>

class SystemUtils : public QObject {
    Q_OBJECT
public:
    explicit SystemUtils(QObject* parent = nullptr);

    Q_INVOKABLE QVariantMap getStorageSpace(const QString& targetPath);
};
