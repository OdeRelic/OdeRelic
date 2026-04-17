#include "openmenu_image_provider.h"
#include <QUrl>
#include <QCoreApplication>
#include <QDir>

OpenMenuImageProvider::OpenMenuImageProvider() 
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage OpenMenuImageProvider::requestImage(const QString &id, QSize *size, const QSize&) {
    // Expected id format: "BOX/T-12345M?rootPath=file:///Users/abc"
    QStringList parts = id.split("?rootPath=");
    if (parts.size() < 2) return QImage();
    
    QString typeAndId = parts[0]; 
    QString rawRootPath = parts[1];
    
    // Resolve URL safety to local files
    QUrl urlPath(rawRootPath);
    QString rootPath = urlPath.isLocalFile() ? urlPath.toLocalFile() : rawRootPath;
    
    QStringList typeParts = typeAndId.split("/");
    if (typeParts.size() < 2) return QImage();

    bool isIcon = (typeParts[0] == "ICON");
    QString gameId = typeParts[1];
    gameId.remove("-").remove(" ");
    
    QString resDir = QCoreApplication::applicationDirPath() + "/../resources/dreamcast/openmenu";
    if (!QDir(resDir).exists()) resDir = "resources/dreamcast/openmenu";
    QString datPath = resDir + "/" + (isIcon ? "ICON.DAT" : "BOX.DAT");
    
    OpenMenuDatManager manager;
    QImage img = manager.extractArtwork(datPath, gameId, isIcon);
    
    // Explicit hardware rotation and morton-curve horizontal mirror correction 
    if (!img.isNull()) {
        QTransform transform;
        transform.rotate(90);
        img = img.transformed(transform, Qt::SmoothTransformation);
        img = img.mirrored(true, false);
    }

    if (size) {
        *size = img.size();
    }
    return img;
}
