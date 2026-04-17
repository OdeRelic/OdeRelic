#ifndef OPENMENU_IMAGE_PROVIDER_H
#define OPENMENU_IMAGE_PROVIDER_H

#include <QQuickImageProvider>
#include "openmenu_dat_manager.h"

class OpenMenuImageProvider : public QQuickImageProvider {
public:
    OpenMenuImageProvider();
    
    /**
     * @brief Requests an image dynamically from the OpenMenu DAT manager for QML rendering.
     * @param id String containing the datPath, gameId, and type separated by | (e.g. "path|T-12345|box").
     * @param size Pointer to store the original size of the loaded image.
     * @param requestedSize The size requested by the QML Image element.
     * @return QImage containing the decoded graphic to render.
     */
    QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize) override;
};

#endif // OPENMENU_IMAGE_PROVIDER_H
