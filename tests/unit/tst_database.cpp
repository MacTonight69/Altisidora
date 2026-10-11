#include <QtTest>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QFile>
#include <QTemporaryDir>
#include <QDateTime>

#include "database/DatabaseManager.h"

/**
 * @brief Tests for the real database layer (src/database/DatabaseManager.cpp).
 *
 * Every test runs against a fresh SQLite file in a temporary directory.
 */
class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void testOpenCreatesSchema();
    void testAddAndGetSongWithNormalization();
    void testDuplicateSongPathRejected();
    void testRemoveSong();
    void testRemoveSongCleansPlaylistReferences();
    void testRemoveSongCleansOrphanEntities();
    void testSearchSongs();
    void testSongsByAlbum();
    void testSongsByArtist();
    void testGetAllArtistsAggregates();
    void testGetAllAlbumsAggregates();
    void testPlaylistLifecycle();
    void testPlaylistSongs();
    void testIgnoredPathsLifecycle();
    void testIgnoredFileHiddenFromQueries();
    void testIgnoredDirectoryHidesChildren();
    void testSchemaMigrationAddsFileStats();
    void testSchemaMigrationBackfillsArtwork();
    void testSongFileStatsReturnsStoredValues();
    void testUpsertSongInsertsNewSong();
    void testUpsertSongUpdatesMetadataButKeepsUserData();
    void testRefreshAlbumArtwork();

private:
    DatabaseManager m_db;
    QTemporaryDir m_tempDir;
};

void TestDatabase::init()
{
    m_db.close();
    const QString path = m_tempDir.filePath(QStringLiteral("test.db"));
    QFile::remove(path);
    QVERIFY(m_db.open(path));
}

void TestDatabase::cleanup()
{
    m_db.close();
}

void TestDatabase::testOpenCreatesSchema()
{
    QVERIFY(m_db.isOpen());

    QSqlQuery query(QSqlDatabase::database());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='songs'")));
    QVERIFY(query.next());
}

void TestDatabase::testAddAndGetSongWithNormalization()
{
    Song song;
    song.path = QStringLiteral("/music/one.mp3");
    song.title = QStringLiteral("One");
    song.artist = QStringLiteral("Artist A");
    song.album = QStringLiteral("Album X");
    song.genre = QStringLiteral("Rock");
    song.year = 2020;
    song.trackNumber = 1;
    song.duration = 200000;

    QVERIFY(m_db.addSong(song));
    QVERIFY(song.id > 0);

    // Names must be resolved back from the normalized tables.
    const Song stored = m_db.getSong(song.id);
    QCOMPARE(stored.path, song.path);
    QCOMPARE(stored.title, song.title);
    QCOMPARE(stored.artist, QStringLiteral("Artist A"));
    QCOMPARE(stored.album, QStringLiteral("Album X"));
    QCOMPARE(stored.genre, QStringLiteral("Rock"));
    QCOMPARE(stored.year, 2020);
    QCOMPARE(stored.trackNumber, 1);
    QCOMPARE(stored.duration, qint64(200000));
}

void TestDatabase::testDuplicateSongPathRejected()
{
    Song first;
    first.path = QStringLiteral("/music/dup.mp3");
    first.title = QStringLiteral("First");
    QVERIFY(m_db.addSong(first));

    Song second;
    second.path = QStringLiteral("/music/dup.mp3");
    second.title = QStringLiteral("Second");
    QVERIFY(!m_db.addSong(second));

    QCOMPARE(m_db.getAllSongs().size(), 1);
}

void TestDatabase::testRemoveSong()
{
    Song song;
    song.path = QStringLiteral("/music/del.mp3");
    QVERIFY(m_db.addSong(song));

    QVERIFY(m_db.removeSong(song.id));
    QCOMPARE(m_db.getSong(song.id).id, 0);
    QCOMPARE(m_db.getAllSongs().size(), 0);
}

