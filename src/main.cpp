#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QIcon>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QMouseEvent>
#include <QRectF>
#include <QUrl>

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

/**
 * @brief Applies the active theme's typeface as the application font.
 *
 * UI elements bind their font to Theme.fontFamily directly (that is what
 * restyles them live). This receiver is the complementary half: it sets the
 * application-wide font too, so surfaces Qt builds on its own — native
 * dialogs, menus and popups created after the switch — inherit the theme
 * typeface even though they have no binding of their own.
 */
class ThemeFontReceiver : public QObject
{
    Q_OBJECT
public:
    explicit ThemeFontReceiver(QObject *theme, QObject *parent = nullptr)
        : QObject(parent)
        , m_theme(theme)
    {
    }

public slots:
    void applyFont()
    {
        QGuiApplication::setFont(
            QFont(m_theme->property("fontFamily").toString()));
    }

private:
    QObject *m_theme = nullptr;
};

/**
 * @brief Blurs the search bar when the user clicks anywhere outside it.
 *
 * Most of the UI uses Qt.NoFocus controls, so nothing steals focus away
 * from the search TextField by itself. A window-level event filter drops
 * the search bar's focus on any mouse press outside its bounds.
 */
class SearchBarFocusFilter : public QObject
{
public:
    SearchBarFocusFilter(QQuickWindow *window, QQuickItem *searchBar)
        : m_window(window)
        , m_searchBar(searchBar)
    {
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() != QEvent::MouseButtonPress || !m_window || !m_searchBar)
            return QObject::eventFilter(watched, event);

        QQuickItem *focused = m_window->activeFocusItem();
        // Only act when the focus actually lives inside the search bar
        // (its internal text editor); leave other inputs alone.
        if (!focused || !isInside(focused, m_searchBar))
            return QObject::eventFilter(watched, event);

        const auto *mouse = static_cast<const QMouseEvent *>(event);
        const QPointF tl = m_searchBar->mapToScene(QPointF(0, 0));
        const QPointF br = m_searchBar->mapToScene(
            QPointF(m_searchBar->width(), m_searchBar->height()));
        if (!QRectF(tl, br).contains(mouse->scenePosition()))
            focused->setFocus(false); // clicking outside blurs the search bar

        return QObject::eventFilter(watched, event);
    }

private:
    static bool isInside(QQuickItem *child, QQuickItem *ancestor)
    {
        for (QQuickItem *it = child; it; it = it->parentItem()) {
            if (it == ancestor)
                return true;
        }
        return false;
    }

    QQuickWindow *m_window = nullptr;
    QQuickItem *m_searchBar = nullptr;
};

} // namespace

int main(int argc, char *argv[])
{
    // The native (GTK/portal) folder picker ignores Qt's currentFolder and
    // reopens wherever it last was. Force the built-in QML FolderDialog so
    // the Scan library picker can reliably open in the user's personal
    // folder. Must be set before the QGuiApplication is constructed.
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);

    QGuiApplication app(argc, argv);

    app.setApplicationName("Altisidora");
    app.setApplicationDisplayName("Altisidora");
    app.setOrganizationName("Altisidora");

    // Bundle the theme fonts so every machine sees the same typefaces
    // regardless of what is installed at the OS level. Registered before
    // the QML engine loads, so Theme.fontFamily resolves immediately.
    {
        static const char *kAppFonts[] = {
            ":/fonts/Baskervville.otf",
            ":/fonts/SunnyBrunch.ttf",
            ":/fonts/AgainstMyself.ttf",
            ":/fonts/Russian.ttf",
            ":/fonts/PixeloidSans.ttf",
            ":/fonts/HandmadeAlphabet.otf",
            ":/fonts/Elaesy.otf",
        };
        for (const char *path : kAppFonts) {
            if (QFontDatabase::addApplicationFont(QString::fromLatin1(path)) < 0)
                qWarning() << "Failed to register bundled font:" << path;
        }
    }

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

        // The theme's typeface drives the whole UI: it becomes the
        // application font so every Text and Control picks it up. Applied
        // once here (the theme already ran its initial apply()) and again
        // on every later theme change via the applied() signal.
        auto *fontReceiver = new ThemeFontReceiver(theme, &engine);
        fontReceiver->applyFont();
        QObject::connect(theme, SIGNAL(applied()), fontReceiver, SLOT(applyFont()));
    }
    if (themeComponent.isError())
        qWarning() << themeComponent.errors();

    engine.rootContext()->setContextProperty(QStringLiteral("Player"), &player);
    engine.rootContext()->setContextProperty(QStringLiteral("Library"), &library);
    engine.rootContext()->setContextProperty(QStringLiteral("Playlists"), &playlists);
    engine.rootContext()->setContextProperty(QStringLiteral("AppSettings"), &appSettings);
    // Where the "Scan library" picker starts: the same personal folder the
    // first-launch scan defaults to (Music location, else home folder).
    engine.rootContext()->setContextProperty(
        QStringLiteral("personalMusicFolder"),
        QUrl::fromLocalFile(personalMusicFolder()));

    engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    // Clicking anywhere outside the search bar drops its focus. The rest
    // of the UI is mostly Qt.NoFocus, so QML never blurs it on its own.
    if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
        if (auto *searchBar = qobject_cast<QQuickItem *>(
                window->findChild<QObject *>(QStringLiteral("searchBar")))) {
            // The filter must outlive this scope (it lives until the app exits):
            // parent it to the window it filters so it is not destroyed
            // as soon as the block ends.
            auto *blurFilter = new SearchBarFocusFilter(window, searchBar);
            blurFilter->setParent(window);
            window->installEventFilter(blurFilter);
        }
    }

    return app.exec();
}

#include "main.moc"
