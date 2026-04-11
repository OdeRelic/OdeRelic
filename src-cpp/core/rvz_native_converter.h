#ifndef RVZ_NATIVE_CONVERTER_H
#define RVZ_NATIVE_CONVERTER_H

#include <QString>
#include <QObject>
#include <QFile>

class RvzNativeConverter : public QObject {
    Q_OBJECT
public:
    explicit RvzNativeConverter(QObject *parent = nullptr);

    /// Asynchronously attempts native C++ expansion of an RVZ / GCZ format stream
    /// into a standard GC ISO image by utilizing zstd dictionary mapping natively.
    bool convertRvzToIso(const QString &sourcePath, const QString &destPath, QString &outError);

signals:
    void progressUpdated(int percentage);
};

#endif // RVZ_NATIVE_CONVERTER_H
