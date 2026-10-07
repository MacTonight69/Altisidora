#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>

#include "audio/PlayerController.h"
#include "audio/PlaybackState.h"
#include "library/LibraryController.h"
#include "app/PlaylistController.h"
#include "app/SettingsController.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setApplicationName("Benzaiten");
    app.setApplicationDisplayName("Benzaiten");
    app.setOrganizationName("Benzaiten");

    // Register C++ types so QML can use them if needed
    qmlRegisterType<PlayerController>("com.musicrep.audio", 1, 0, "PlayerController");
    qmlRegisterType<PlaybackState>("com.musicrep.audio", 1, 0, "PlaybackState");
    qmlRegisterType<LibraryController>("com.musicrep.library", 1, 0, "LibraryController");
    qmlRegisterType<PlaylistController>("com.musicrep.app", 1, 0, "PlaylistController");
    qmlRegisterType<SettingsController>("com.musicrep.app", 1, 0, "SettingsController");

    // Controller instances shared with QML as context properties
    PlayerController player;
    LibraryController library;
    PlaylistController playlists;
    SettingsController appSettings;

    // Load persisted library data on startup (scan happens explicitly)
    library.refresh();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Player"), &player);
    engine.rootContext()->setContextProperty(QStringLiteral("Library"), &library);
    engine.rootContext()->setContextProperty(QStringLiteral("Playlists"), &playlists);
    engine.rootContext()->setContextProperty(QStringLiteral("AppSettings"), &appSettings);

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
