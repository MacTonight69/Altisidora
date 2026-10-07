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

    app.setApplicationName("MusicRep");
    app.setApplicationDisplayName("MusicRep");
    app.setOrganizationName("MusicRep");

    // Register C++ types with QML
    qmlRegisterType<PlayerController>("com.musicrep.audio", 1, 0, "PlayerController");
    qmlRegisterType<PlaybackState>("com.musicrep.audio", 1, 0, "PlaybackState");
    qmlRegisterType<LibraryController>("com.musicrep.library", 1, 0, "LibraryController");
    qmlRegisterType<PlaylistController>("com.musicrep.app", 1, 0, "PlaylistController");
    qmlRegisterType<SettingsController>("com.musicrep.app", 1, 0, "SettingsController");

    QQmlApplicationEngine engine;

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
