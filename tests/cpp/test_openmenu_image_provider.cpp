#include <QtTest>
#include <QImage>
#include "openmenu_image_provider.h"

class TestOpenMenuImageProvider : public QObject {
    Q_OBJECT

private slots:
    void testImageResolution() {
        OpenMenuImageProvider provider;
        
        QSize requestedSize;
        // Since we don't have a mock BOX.DAT with valid images mounted here, 
        // the provider should gracefully return a null/empty image when the ID isn't found.
        QImage result = provider.requestImage("NON_EXISTENT_ID", nullptr, requestedSize);
        QVERIFY(result.isNull());
    }
};

QTEST_MAIN(TestOpenMenuImageProvider)
#include "test_openmenu_image_provider.moc"
