#include <QtTest>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QStandardPaths>
#include <QUrl>

#include "../../src/app/PlaylistController.h"
#include "../../src/app/SettingsController.h"
#include "../../src/audio/PlayerController.h"
#include "../../src/library/LibraryController.h"

// Directories holding the real QML, injected by tests/CMakeLists.txt.
#ifndef THEME_QML_DIR
#define THEME_QML_DIR ""
#endif
#ifndef VIEWS_QML_DIR
#define VIEWS_QML_DIR ""
#endif

/**
 * @brief Loads the album/artist drill-down view with the real QML and the
 * real controllers, so the document itself is exercised.
 *
 * SongListDetailView is created lazily by main.qml (it lives inside a
 * Component, on purpose: the drill-down is pushed only when an album or
 * artist is opened), so neither a smoke run nor any other test ever
 * instantiates it. Creating it here is the only check that catches the class
 * of error which appears when the document is built rather than when it is
 * parsed — an unknown property, a missing id, a broken binding — including
 * the album edit dialog that adds the artist and cover fields.
 */
class tst_detail_view : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testDetailViewLoadsWithAlbumEditDialog();

private:
    QQmlEngine *m_engine = nullptr;
    QObject *m_theme = nullptr;
    SettingsController *m_settings = nullptr;
    LibraryController *m_library = nullptr;
    PlaylistController *m_playlists = nullptr;
    PlayerController *m_player = nullptr;
};

void tst_detail_view::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("tst_detail_view"));

    m_engine = new QQmlEngine(this);

    // The same wiring main.cpp performs: Theme.qml is instantiated once and
    // exposed as a context property, alongside the controllers.
    QQmlComponent themeComponent(
        m_engine,
        QUrl::fromLocalFile(QStringLiteral(THEME_QML_DIR)
                            + QStringLiteral("/Theme.qml")));
    m_theme = themeComponent.create();
    QVERIFY2(m_theme != nullptr, qPrintable(themeComponent.errorString()));

    m_settings = new SettingsController(this);
    m_library = new LibraryController(this);
    m_playlists = new PlaylistController(this);
    m_player = new PlayerController(this);

    QQmlContext *context = m_engine->rootContext();
    context->setContextProperty(QStringLiteral("Theme"), m_theme);
    context->setContextProperty(QStringLiteral("Library"), m_library);
    context->setContextProperty(QStringLiteral("AppSettings"), m_settings);
    context->setContextProperty(QStringLiteral("Playlists"), m_playlists);
    context->setContextProperty(QStringLiteral("Player"), m_player);
}

void tst_detail_view::cleanupTestCase()
{
    delete m_player;
    m_player = nullptr;
    delete m_playlists;
    m_playlists = nullptr;
    delete m_library;
    m_library = nullptr;
    delete m_settings;
    m_settings = nullptr;
    delete m_theme;
    m_theme = nullptr;
    delete m_engine;
    m_engine = nullptr;
}

void tst_detail_view::testDetailViewLoadsWithAlbumEditDialog()
{
    QQmlComponent component(
        m_engine,
        QUrl::fromLocalFile(QStringLiteral(VIEWS_QML_DIR)
                            + QStringLiteral("/SongListDetailView.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    // Creating the document is what a smoke run never does.
    QObject *view = component.create();
    QVERIFY2(view != nullptr, qPrintable(component.errorString()));
    QVERIFY(qobject_cast<QQuickItem *>(view) != nullptr);

    // The album edit dialog must come up carrying the album-scoped edit
    // state it needs for the artist and cover fields.
    QObject *dialog = nullptr;
    const QList<QObject *> children = view->findChildren<QObject *>();
    for (QObject *child : children) {
        if (child->property("pickedArtwork").isValid()) {
            dialog = child;
            break;
        }
    }
    QVERIFY2(dialog != nullptr, "the album edit dialog was not created");
    QVERIFY(dialog->property("removeArtwork").isValid());
    QVERIFY(dialog->property("effectiveArtwork").isValid());

    // Both default to "no pending cover change", so merely opening the
    // dialog cannot wipe the album's artwork.
    QCOMPARE(dialog->property("pickedArtwork").toString(), QString());
    QCOMPARE(dialog->property("removeArtwork").toBool(), false);
    QCOMPARE(dialog->property("effectiveArtwork").toString(), QString());

    delete view;
}

QTEST_MAIN(tst_detail_view)
#include "tst_detail_view.moc"