void TestDatabase::testRemoveSongCleansPlaylistReferences()
{
    Playlist playlist;
    playlist.name = QStringLiteral("DeleteMe");
    QVERIFY(m_db.addPlaylist(playlist));

    Song keep;
    keep.path = QStringLiteral("/music/keep.mp3");
    Song gone;
    gone.path = QStringLiteral("/music/gone.mp3");
    QVERIFY(m_db.addSong(keep));
    QVERIFY(m_db.addSong(gone));

    QVERIFY(m_db.addSongToPlaylist(playlist.id, keep.id, 1));
    QVERIFY(m_db.addSongToPlaylist(playlist.id, gone.id, 2));
    QCOMPARE(m_db.getPlaylistSongs(playlist.id).size(), 2);

    // Deleting a song also removes its (orphaned) playlist references.
    QVERIFY(m_db.removeSong(gone.id));
    QCOMPARE(m_db.getSong(gone.id).id, 0);
    const QList<Song> remaining = m_db.getPlaylistSongs(playlist.id);
    QCOMPARE(remaining.size(), 1);
    QCOMPARE(remaining.first().path, keep.path);

    // The playlist itself is unaffected.
    QCOMPARE(m_db.getAllPlaylists().size(), 1);
}

void TestDatabase::testRemoveSongCleansOrphanEntities()
{
    Song song;
    song.path = QStringLiteral("/music/orphan.mp3");
    song.artist = QStringLiteral("Orphan Artist");
    song.album = QStringLiteral("Orphan Album");
    song.genre = QStringLiteral("Orphan Genre");
    QVERIFY(m_db.addSong(song));
    QCOMPARE(m_db.getAllArtists().size(), 1);
    QCOMPARE(m_db.getAllAlbums().size(), 1);

    QVERIFY(m_db.removeSong(song.id));

    // Artists/albums without songs are cleaned up.
    QCOMPARE(m_db.getAllArtists().size(), 0);
    QCOMPARE(m_db.getAllAlbums().size(), 0);
}

void TestDatabase::testSearchSongs()
{
    Song rhapsody;
    rhapsody.path = QStringLiteral("/music/rhapsody.mp3");
    rhapsody.title = QStringLiteral("Bohemian Rhapsody");

    Song likeYou;
    likeYou.path = QStringLiteral("/music/likeyou.mp3");
    likeYou.title = QStringLiteral("Bohemian Like You");

    Song stairway;
    stairway.path = QStringLiteral("/music/stairway.mp3");
    stairway.title = QStringLiteral("Stairway to Heaven");

    QVERIFY(m_db.addSong(rhapsody));
    QVERIFY(m_db.addSong(likeYou));
    QVERIFY(m_db.addSong(stairway));

    // SQLite LIKE is case-insensitive for ASCII.
    const QList<Song> results = m_db.searchSongs(QStringLiteral("bohemian"));
    QCOMPARE(results.size(), 2);

    const QList<Song> none = m_db.searchSongs(QStringLiteral("zzz"));
    QCOMPARE(none.size(), 0);
}

void TestDatabase::testSongsByAlbum()
{
    Song t1;
    t1.path = QStringLiteral("/music/aa/t1.mp3");
    t1.title = QStringLiteral("Track 1");
    t1.artist = QStringLiteral("Artist A");
    t1.album = QStringLiteral("Album X");
    t1.trackNumber = 2;

    Song t2;
    t2.path = QStringLiteral("/music/aa/t2.mp3");
    t2.title = QStringLiteral("Track 2");
    t2.artist = QStringLiteral("Artist A");
    t2.album = QStringLiteral("Album X");
    t2.trackNumber = 1;

    // Same album title under a different artist must not be returned.
    Song other;
    other.path = QStringLiteral("/music/ab/other.mp3");
    other.title = QStringLiteral("Other");
    other.artist = QStringLiteral("Artist B");
    other.album = QStringLiteral("Album X");

    Song elsewhere;
    elsewhere.path = QStringLiteral("/music/other.mp3");
    elsewhere.title = QStringLiteral("Elsewhere");
    elsewhere.artist = QStringLiteral("Artist A");
    elsewhere.album = QStringLiteral("Album Y");

    QVERIFY(m_db.addSong(t1));
    QVERIFY(m_db.addSong(t2));
    QVERIFY(m_db.addSong(other));
    QVERIFY(m_db.addSong(elsewhere));

    // Narrowed by album title + artist.
    const QList<Song> exact = m_db.getSongsByAlbum(QStringLiteral("Album X"),
                                                   QStringLiteral("Artist A"));
    QCOMPARE(exact.size(), 2);
    // Ordered by track number (behaves like a playlist).
    QCOMPARE(exact.at(0).title, QStringLiteral("Track 2"));
    QCOMPARE(exact.at(1).title, QStringLiteral("Track 1"));

    // Same title under another artist is kept apart.
    const QList<Song> otherArtist =
        m_db.getSongsByAlbum(QStringLiteral("Album X"), QStringLiteral("Artist B"));
    QCOMPARE(otherArtist.size(), 1);
    QCOMPARE(otherArtist.first().title, QStringLiteral("Other"));

    // Empty artist widens the match to any album with that title.
    const QList<Song> anyArtist = m_db.getSongsByAlbum(QStringLiteral("Album X"));
    QCOMPARE(anyArtist.size(), 3);

    const QList<Song> none = m_db.getSongsByAlbum(QStringLiteral("zzz"));
    QCOMPARE(none.size(), 0);
}

