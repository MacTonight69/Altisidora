#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSqlQuery>

#include "../../src/app/PlaylistController.h"
#include "../TestHelpers.h"

/**
 * @brief Tests for PlaylistController
 *
 * Verifies playlist CRUD operations and change notification.
 * The database is redirected to Qt's test mode so the user's real
 * library database is never touched, and it is removed before the
 * controller is created so every test run starts from scratch.
 */
class tst_playlist : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void testCreatePlaylist();
    void testCreateMultiplePlaylists();
    void testRenamePlaylist();
    void testDeletePlaylist();
    void testPlaylistsChangedSignal();
    void testGetPlaylistSongsForUnknownPlaylist();
    void testAddRemoveSongDoesNotCrash();
    void testAddSongsBatch();

private:
    void clearPlaylists();
    QList<qint64> playlistIds() const;

    PlaylistController *m_controller = nullptr;
};

void tst_playlist::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("tst_playlist"));

    TestHelpers::removeDatabaseFiles();
    m_controller = new PlaylistController(this);
}

void tst_playlist::cleanupTestCase()
{
    delete m_controller;
    m_controller = nullptr;
}

void tst_playlist::clearPlaylists()
{
    const QVector<QVariant> lists = m_controller->playlists();
    for (const QVariant &entry : lists) {
        m_controller->deletePlaylist(entry.toMap().value(QStringLiteral("id")).toLongLong());
    }
}

void tst_playlist::init()
{
    // Every test starts with no playlists
    clearPlaylists();
}

QList<qint64> tst_playlist::playlistIds() const
{
    QList<qint64> ids;
    const QVector<QVariant> lists = m_controller->playlists();
    for (const QVariant &entry : lists) {
        ids.append(entry.toMap().value(QStringLiteral("id")).toLongLong());
    }
    return ids;
}

void tst_playlist::testCreatePlaylist()
{
    m_controller->createPlaylist(QStringLiteral("Road Trip"));

    QCOMPARE(m_controller->playlists().size(), 1);
    QCOMPARE(m_controller->playlists().at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Road Trip"));
}

void tst_playlist::testCreateMultiplePlaylists()
{
    m_controller->createPlaylist(QStringLiteral("Favorites"));
    m_controller->createPlaylist(QStringLiteral("Workout"));
    m_controller->createPlaylist(QStringLiteral("Chill"));

    QCOMPARE(m_controller->playlists().size(), 3);
}

void tst_playlist::testRenamePlaylist()
{
    m_controller->createPlaylist(QStringLiteral("Old Name"));
    const QList<qint64> ids = playlistIds();
    QCOMPARE(ids.size(), 1);

    m_controller->renamePlaylist(ids.first(), QStringLiteral("New Name"));

    QCOMPARE(m_controller->playlists().size(), 1);
    QCOMPARE(m_controller->playlists().at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("New Name"));
}

void tst_playlist::testDeletePlaylist()
{
    m_controller->createPlaylist(QStringLiteral("Keep"));
    m_controller->createPlaylist(QStringLiteral("Remove"));

    const QList<qint64> ids = playlistIds();
    QCOMPARE(ids.size(), 2);

    // Delete the second playlist
    m_controller->deletePlaylist(ids.at(1));

    const QVector<QVariant> remaining = m_controller->playlists();
    QCOMPARE(remaining.size(), 1);
    QCOMPARE(remaining.at(0).toMap().value(QStringLiteral("name")).toString(),
             QStringLiteral("Keep"));
}

void tst_playlist::testPlaylistsChangedSignal()
{
    QSignalSpy spy(m_controller, &PlaylistController::playlistsChanged);
    QVERIFY(spy.isValid());

    m_controller->createPlaylist(QStringLiteral("Signal Test"));
    QCOMPARE(spy.count(), 1);

    const QList<qint64> ids = playlistIds();
    QCOMPARE(ids.size(), 1);

    m_controller->renamePlaylist(ids.first(), QStringLiteral("Renamed"));
    QCOMPARE(spy.count(), 2);

    m_controller->deletePlaylist(ids.first());
    QCOMPARE(spy.count(), 3);
}

void tst_playlist::testGetPlaylistSongsForUnknownPlaylist()
{
    // Unknown playlist id must return an empty list, not crash
    QVERIFY(m_controller->getPlaylistSongs(999999).isEmpty());
}

void tst_playlist::testAddRemoveSongDoesNotCrash()
{
    m_controller->createPlaylist(QStringLiteral("With Songs"));
    const QList<qint64> ids = playlistIds();
    QCOMPARE(ids.size(), 1);

    // Song 1 does not exist in the songs table; the JOIN in
    // getPlaylistSongs must simply not return it.
    m_controller->addSongs(ids.first(), QVariantList{1});
    QVERIFY(m_controller->getPlaylistSongs(ids.first()).isEmpty());

    // Removing it again must also be safe
    m_controller->removeSong(ids.first(), 1);
    QVERIFY(m_controller->getPlaylistSongs(ids.first()).isEmpty());
}

void tst_playlist::testAddSongsBatch()
{
    // Seed real songs so the JOIN in getPlaylistSongs() can resolve them.
    // Plain SQL on the shared default connection (no temporary
    // DatabaseManager instance: its destructor would close that connection).
    QList<qint64> songIds;
    for (int i = 1; i <= 3; ++i) {
        QSqlQuery insert;
        insert.prepare(QStringLiteral("INSERT INTO songs (path, title) VALUES (:path, :title)"));
        insert.bindValue(":path", QStringLiteral("/music/%1.mp3").arg(i));
        insert.bindValue(":title", QStringLiteral("Song %1").arg(i));
        QVERIFY(insert.exec());
        songIds.append(insert.lastInsertId().toLongLong());
    }

    m_controller->createPlaylist(QStringLiteral("Batch"));
    const QList<qint64> ids = playlistIds();
    QCOMPARE(ids.size(), 1);
    const qint64 playlistId = ids.first();

    QVariantList batch;
    for (const qint64 id : songIds)
        batch.append(id);

    QSignalSpy spy(m_controller, &PlaylistController::playlistsChanged);
    QVERIFY(spy.isValid());
    m_controller->addSongs(playlistId, batch);

    // The whole batch is notified exactly once.
    QCOMPARE(spy.count(), 1);

    // All songs were inserted, in the given order.
    const QVector<QVariant> songs = m_controller->getPlaylistSongs(playlistId);
    QCOMPARE(songs.size(), 3);
    QCOMPARE(songs.at(0).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Song 1"));
    QCOMPARE(songs.at(1).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Song 2"));
    QCOMPARE(songs.at(2).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Song 3"));

    // An empty batch must not emit anything.
    QSignalSpy emptySpy(m_controller, &PlaylistController::playlistsChanged);
    m_controller->addSongs(playlistId, QVariantList{});
    QCOMPARE(emptySpy.count(), 0);
    QCOMPARE(m_controller->getPlaylistSongs(playlistId).size(), 3);
}

QTEST_MAIN(tst_playlist)
#include "tst_playlist.moc"
