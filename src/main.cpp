// Aces High film companion - camera controls for the Aces High film viewer, through its socket only.
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "core.h"
#include "http_api.h"
#include "dock.h"
#include <QQuickWindow>
#include <QQuickStyle>
#include <QTimer>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("AcesHighFilmCompanion");
    QGuiApplication::setApplicationName("Aces High film companion");
    QQuickStyle::setStyle("Basic");   // the stock controls take the studio's dark palette (Main.qml)
    Core core;
    HttpApi api(&core, 8780);
    Dock dock;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("core", &core);
    engine.rootContext()->setContextProperty("api", &api);
    engine.rootContext()->setContextProperty("dock", &dock);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Companion", "Main");
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (window) dock.setCompanion(window);
    // --grab <file.png>: save a picture of the window after it has settled, then quit (for the manual's screenshots;
    // with QT_QPA_PLATFORM=offscreen nothing appears on screen).
    const int g = app.arguments().indexOf("--grab");
    if (window && g > 0 && g + 1 < app.arguments().size()) {
        const QString file = app.arguments().at(g + 1);
        QTimer::singleShot(2500, &app, [window, file] { window->grabWindow().save(file); QCoreApplication::quit(); });
    }
    return app.exec();
}
