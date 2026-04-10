#include "core/opl_library_service.h"
#include "core/swiss_library_service.h"
#include "core/translation_manager.h"
#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QDebug>

int main(int argc, char *argv[]) {
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
  engine.rootContext()->setContextProperty("translationManager", &translationManager);

  // Expose our C++ backend to QML
  OplLibraryService oplLibraryService;
  engine.rootContext()->setContextProperty("oplLibraryService",
                                           &oplLibraryService);

  SwissLibraryService swissLibraryService;
  engine.rootContext()->setContextProperty("swissLibraryService",
                                           &swissLibraryService);

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