void TestDatabase::testSongsByArtist()
{
    Song a1;
    a1.path = QStringLiteral("/music/a1.mp3");
    a1.title = QStringLiteral("A One");
    a1.artist = QStringLiteral("Artist A");
    a1.album = QStringLiteral("Album X");
    a1.trackNumber = 1;

    Song a2;
    a2.path = QStringLiteral("/music/a2.mp3");
    a2.title = QStringLiteral("A Two");
    a2.artist = QStringLiteral("Artist A");
    a2.album = QStringLiteral("Album Y");
    a2.trackNumber = 2;

    Song b1;
    b1.path = QStringLiteral("/music/b1.mp3");
    b1.title = QStringLiteral("B One");
    b1.artist = QStringLiteral("Artist B");
    b1.album = QStringLiteral("Album Z");

    Song hidden;
    hidden.path = QStringLiteral("/music/hidden.mp3");
    hidden.title = QStringLiteral("Hidden");
    hidden.artist = QStringLiteral("Artist A");
    hidden.album = QStringLiteral("Album Z");

    QVERIFY(m_db.addSong(a1));
    QVERIFY(m_db.addSong(a2));
    QVERIFY(m_db.addSong(b1));
    QVERIFY(m_db.addSong(hidden));

    // Full set for the artist before hiding anything.
    QCOMPARE(m_db.getSongsByArtist(QStringLiteral("Artist A")).size(), 3);

    // Hidden songs are never part of the drill-down playlist.
    QVERIFY(m_db.addIgnoredPath(hidden.path));
    const QList<Song> results = m_db.getSongsByArtist(QStringLiteral("Artist A"));
    QCOMPARE(results.size(), 2);
    // Ordered by album, then by track: playlist-like.
    QCOMPARE(results.at(0).path, a1.path);
    QCOMPARE(results.at(1).path, a2.path);

    QCOMPARE(m_db.getSongsByArtist(QStringLiteral("Artist B")).size(), 1);

    QCOMPARE(m_db.getSongsByArtist(QStringLiteral("zzz")).size(), 0);
}

void TestDatabase::testGetAllArtistsAggregates()
{
    Song a1;
    a1.path = QStringLiteral("/music/a1.mp3");
    a1.artist = QStringLiteral("Artist A");
    Song a2;
    a2.path = QStringLiteral("/music/a2.mp3");
    a2.artist = QStringLiteral("Artist A");
    Song b1;
    b1.path = QStringLiteral("/music/b1.mp3");
    b1.artist = QStringLiteral("Artist B");

    QVERIFY(m_db.addSong(a1));
    QVERIFY(m_db.addSong(a2));
    QVERIFY(m_db.addSong(b1));

    const QList<Artist> artists = m_db.getAllArtists();
    QCOMPARE(artists.size(), 2);
    for (const Artist &artist : artists) {
        if (artist.name == QStringLiteral("Artist A")) {
            QCOMPARE(artist.songCount, 2);
        } else {
            QCOMPARE(artist.name, QStringLiteral("Artist B"));
            QCOMPARE(artist.songCount, 1);
        }
    }
}

