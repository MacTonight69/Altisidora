#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QIcon>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

#include "audio/PlayerController.h"
#include "audio/PlaybackState.h"
#include "library/LibraryController.h"
#include "app/PlaylistController.h"
#include "app/SettingsController.h"
#include "mpris/MprisPlayer.h"

namespace {

/**
 * @brief The user's personal music folder to default to on first launch.
 *
 * Prefers the standard Music location (respects the desktop locale, e.g.
 * ~/Música); falls back to the home folder when it does not exist.
 */
QString personalMusicFolder()
{
    const QString location = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (!location.isEmpty() && QFileInfo(location).isDir())
        return location;
    return QDir::homePath();
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    app.setApplicationName("Altisidora");
    app.setApplicationDisplayName("Altisidora");
    app.setOrganizationName("Altisidora");

    // Controller instances shared with QML as context properties
    PlayerController player;
    LibraryController library;
    PlaylistController playlists;
    SettingsController appSettings;

    // Load persisted library data on startup
    library.refresh();

    // First launch: no music folder is configured yet and the library is
    // empty, so the app defaults to the user's personal folder and scans
    // it once in the background (skipping audio <= 30 s and paths already
    // in the database). The folder is remembered, so later launches only
    // scan when the user asks for it from "Scan library".
    if (appSettings.musicDirectory().isEmpty() && library.songCount() == 0) {
        const QString personalFolder = personalMusicFolder();
        appSettings.setMusicDirectory(personalFolder);
        library.scanPersonalMusic(personalFolder);
    }

    // Expose playback to the desktop as an MPRIS media player, so MangoWM /
    // system media controls see the audio while it is playing.
    MprisPlayer mpris(&player);
    mpris.registerService();

    QQmlApplicationEngine engine;

    // Theme provider (QML component). Exposed as a context property so
    // every document can bind to Theme.<role> without imports; the object
    // lives for the whole application, parented to the engine.
    QQmlComponent themeComponent(&engine, QUrl(QStringLiteral("qrc:/qml/themes/Theme.qml")));
    if (QObject *theme = themeComponent.create()) {
        theme->setParent(&engine);
        engine.rootContext()->setContextProperty(QStringLiteral("Theme"), theme);
    }
    if (themeComponent.isError())
        qWarning() << themeComponent.errors();

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
