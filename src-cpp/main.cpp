#include "core/common/system_utils.h"
#include "core/i18n/translation_manager.h"
#include "core/logging/logger.h"
#include "platforms/dreamcast/dreamcast_library_service.h"
#include "platforms/dreamcast/ArtNDataUtils/openmenu_image_provider.h"
#include "platforms/ngc/swiss_library_service.h"
#include "platforms/ps2/opl_library_service.h"
#include "platforms/psx/ps1_xstation_library_service.h"
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>

int main(int argc, char *argv[]) {
  Logger::init();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

  qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
  QGuiApplication app(argc, argv);
  app.setWindowIcon(QIcon(":/app_icon.png"));
  app.setApplicationName("OdeRelic");
  app.setOrganizationName("OdeRelic");
  app.setOrganizationDomain("oderelic.github.io");

  QQmlApplicationEngine engine;

  // Set up dynamic multi-language architecture
  TranslationManager translationManager(&engine);
  engine.rootContext()->setContextProperty("translationManager",
                                           &translationManager);

  // Expose our C++ backend to QML
  OplLibraryService oplLibraryService;
  engine.rootContext()->setContextProperty("oplLibraryService",
                                           &oplLibraryService);

  SwissLibraryService swissLibraryService;
  engine.rootContext()->setContextProperty("swissLibraryService",
                                           &swissLibraryService);

  PS1XstationLibraryService ps1XstationLibraryService;
  engine.rootContext()->setContextProperty("ps1XstationLibraryService",
                                           &ps1XstationLibraryService);

  DreamcastLibraryService dreamcastLibraryService;
  engine.rootContext()->setContextProperty("dreamcastLibraryService",
                                           &dreamcastLibraryService);

  SystemUtils systemUtils;
  engine.rootContext()->setContextProperty("systemUtils", &systemUtils);

  // Hook dynamic DB extraction Image Provider
  engine.addImageProvider("openmenu", new OpenMenuImageProvider);

  const QUrl url(QStringLiteral("qrc:/main.qml"));
  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
          QCoreApplication::exit(-1);
      },
      Qt::QueuedConnection);
  engine.load(url);

  return app.exec();
}