void TestDatabase::testGetAllAlbumsAggregates()
{
    Song x1;
    x1.path = QStringLiteral("/music/x1.mp3");
    x1.artist = QStringLiteral("Artist A");
    x1.album = QStringLiteral("Album X");
    Song x2;
    x2.path = QStringLiteral("/music/x2.mp3");
    x2.artist = QStringLiteral("Artist A");
    x2.album = QStringLiteral("Album X");
    Song y1;
    y1.path = QStringLiteral("/music/y1.mp3");
    y1.artist = QStringLiteral("Artist B");
    y1.album = QStringLiteral("Album Y");

    QVERIFY(m_db.addSong(x1));
    QVERIFY(m_db.addSong(x2));
    QVERIFY(m_db.addSong(y1));

    const QList<Album> albums = m_db.getAllAlbums();
    QCOMPARE(albums.size(), 2);
    for (const Album &album : albums) {
        if (album.title == QStringLiteral("Album X")) {
            QCOMPARE(album.artist, QStringLiteral("Artist A"));
            QCOMPARE(album.songCount, 2);
        } else {
            QCOMPARE(album.title, QStringLiteral("Album Y"));
            QCOMPARE(album.artist, QStringLiteral("Artist B"));
            QCOMPARE(album.songCount, 1);
        }
    }
}

void TestDatabase::testPlaylistLifecycle()
{
    Playlist playlist;
    playlist.name = QStringLiteral("Favorites");
    playlist.dateCreated = QDateTime::currentDateTime();
    QVERIFY(m_db.addPlaylist(playlist));
    QVERIFY(playlist.id > 0);

    QCOMPARE(m_db.getAllPlaylists().size(), 1);
    QCOMPARE(m_db.getAllPlaylists().first().name, QStringLiteral("Favorites"));
    QCOMPARE(m_db.getAllPlaylists().first().songCount, 0);

    playlist.name = QStringLiteral("Renamed");
    QVERIFY(m_db.updatePlaylist(playlist));
    QCOMPARE(m_db.getAllPlaylists().first().name, QStringLiteral("Renamed"));

    Song song;
    song.path = QStringLiteral("/music/plsong.mp3");
    QVERIFY(m_db.addSong(song));

    QVERIFY(m_db.addSongToPlaylist(playlist.id, song.id, 0));
    QCOMPARE(m_db.getAllPlaylists().first().songCount, 1);

    QVERIFY(m_db.removePlaylist(playlist.id));
    QCOMPARE(m_db.getAllPlaylists().size(), 0);
}

void TestDatabase::testPlaylistSongs()
{
    Playlist playlist;
    playlist.name = QStringLiteral("Ordered");
    QVERIFY(m_db.addPlaylist(playlist));

    Song first;
    first.path = QStringLiteral("/music/pl_first.mp3");
    first.title = QStringLiteral("First");
    Song second;
    second.path = QStringLiteral("/music/pl_second.mp3");
    second.title = QStringLiteral("Second");
    QVERIFY(m_db.addSong(first));
    QVERIFY(m_db.addSong(second));

    QVERIFY(m_db.addSongToPlaylist(playlist.id, first.id, 1));
    QVERIFY(m_db.addSongToPlaylist(playlist.id, second.id, 2));

    const QList<Song> songs = m_db.getPlaylistSongs(playlist.id);
    QCOMPARE(songs.size(), 2);
    // Ordered by position.
    QCOMPARE(songs.at(0).path, first.path);
    QCOMPARE(songs.at(1).path, second.path);

    QVERIFY(m_db.removeSongFromPlaylist(playlist.id, first.id));
    QCOMPARE(m_db.getPlaylistSongs(playlist.id).size(), 1);
}

