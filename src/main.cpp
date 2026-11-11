// Aces High film companion - camera controls for the Aces High film viewer, through its socket only.
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "core.h"
#include "http_api.h"
#include "dock.h"
#include <QQuickWindow>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("AcesHighFilmCompanion");
    QGuiApplication::setApplicationName("Aces High film companion");
    Core core;
    HttpApi api(&core, 8780);
    Dock dock;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("core", &core);
    engine.rootContext()->setContextProperty("api", &api);
    engine.rootContext()->setContextProperty("dock", &dock);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Companion", "Main");
    if (!engine.rootObjects().isEmpty()) dock.setCompanion(qobject_cast<QQuickWindow *>(engine.rootObjects().first()));
    return app.exec();
}
