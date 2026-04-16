#ifndef OPENMENU_IMAGE_PROVIDER_H
#define OPENMENU_IMAGE_PROVIDER_H

#include <QQuickImageProvider>
#include "openmenu_dat_manager.h"

class OpenMenuImageProvider : public QQuickImageProvider {
public:
    OpenMenuImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize& requestedSize) override;
};

#endif // OPENMENU_IMAGE_PROVIDER_H