void TestDatabase::testIgnoredPathsLifecycle()
{
    QVERIFY(m_db.addIgnoredPath(QStringLiteral("/music/skip.mp3"), /*isDir=*/false));
    QVERIFY(m_db.addIgnoredPath(QStringLiteral("/music/skipdir"), /*isDir=*/true));

    const QStringList paths = m_db.ignoredPaths();
    QCOMPARE(paths.size(), 2);
    QVERIFY(paths.contains(QStringLiteral("/music/skip.mp3")));
    QVERIFY(paths.contains(QStringLiteral("/music/skipdir")));

    // Adding the same path again is a no-op (UNIQUE constraint).
    QVERIFY(m_db.addIgnoredPath(QStringLiteral("/music/skip.mp3")));
    QCOMPARE(m_db.ignoredPaths().size(), 2);

    QVERIFY(m_db.removeIgnoredPath(QStringLiteral("/music/skip.mp3")));
    QCOMPARE(m_db.ignoredPaths().size(), 1);
    QVERIFY(!m_db.ignoredPaths().contains(QStringLiteral("/music/skip.mp3")));
}

void TestDatabase::testIgnoredFileHiddenFromQueries()
{
    Song keep;
    keep.path = QStringLiteral("/music/keep.mp3");
    keep.title = QStringLiteral("Keep");
    keep.artist = QStringLiteral("Artist A");
    keep.album = QStringLiteral("Album X");
    keep.genre = QStringLiteral("Rock");

    Song hide;
    hide.path = QStringLiteral("/music/hide.mp3");
    hide.title = QStringLiteral("Hide");
    hide.artist = QStringLiteral("Artist A");
    hide.album = QStringLiteral("Album X");
    hide.genre = QStringLiteral("Rock");

    QVERIFY(m_db.addSong(keep));
    QVERIFY(m_db.addSong(hide));

    // The hidden song is ignored by the "ignore" feature: the row stays in
    // the database (getSong still finds it) but every list query hides it.
    QVERIFY(m_db.addIgnoredPath(hide.path));

    // getSong is used for playback of an already-known song: not filtered.
    QCOMPARE(m_db.getSong(hide.id).path, hide.path);

    QCOMPARE(m_db.getAllSongs().size(), 1);
    QCOMPARE(m_db.getAllSongs().first().path, keep.path);

    // Search also filters hidden songs.
    const QList<Song> searchResults = m_db.searchSongs(QStringLiteral("hide"));
    QCOMPARE(searchResults.size(), 0);
    const QList<Song> visibleSearch = m_db.searchSongs(QStringLiteral("keep"));
    QCOMPARE(visibleSearch.size(), 1);

    // Aggregate counts discount the hidden song.
    QCOMPARE(m_db.getAllArtists().first().songCount, 1);
    QCOMPARE(m_db.getAllAlbums().first().songCount, 1);

    // Playlist membership keeps the song, but the listing filters it.
    Playlist playlist;
    playlist.name = QStringLiteral("WithHidden");
    QVERIFY(m_db.addPlaylist(playlist));
    QVERIFY(m_db.addSongToPlaylist(playlist.id, keep.id, 1));
    QVERIFY(m_db.addSongToPlaylist(playlist.id, hide.id, 2));
    QCOMPARE(m_db.getPlaylistSongs(playlist.id).size(), 1);
    QCOMPARE(m_db.getPlaylistSongs(playlist.id).first().path, keep.path);

    // Un-hiding makes the song visible again without a rescan.
    QVERIFY(m_db.removeIgnoredPath(hide.path));
    QCOMPARE(m_db.getAllSongs().size(), 2);
}

void TestDatabase::testIgnoredDirectoryHidesChildren()
{
    Song child1;
    child1.path = QStringLiteral("/music/album/one.mp3");
    Song child2;
    child2.path = QStringLiteral("/music/album/two.flac");
    Song outside;
    outside.path = QStringLiteral("/music/outside.ogg");

    QVERIFY(m_db.addSong(child1));
    QVERIFY(m_db.addSong(child2));
    QVERIFY(m_db.addSong(outside));

    QVERIFY(m_db.addIgnoredPath(QStringLiteral("/music/album"), /*isDir=*/true));
    QCOMPARE(m_db.getAllSongs().size(), 1);
    QCOMPARE(m_db.getAllSongs().first().path, outside.path);
}

void TestDatabase::testSchemaMigrationAddsFileStats()
{
    // Build a v1 database by hand: songs without mtime/size, schema_version
    // at 1, plus a row that must survive the migration untouched.
    const QString legacyPath = m_tempDir.filePath(QStringLiteral("legacy.db"));
    QFile::remove(legacyPath);
    {
        const QString connName = QStringLiteral("legacy_setup");
        {
            QSqlDatabase setup = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            setup.setDatabaseName(legacyPath);
            QVERIFY(setup.open());
            {
                QSqlQuery q(setup);
                QVERIFY(q.exec(QStringLiteral(
                    "CREATE TABLE songs ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT UNIQUE NOT NULL, "
                    "title TEXT, artist_id INTEGER, album_id INTEGER, genre_id INTEGER, "
                    "year INTEGER, track_number INTEGER, disc_number INTEGER, "
                    "duration INTEGER, artwork_path TEXT, date_added TEXT, "
                    "last_played TEXT, play_count INTEGER DEFAULT 0, "
                    "favorite INTEGER DEFAULT 0)")));
                QVERIFY(q.exec(QStringLiteral(
                    "INSERT INTO songs (path, title, play_count, favorite) "
                    "VALUES ('/music/legacy.mp3', 'Legacy', 7, 1)")));
                QVERIFY(q.exec(QStringLiteral("CREATE TABLE schema_version (version INTEGER)")));
                QVERIFY(q.exec(QStringLiteral("INSERT INTO schema_version (version) VALUES (1)")));
            }
            setup.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    // Reopen through DatabaseManager: free the shared connection first, then
    // createTables() migrates the v1 schema in place.
    m_db.close();
    DatabaseManager migrated;
    QVERIFY(migrated.open(legacyPath));

    // mtime/size now exist.
    bool hasMtime = false;
    bool hasSize = false;
    {
        QSqlQuery info(QSqlDatabase::database());
        QVERIFY(info.exec(QStringLiteral("PRAGMA table_info(songs)")));
        while (info.next()) {
            const QString name = info.value(QStringLiteral("name")).toString();
            hasMtime = hasMtime || name == QStringLiteral("mtime");
            hasSize = hasSize || name == QStringLiteral("size");
        }
    }
    QVERIFY(hasMtime);
    QVERIFY(hasSize);

    // The legacy row is intact (no data loss, user data preserved).
    const QList<Song> songs = migrated.getAllSongs();
    QCOMPARE(songs.size(), 1);
    QCOMPARE(songs.first().path, QStringLiteral("/music/legacy.mp3"));
    QCOMPARE(songs.first().playCount, 7);
    QVERIFY(songs.first().favorite);

    // And the stored schema version is 3 (bumped by the artwork backfill
    // migration), as a single row.
    QSqlQuery versionQuery(QSqlDatabase::database());
    QVERIFY(versionQuery.exec(QStringLiteral("SELECT version FROM schema_version")));
    QVERIFY(versionQuery.next());
    QCOMPARE(versionQuery.value(0).toInt(), 3);
    QVERIFY(!versionQuery.next());
}

// Schema v3 (embedded artwork): a v2 database whose rows were written
// without artwork extraction must have its artwork-less songs marked stale
// (mtime = 0) so the next scan re-reads them and backfills artwork_path.
// The migration runs once (guarded by the stored version), so a file that
// genuinely has no embedded art is not forced into a rescan loop.
void TestDatabase::testSchemaMigrationBackfillsArtwork()
{
    // Build a v2 database by hand: mtime/size exist and hold real
    // fingerprints, schema_version at 2.
    const QString v2Path = m_tempDir.filePath(QStringLiteral("v2.db"));
    QFile::remove(v2Path);
    {
        const QString connName = QStringLiteral("v2_setup");
        {
            QSqlDatabase setup = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            setup.setDatabaseName(v2Path);
            QVERIFY(setup.open());
            {
                QSqlQuery q(setup);
                QVERIFY(q.exec(QStringLiteral(
                    "CREATE TABLE songs ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT UNIQUE NOT NULL, "
                    "title TEXT, artist_id INTEGER, album_id INTEGER, genre_id INTEGER, "
                    "year INTEGER, track_number INTEGER, disc_number INTEGER, "
                    "duration INTEGER, artwork_path TEXT, mtime INTEGER, size INTEGER, "
                    "date_added TEXT, last_played TEXT, play_count INTEGER DEFAULT 0, "
                    "favorite INTEGER DEFAULT 0)")));
                QVERIFY(q.exec(QStringLiteral(
                    "INSERT INTO songs (path, artwork_path, mtime, size) "
                    "VALUES ('/music/needs_art.mp3', NULL, 111, 1)")));
                QVERIFY(q.exec(QStringLiteral(
                    "INSERT INTO songs (path, artwork_path, mtime, size) "
                    "VALUES ('/music/has_art.mp3', '/cache/a.png', 222, 2)")));
                QVERIFY(q.exec(QStringLiteral("CREATE TABLE schema_version (version INTEGER)")));
                QVERIFY(q.exec(QStringLiteral("INSERT INTO schema_version (version) VALUES (2)")));
            }
            setup.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    m_db.close();
    DatabaseManager migrated;
    QVERIFY(migrated.open(v2Path));

    {
        const auto stats = migrated.songFileStats();
        // Artwork-less row: the fingerprint was broken -> forced rescan.
        QCOMPARE(stats.value(QStringLiteral("/music/needs_art.mp3")).first, 0LL);
        // Artwork-bearing row: untouched.
        QCOMPARE(stats.value(QStringLiteral("/music/has_art.mp3")).first, 222LL);
    }

    // Simulate the rescan the backfill triggers: the file was re-read and
    // got a fresh fingerprint (it simply has no embedded art).
    {
        QSqlQuery q(QSqlDatabase::database());
        QVERIFY(q.exec(QStringLiteral(
            "UPDATE songs SET mtime = 999, size = 42 "
            "WHERE path = '/music/needs_art.mp3'")));
    }

    // Reopen at v3: the version guard stops the migration from running
    // again, so the fingerprint survives (no permanent rescan loop).
    migrated.close();
    QVERIFY(migrated.open(v2Path));

    const auto statsAgain = migrated.songFileStats();
    QCOMPARE(statsAgain.value(QStringLiteral("/music/needs_art.mp3")).first, 999LL);
    QCOMPARE(statsAgain.value(QStringLiteral("/music/has_art.mp3")).first, 222LL);
}

void TestDatabase::testSongFileStatsReturnsStoredValues()
{
    Song song;
    song.path = QStringLiteral("/music/stats.mp3");
    song.title = QStringLiteral("Stats");
    song.mtimeMs = 1234567890123LL;
    song.size = 4242;
    QVERIFY(m_db.upsertSong(song));

    const auto stats = m_db.songFileStats();
    QVERIFY(stats.contains(song.path));
    QCOMPARE(stats.value(song.path).first, 1234567890123LL);
    QCOMPARE(stats.value(song.path).second, 4242LL);

    // Unknown paths are absent.
    QVERIFY(!stats.contains(QStringLiteral("/music/nope.mp3")));
}

void TestDatabase::testUpsertSongInsertsNewSong()
{
    Song song;
    song.path = QStringLiteral("/music/new.mp3");
    song.title = QStringLiteral("Brand New");
    song.artist = QStringLiteral("Artist A");
    song.album = QStringLiteral("Album X");
    song.genre = QStringLiteral("Rock");
    song.year = 2024;
    song.duration = 12345;
    song.mtimeMs = 111;
    song.size = 222;

    QVERIFY(m_db.upsertSong(song));
    QVERIFY(song.id > 0);

    const Song stored = m_db.getSong(song.id);
    QCOMPARE(stored.path, song.path);
    QCOMPARE(stored.title, QStringLiteral("Brand New"));
    QCOMPARE(stored.artist, QStringLiteral("Artist A"));
    QCOMPARE(stored.album, QStringLiteral("Album X"));
    QCOMPARE(stored.genre, QStringLiteral("Rock"));
    QCOMPARE(stored.duration, qint64(12345));
}

void TestDatabase::testUpsertSongUpdatesMetadataButKeepsUserData()
{
    Song song;
    song.path = QStringLiteral("/music/rescan.mp3");
    song.title = QStringLiteral("Original");
    song.artist = QStringLiteral("Artist A");
    song.album = QStringLiteral("Album X");
    song.duration = 1000;
    song.mtimeMs = 100;
    song.size = 200;
    QVERIFY(m_db.upsertSong(song));
    const int id = song.id;
    QVERIFY(id > 0);

    // Simulate user data a rescan must never clobber.
    {
        QSqlQuery q(QSqlDatabase::database());
        q.prepare(QStringLiteral(
            "UPDATE songs SET play_count = 9, favorite = 1, "
            "date_added = '2020-01-02T03:04:05' WHERE id = ?"));
        q.addBindValue(id);
        QVERIFY(q.exec());
    }

    // Rescan-style upsert: same path, new tag metadata and file stats.
    Song updated;
    updated.path = song.path;
    updated.title = QStringLiteral("Updated");
    updated.artist = QStringLiteral("Artist B");
    updated.album = QStringLiteral("Album Y");
    updated.duration = 2000;
    updated.mtimeMs = 300;
    updated.size = 400;
    QVERIFY(m_db.upsertSong(updated));
    QCOMPARE(updated.id, id); // same row, not a duplicate

    QCOMPARE(m_db.getAllSongs().size(), 1);
    const Song stored = m_db.getSong(id);
    QCOMPARE(stored.title, QStringLiteral("Updated"));
    QCOMPARE(stored.artist, QStringLiteral("Artist B"));
    QCOMPARE(stored.album, QStringLiteral("Album Y"));
    QCOMPARE(stored.duration, qint64(2000));
    // User data untouched.
    QCOMPARE(stored.playCount, 9);
    QVERIFY(stored.favorite);
    QCOMPARE(stored.dateAdded,
             QDateTime::fromString(QStringLiteral("2020-01-02T03:04:05"), Qt::ISODate));

    // The fingerprint was refreshed too.
    const auto stats = m_db.songFileStats();
    QCOMPARE(stats.value(song.path).first, 300LL);
    QCOMPARE(stats.value(song.path).second, 400LL);
}

void TestDatabase::testRefreshAlbumArtwork()
{
    // Album X: only track 3 carries artwork, so the album must inherit
    // track 3's cached cover.
    Song t1;
    t1.path = QStringLiteral("/music/aw/t1.mp3");
    t1.title = QStringLiteral("Track 1");
    t1.artist = QStringLiteral("Artist A");
    t1.album = QStringLiteral("Album X");
    t1.trackNumber = 1;

    Song t2;
    t2.path = QStringLiteral("/music/aw/t2.mp3");
    t2.title = QStringLiteral("Track 2");
    t2.artist = t1.artist;
    t2.album = t1.album;
    t2.trackNumber = 2;

    Song t3;
    t3.path = QStringLiteral("/music/aw/t3.mp3");
    t3.title = QStringLiteral("Track 3");
    t3.artist = t1.artist;
    t3.album = t1.album;
    t3.trackNumber = 3;
    t3.artworkPath = QStringLiteral("/cache/track3.png");

    // Album Y: no artwork anywhere, so the album stays without a cover.
    Song bare;
    bare.path = QStringLiteral("/music/bare/b.mp3");
    bare.title = QStringLiteral("Bare");
    bare.artist = QStringLiteral("Artist B");
    bare.album = QStringLiteral("Album Y");

    QVERIFY(m_db.addSong(t1));
    QVERIFY(m_db.addSong(t2));
    QVERIFY(m_db.addSong(t3));
    QVERIFY(m_db.addSong(bare));

    m_db.refreshAlbumArtwork();

    const QList<Album> albums = m_db.getAllAlbums();
    QCOMPARE(albums.size(), 2);
    for (const Album &album : albums) {
        if (album.title == QStringLiteral("Album X"))
            QCOMPARE(album.artworkPath, QStringLiteral("/cache/track3.png"));
        else
            QVERIFY(album.title == QStringLiteral("Album Y")
                    && album.artworkPath.isEmpty());
    }
}

QTEST_MAIN(TestDatabase)
#include "tst_database.moc"