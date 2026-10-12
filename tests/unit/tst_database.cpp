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
    void testGetAllArtistsCountsAlbumsWithOnlyIgnoredSongs();
    void testGetAllArtistsKeepsSameAlbumTitleApart();
    void testGetAllArtistsHidesFullyIgnoredArtist();
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

    // ---- Tag editing (Option D) ----
    void testSchemaMigrationAddsEditedFlag();
    void testUpdateSongChangesTagsPreservesUserDataAndSetsEdited();
    void testUpdateSongCreatesMissingArtistAndAlbumRows();
    void testUpdateSongRejectsUnknownId();
    void testUpdateSongsUpdatesAllRowsAndKeepsUntouchedFields();
    void testUpdateSongsAtomicRollback();
    void testUpsertSongKeepsEditedTagColumns();
    void testRenameArtistFollowsSongsAndRefusesCollision();
    void testSetAlbumIdentityFollowsSongsAndRefusesCollision();
    void testSetArtworkStoresPathAndFlagsRow();
    void testSetAlbumIdentityMovesAlbumAndSongs();
    void testSetAlbumIdentityHandlesACombinedTitleAndArtistChange();
    void testSetAlbumArtworkCoversEverySongAndRollsUp();

private:
    // Raw read of the edited flag. The Song struct deliberately does not
    // carry it (it is internal to the rescan policy), so tests peek at the
    // column directly. Returns -1 when the row is missing.
    int editedFlag(qint64 songId) const;

    DatabaseManager m_db;
    QTemporaryDir m_tempDir;
};

int TestDatabase::editedFlag(qint64 songId) const
{
    QSqlQuery query(QSqlDatabase::database());
    query.prepare(QStringLiteral("SELECT edited FROM songs WHERE id = ?"));
    query.addBindValue(songId);
    if (!query.exec() || !query.next())
        return -1;
    return query.value(0).toInt();
}

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
    a1.album = QStringLiteral("Album X");
    Song a2;
    a2.path = QStringLiteral("/music/a2.mp3");
    a2.artist = QStringLiteral("Artist A");
    a2.album = QStringLiteral("Album X");
    Song b1;
    b1.path = QStringLiteral("/music/b1.mp3");
    b1.artist = QStringLiteral("Artist B");
    b1.album = QStringLiteral("Album Z");

    QVERIFY(m_db.addSong(a1));
    QVERIFY(m_db.addSong(a2));
    QVERIFY(m_db.addSong(b1));

    const QList<Artist> artists = m_db.getAllArtists();
    QCOMPARE(artists.size(), 2);
    for (const Artist &artist : artists) {
        if (artist.name == QStringLiteral("Artist A")) {
            // Two songs share one album: the (artist, song, album) fan-out
            // must not inflate either aggregate.
            QCOMPARE(artist.songCount, 2);
            QCOMPARE(artist.albumCount, 1);
        } else {
            QCOMPARE(artist.name, QStringLiteral("Artist B"));
            QCOMPARE(artist.songCount, 1);
            QCOMPARE(artist.albumCount, 1);
        }
    }
}

// The ignore feature acts on song paths; albums have no path of their own
// and are only reachable through their songs. An album whose every song is
// hidden therefore still belongs to the artist: album_count keeps counting
// it while song_count drops. Pins this behavior for the aggregate join.
void TestDatabase::testGetAllArtistsCountsAlbumsWithOnlyIgnoredSongs()
{
    Song inA;
    inA.path = QStringLiteral("/music/agg/inA.mp3");
    inA.artist = QStringLiteral("Artist A");
    inA.album = QStringLiteral("Album A");
    Song inB;
    inB.path = QStringLiteral("/music/agg/inB.mp3");
    inB.artist = QStringLiteral("Artist A");
    inB.album = QStringLiteral("Album B");

    QVERIFY(m_db.addSong(inA));
    QVERIFY(m_db.addSong(inB));

    // Hide the only song of Album A: the song disappears from songCount,
    // the album stays in albumCount.
    QVERIFY(m_db.addIgnoredPath(inA.path));

    const QList<Artist> hidden = m_db.getAllArtists();
    QCOMPARE(hidden.size(), 1);
    QCOMPARE(hidden.first().name, QStringLiteral("Artist A"));
    QCOMPARE(hidden.first().songCount, 1);
    QCOMPARE(hidden.first().albumCount, 2);

    // Un-hiding restores the song without any rescan.
    QVERIFY(m_db.removeIgnoredPath(inA.path));
    const QList<Artist> restored = m_db.getAllArtists();
    QCOMPARE(restored.size(), 1);
    QCOMPARE(restored.first().songCount, 2);
    QCOMPARE(restored.first().albumCount, 2);
}

// Albums are keyed on (title, artist_id): the same album title under two
// different artists is two distinct albums, one per artist.
void TestDatabase::testGetAllArtistsKeepsSameAlbumTitleApart()
{
    Song a1;
    a1.path = QStringLiteral("/music/hits/a1.mp3");
    a1.artist = QStringLiteral("Artist A");
    a1.album = QStringLiteral("Greatest Hits");
    Song b1;
    b1.path = QStringLiteral("/music/hits/b1.mp3");
    b1.artist = QStringLiteral("Artist B");
    b1.album = QStringLiteral("Greatest Hits");

    QVERIFY(m_db.addSong(a1));
    QVERIFY(m_db.addSong(b1));

    const QList<Artist> artists = m_db.getAllArtists();
    QCOMPARE(artists.size(), 2);
    for (const Artist &artist : artists) {
        QCOMPARE(artist.songCount, 1);
        QCOMPARE(artist.albumCount, 1);
    }
}

// An artist whose every song is hidden aggregates to song_count = 0, so
// the HAVING clause keeps the artist out of the list entirely (the rows
// stay in the database and come back once un-hidden).
void TestDatabase::testGetAllArtistsHidesFullyIgnoredArtist()
{
    Song audible;
    audible.path = QStringLiteral("/music/audible.mp3");
    audible.artist = QStringLiteral("Audible Artist");
    Song muted;
    muted.path = QStringLiteral("/music/muted.mp3");
    muted.artist = QStringLiteral("Muted Artist");

    QVERIFY(m_db.addSong(audible));
    QVERIFY(m_db.addSong(muted));

    QVERIFY(m_db.addIgnoredPath(muted.path));

    const QList<Artist> artists = m_db.getAllArtists();
    QCOMPARE(artists.size(), 1);
    QCOMPARE(artists.first().name, QStringLiteral("Audible Artist"));
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

    // And the stored schema version is 4 (the current schema, bumped by
    // the edited-flag migration), as a single row.
    QSqlQuery versionQuery(QSqlDatabase::database());
    QVERIFY(versionQuery.exec(QStringLiteral("SELECT version FROM schema_version")));
    QVERIFY(versionQuery.next());
    QCOMPARE(versionQuery.value(0).toInt(), 4);
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

// Schema v4 (tag editing): opening a v3 database adds the edited flag,
// existing rows backfill to 0 (file tags win, the pre-edit behavior) and
// the stored version becomes 4. Reopening is idempotent.
void TestDatabase::testSchemaMigrationAddsEditedFlag()
{
    // Build a v3 database by hand: songs without edited, schema_version 3.
    const QString v3Path = m_tempDir.filePath(QStringLiteral("v3.db"));
    QFile::remove(v3Path);
    {
        const QString connName = QStringLiteral("v3_setup");
        {
            QSqlDatabase setup = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connName);
            setup.setDatabaseName(v3Path);
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
                    "INSERT INTO songs (path, title) "
                    "VALUES ('/music/legacy.mp3', 'Legacy')")));
                QVERIFY(q.exec(QStringLiteral("CREATE TABLE schema_version (version INTEGER)")));
                QVERIFY(q.exec(QStringLiteral("INSERT INTO schema_version (version) VALUES (3)")));
            }
            setup.close();
        }
        QSqlDatabase::removeDatabase(connName);
    }

    // Reopen through DatabaseManager: createTables() migrates in place.
    m_db.close();
    DatabaseManager migrated;
    QVERIFY(migrated.open(v3Path));

    bool hasEdited = false;
    {
        QSqlQuery info(QSqlDatabase::database());
        QVERIFY(info.exec(QStringLiteral("PRAGMA table_info(songs)")));
        while (info.next()) {
            hasEdited = hasEdited
                || info.value(QStringLiteral("name")).toString() == QStringLiteral("edited");
        }
    }
    QVERIFY(hasEdited);

    // The legacy row is intact and backfilled to 0 (the file's tags win).
    {
        QSqlQuery q(QSqlDatabase::database());
        QVERIFY(q.exec(QStringLiteral("SELECT title, edited FROM songs")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toString(), QStringLiteral("Legacy"));
        QCOMPARE(q.value(1).toInt(), 0);
    }

    // The stored schema version is 4, as a single row.
    {
        QSqlQuery versionQuery(QSqlDatabase::database());
        QVERIFY(versionQuery.exec(QStringLiteral("SELECT version FROM schema_version")));
        QVERIFY(versionQuery.next());
        QCOMPARE(versionQuery.value(0).toInt(), 4);
        QVERIFY(!versionQuery.next());
    }

    // Reopening (the migration is presence-checked) changes nothing.
    migrated.close();
    QVERIFY(migrated.open(v3Path));
    {
        QSqlQuery q(QSqlDatabase::database());
        QVERIFY(q.exec(QStringLiteral("SELECT edited FROM songs")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }
}

// updateSong() changes the tag columns, flags the row edited and never
// touches user data, track/disc numbers or the file fingerprint.
void TestDatabase::testUpdateSongChangesTagsPreservesUserDataAndSetsEdited()
{
    Song song;
    song.path = QStringLiteral("/music/edit.mp3");
    song.title = QStringLiteral("Original Title");
    song.artist = QStringLiteral("Artist A");
    song.album = QStringLiteral("Album X");
    song.genre = QStringLiteral("Rock");
    song.year = 2001;
    song.trackNumber = 3;
    song.duration = 100000;
    song.mtimeMs = 111;
    song.size = 222;
    QVERIFY(m_db.upsertSong(song));

    // User data plus a track number the edit must never touch.
    {
        QSqlQuery q(QSqlDatabase::database());
        QVERIFY(q.exec(QStringLiteral(
            "UPDATE songs SET play_count = 9, favorite = 1, "
            "last_played = '2021-06-07T08:09:10', "
            "date_added = '2020-01-02T03:04:05' WHERE id = ") +
            QString::number(song.id)));
    }

    Song corrected = m_db.getSong(song.id);
    corrected.title = QStringLiteral("Fixed Title");
    corrected.artist = QStringLiteral("Correct Artist");
    corrected.album = QStringLiteral("Correct Album");
    corrected.genre = QStringLiteral("Jazz");
    corrected.year = 1999;
    corrected.duration = 123456;
    corrected.artworkPath = QStringLiteral("/cache/chosen.png");
    QVERIFY(m_db.updateSong(corrected));

    const Song stored = m_db.getSong(song.id);
    QCOMPARE(stored.title, QStringLiteral("Fixed Title"));
    QCOMPARE(stored.artist, QStringLiteral("Correct Artist"));
    QCOMPARE(stored.album, QStringLiteral("Correct Album"));
    QCOMPARE(stored.genre, QStringLiteral("Jazz"));
    QCOMPARE(stored.year, 1999);
    QCOMPARE(stored.duration, qint64(123456));
    QCOMPARE(stored.artworkPath, QStringLiteral("/cache/chosen.png"));

    // User data untouched.
    QCOMPARE(stored.playCount, 9);
    QVERIFY(stored.favorite);
    QCOMPARE(stored.lastPlayed,
             QDateTime::fromString(QStringLiteral("2021-06-07T08:09:10"), Qt::ISODate));
    QCOMPARE(stored.dateAdded,
             QDateTime::fromString(QStringLiteral("2020-01-02T03:04:05"), Qt::ISODate));

    // Track number survives (not user-editable)...
    QCOMPARE(stored.trackNumber, 3);
    // ...the fingerprint survives too (the file did not change, so the
    // next scan keeps skipping it)...
    const auto stats = m_db.songFileStats();
    QCOMPARE(stats.value(song.path).first, 111LL);
    QCOMPARE(stats.value(song.path).second, 222LL);
    // ...and the row is flagged as curated.
    QCOMPARE(editedFlag(song.id), 1);
}

// Moving a song to brand-new artist/album names creates the entity rows;
// the rows the move orphaned are cleaned up in the same transaction.
void TestDatabase::testUpdateSongCreatesMissingArtistAndAlbumRows()
{
    Song song;
    song.path = QStringLiteral("/music/move.mp3");
    song.title = QStringLiteral("Mover");
    song.artist = QStringLiteral("Old Artist");
    song.album = QStringLiteral("Old Album");
    song.genre = QStringLiteral("Old Genre");
    QVERIFY(m_db.addSong(song));

    Song corrected = m_db.getSong(song.id);
    corrected.artist = QStringLiteral("Brand New Artist");
    corrected.album = QStringLiteral("Brand New Album");
    corrected.genre = QStringLiteral("Brand New Genre");
    QVERIFY(m_db.updateSong(corrected));

    const Song stored = m_db.getSong(song.id);
    QCOMPARE(stored.artist, QStringLiteral("Brand New Artist"));
    QCOMPARE(stored.album, QStringLiteral("Brand New Album"));
    QCOMPARE(stored.genre, QStringLiteral("Brand New Genre"));

    // Exactly one row per entity: the new names exist, the orphaned old
    // ones were really deleted (not just hidden by the HAVING clause).
    auto entityCount = [](const char *table) {
        QSqlQuery q(QSqlDatabase::database());
        q.exec(QStringLiteral("SELECT COUNT(*) FROM %1").arg(QString::fromLatin1(table)));
        return q.next() ? q.value(0).toInt() : -1;
    };
    QCOMPARE(entityCount("artists"), 1);
    QCOMPARE(entityCount("albums"), 1);
    QCOMPARE(entityCount("genres"), 1);
}

// An unknown id is refused and leaves no trace: the name resolutions the
// failed update performed are rolled back with the transaction.
void TestDatabase::testUpdateSongRejectsUnknownId()
{
    Song song;
    song.path = QStringLiteral("/music/kept.mp3");
    song.title = QStringLiteral("Kept");
    song.artist = QStringLiteral("Kept Artist");
    song.album = QStringLiteral("Kept Album");
    QVERIFY(m_db.addSong(song));

    Song ghost;
    ghost.id = 999999; // no such row
    ghost.title = QStringLiteral("Ghost Title");
    ghost.artist = QStringLiteral("Ghost Artist");
    ghost.album = QStringLiteral("Ghost Album");
    ghost.genre = QStringLiteral("Ghost Genre");
    QVERIFY(!m_db.updateSong(ghost));

    // Nothing changed: the song is untouched and the ghost's would-be
    // entity rows are gone (the rollback worked).
    QCOMPARE(m_db.getAllSongs().size(), 1);
    QCOMPARE(m_db.getSong(song.id).title, QStringLiteral("Kept"));
    QCOMPARE(m_db.getSong(song.id).artist, QStringLiteral("Kept Artist"));
    {
        QSqlQuery q(QSqlDatabase::database());
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM artists")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM albums")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 1);
        QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM genres")));
        QVERIFY(q.next());
        QCOMPARE(q.value(0).toInt(), 0);
    }
}

// A batch that reassigns only the album must leave every other field of
// every row exactly as stored (the "no equalizing" rule) and flag all
// rows edited.
void TestDatabase::testUpdateSongsUpdatesAllRowsAndKeepsUntouchedFields()
{
    QList<qint64> ids;
    for (int i = 1; i <= 3; ++i) {
        Song song;
        song.path = QStringLiteral("/music/batch%1.mp3").arg(i);
        song.title = QStringLiteral("Batch Title %1").arg(i);
        song.artist = QStringLiteral("Batch Artist");
        song.album = QStringLiteral("Batch Album %1").arg(i);
        song.trackNumber = i;
        QVERIFY(m_db.addSong(song));
        ids.append(song.id);
    }

    // Field-level reassignment: only the album changes, per song.
    QList<Song> batch;
    for (const qint64 id : ids) {
        Song song = m_db.getSong(id);
        song.album = QStringLiteral("Reassigned Album");
        batch.append(song);
    }
    QVERIFY(m_db.updateSongs(batch));

    for (int i = 0; i < ids.size(); ++i) {
        const Song stored = m_db.getSong(ids.at(i));
        QCOMPARE(stored.album, QStringLiteral("Reassigned Album"));
        QCOMPARE(stored.title, QStringLiteral("Batch Title %1").arg(i + 1));
        QCOMPARE(stored.artist, QStringLiteral("Batch Artist"));
        QCOMPARE(stored.trackNumber, i + 1);
        QCOMPARE(editedFlag(ids.at(i)), 1);
    }
    QCOMPARE(m_db.getAllAlbums().size(), 1);
}

// One bad row rolls the whole batch back: nothing is applied at all.
void TestDatabase::testUpdateSongsAtomicRollback()
{
    QList<qint64> ids;
    for (int i = 1; i <= 3; ++i) {
        Song song;
        song.path = QStringLiteral("/music/atomic%1.mp3").arg(i);
        song.title = QStringLiteral("Atomic Title %1").arg(i);
        song.artist = QStringLiteral("Atomic Artist");
        song.album = QStringLiteral("Atomic Album %1").arg(i);
        QVERIFY(m_db.addSong(song));
        ids.append(song.id);
    }

    QList<Song> batch;
    for (const qint64 id : ids) {
        Song song = m_db.getSong(id);
        song.album = QStringLiteral("Atomic Batch Album");
        batch.append(song);
    }
    Song ghost;
    ghost.id = 999999;
    ghost.title = QStringLiteral("Ghost");
    ghost.album = QStringLiteral("Atomic Batch Album");
    batch.append(ghost);

    QVERIFY(!m_db.updateSongs(batch));

    // No row changed: titles, albums and the edited flag are all as they
    // were before the failed batch.
    for (int i = 0; i < ids.size(); ++i) {
        const Song stored = m_db.getSong(ids.at(i));
        QCOMPARE(stored.title, QStringLiteral("Atomic Title %1").arg(i + 1));
        QCOMPARE(stored.album, QStringLiteral("Atomic Album %1").arg(i + 1));
        QCOMPARE(editedFlag(ids.at(i)), 0);
    }

    // An empty batch is a refused no-op.
    QVERIFY(!m_db.updateSongs({}));
}

// THE rescan policy pin: an edited row keeps its user-curated tag columns
// through an upsert with different file tags, while mtime/size still
// refresh (no permanent re-read loop) and an unedited control row in the
// same database takes the file's new tags.
void TestDatabase::testUpsertSongKeepsEditedTagColumns()
{
    Song edited;
    edited.path = QStringLiteral("/music/edited.mp3");
    edited.title = QStringLiteral("File Title");
    edited.artist = QStringLiteral("File Artist");
    edited.album = QStringLiteral("File Album");
    edited.genre = QStringLiteral("Rock");
    edited.year = 2001;
    edited.duration = 1000;
    edited.mtimeMs = 100;
    edited.size = 200;
    QVERIFY(m_db.upsertSong(edited));

    Song control;
    control.path = QStringLiteral("/music/control.mp3");
    control.title = QStringLiteral("Control File Title");
    control.artist = QStringLiteral("Control Artist");
    control.album = QStringLiteral("Control File Album");
    control.mtimeMs = 100;
    control.size = 200;
    QVERIFY(m_db.upsertSong(control));

    // The user corrects the first song (this flags the row edited = 1).
    Song user = m_db.getSong(edited.id);
    user.title = QStringLiteral("Edited Title");
    user.artist = QStringLiteral("Edited Artist");
    user.album = QStringLiteral("Edited Album");
    QVERIFY(m_db.updateSong(user));

    // Play stats accumulated between the edit and the rescan must survive.
    {
        QSqlQuery q(QSqlDatabase::database());
        QVERIFY(q.exec(QStringLiteral("UPDATE songs SET play_count = 5 WHERE id = ") +
                        QString::number(edited.id)));
    }

    // "Rescan": both files show up with different tags and fresh
    // fingerprints.
    Song rescanEdited;
    rescanEdited.path = edited.path;
    rescanEdited.title = QStringLiteral("Changed File Title");
    rescanEdited.artist = QStringLiteral("Changed File Artist");
    rescanEdited.album = QStringLiteral("Changed File Album");
    rescanEdited.genre = QStringLiteral("Pop");
    rescanEdited.year = 2002;
    rescanEdited.duration = 2000;
    rescanEdited.mtimeMs = 300;
    rescanEdited.size = 400;
    QVERIFY(m_db.upsertSong(rescanEdited));

    Song rescanControl;
    rescanControl.path = control.path;
    rescanControl.title = QStringLiteral("New Control Title");
    rescanControl.artist = QStringLiteral("New Control Artist");
    rescanControl.album = QStringLiteral("New Control Album");
    rescanControl.mtimeMs = 300;
    rescanControl.size = 400;
    QVERIFY(m_db.upsertSong(rescanControl));

    // The edited row kept every user-curated tag column...
    const Song storedEdited = m_db.getSong(edited.id);
    QCOMPARE(storedEdited.title, QStringLiteral("Edited Title"));
    QCOMPARE(storedEdited.artist, QStringLiteral("Edited Artist"));
    QCOMPARE(storedEdited.album, QStringLiteral("Edited Album"));
    QCOMPARE(storedEdited.genre, QStringLiteral("Rock"));
    QCOMPARE(storedEdited.year, 2001);
    QCOMPARE(storedEdited.duration, qint64(1000));
    // ...the user data survived as always...
    QCOMPARE(storedEdited.playCount, 5);
    // ...but the fingerprint DID refresh, so the next scan skips the file
    // instead of re-reading it forever.
    const auto stats = m_db.songFileStats();
    QCOMPARE(stats.value(edited.path).first, 300LL);
    QCOMPARE(stats.value(edited.path).second, 400LL);

    // The unedited control row took its new file tags wholesale.
    const Song storedControl = m_db.getSong(control.id);
    QCOMPARE(storedControl.title, QStringLiteral("New Control Title"));
    QCOMPARE(storedControl.artist, QStringLiteral("New Control Artist"));
    QCOMPARE(storedControl.album, QStringLiteral("New Control Album"));
    QCOMPARE(stats.value(control.path).first, 300LL);
    QCOMPARE(stats.value(control.path).second, 400LL);
}

void TestDatabase::testRenameArtistFollowsSongsAndRefusesCollision()
{
    Song a;
    a.path = QStringLiteral("/music/renamed_a.mp3");
    a.title = QStringLiteral("Song A");
    a.artist = QStringLiteral("Rename Artist A");
    a.album = QStringLiteral("Rename Album A");
    Song b;
    b.path = QStringLiteral("/music/renamed_b.mp3");
    b.title = QStringLiteral("Song B");
    b.artist = QStringLiteral("Rename Artist B");
    b.album = QStringLiteral("Rename Album B");
    QVERIFY(m_db.addSong(a));
    QVERIFY(m_db.addSong(b));

    // Refused: the target name already exists (a merge, not a rename).
    QVERIFY(!m_db.renameArtist(QStringLiteral("Rename Artist A"),
                                QStringLiteral("Rename Artist B")));
    QCOMPARE(m_db.getSongsByArtist(QStringLiteral("Rename Artist A")).size(), 1);

    // Empty or identical names are refused outright.
    QVERIFY(!m_db.renameArtist(QString(), QStringLiteral("X")));
    QVERIFY(!m_db.renameArtist(QStringLiteral("X"), QString()));
    QVERIFY(!m_db.renameArtist(QStringLiteral("Same Name"), QStringLiteral("Same Name")));

    // Success: songs follow through artist_id and the old name is gone.
    QVERIFY(m_db.renameArtist(QStringLiteral("Rename Artist A"),
                               QStringLiteral("Rename Artist C")));
    const QList<Song> followed = m_db.getSongsByArtist(QStringLiteral("Rename Artist C"));
    QCOMPARE(followed.size(), 1);
    QCOMPARE(followed.first().title, QStringLiteral("Song A"));
    QCOMPARE(m_db.getSongsByArtist(QStringLiteral("Rename Artist A")).size(), 0);

    const QList<Artist> artists = m_db.getAllArtists();
    QCOMPARE(artists.size(), 2);
    for (const Artist &artist : artists) {
        QVERIFY(artist.name != QStringLiteral("Rename Artist A"));
    }

    // An unknown source name is refused too.
    QVERIFY(!m_db.renameArtist(QStringLiteral("No Such Artist"),
                               QStringLiteral("Whatever")));
}

void TestDatabase::testSetAlbumIdentityFollowsSongsAndRefusesCollision()
{
    Song x;
    x.path = QStringLiteral("/music/album_x.mp3");
    x.title = QStringLiteral("Album Song X");
    x.artist = QStringLiteral("Album Artist");
    x.album = QStringLiteral("Collision Album");
    Song y;
    y.path = QStringLiteral("/music/album_y.mp3");
    y.title = QStringLiteral("Album Song Y");
    y.artist = QStringLiteral("Album Artist");
    y.album = QStringLiteral("Target Album");
    QVERIFY(m_db.addSong(x));
    QVERIFY(m_db.addSong(y));

    // Seed the album row with artwork that must survive the rename.
    {
        QSqlQuery q(QSqlDatabase::database());
        QVERIFY(q.exec(QStringLiteral(
            "UPDATE albums SET artwork_path = '/cache/album_cover.png' "
            "WHERE title = 'Collision Album'")));
    }

    // Refused: the artist already has an album titled "Target Album"
    // (no database constraint enforces this — the refusal is code only).
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("Collision Album"),
                                   QStringLiteral("Album Artist"),
                                   QStringLiteral("Target Album"),
                                   QStringLiteral("Album Artist")));
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Collision Album"),
                                   QStringLiteral("Album Artist")).size(), 1);

    // Identical names are refused outright.
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("Collision Album"),
                                   QStringLiteral("Album Artist"),
                                   QStringLiteral("Collision Album"),
                                   QStringLiteral("Album Artist")));

    // Success: songs follow through album_id, the old title is gone and
    // the artwork column survives on the renamed row.
    QVERIFY(m_db.setAlbumIdentity(QStringLiteral("Collision Album"),
                                  QStringLiteral("Album Artist"),
                                  QStringLiteral("Renamed Album"),
                                  QStringLiteral("Album Artist")));
    const QList<Song> followed = m_db.getSongsByAlbum(QStringLiteral("Renamed Album"),
                                                      QStringLiteral("Album Artist"));
    QCOMPARE(followed.size(), 1);
    QCOMPARE(followed.first().title, QStringLiteral("Album Song X"));
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Collision Album")).size(), 0);

    const QList<Album> albums = m_db.getAllAlbums();
    QCOMPARE(albums.size(), 2);
    for (const Album &album : albums) {
        if (album.title == QStringLiteral("Renamed Album")) {
            QCOMPARE(album.artist, QStringLiteral("Album Artist"));
            QCOMPARE(album.artworkPath, QStringLiteral("/cache/album_cover.png"));
        } else {
            QCOMPARE(album.title, QStringLiteral("Target Album"));
        }
    }

    // An unknown album is refused.
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("No Such Album"),
                                   QStringLiteral("Album Artist"),
                                   QStringLiteral("Whatever"),
                                   QStringLiteral("Album Artist")));
}

void TestDatabase::testSetArtworkStoresPathAndFlagsRow()
{
    Song song;
    song.path = QStringLiteral("/music/artwork_song.mp3");
    song.title = QStringLiteral("Coverless");
    song.artist = QStringLiteral("Artwork Artist");
    song.album = QStringLiteral("Artwork Album");
    QVERIFY(m_db.addSong(song));

    {
        QSqlQuery q(QSqlDatabase::database());
        QVERIFY(q.exec(QStringLiteral("UPDATE songs SET play_count = 4, favorite = 1 "
                                      "WHERE id = ") + QString::number(song.id)));
    }

    const QString picked = QStringLiteral("/cache/picked_cover.png");
    QVERIFY(m_db.setArtwork(song.id, picked));

    const Song stored = m_db.getSong(song.id);
    QCOMPARE(stored.artworkPath, picked);
    QCOMPARE(editedFlag(song.id), 1);
    // User data preserved.
    QCOMPARE(stored.playCount, 4);
    QVERIFY(stored.favorite);

    // The album rollup picked the new cover up, like at scan time.
    const QList<Album> albums = m_db.getAllAlbums();
    QCOMPARE(albums.size(), 1);
    QCOMPARE(albums.first().artworkPath, picked);

    // Unknown id: refused.
    QVERIFY(!m_db.setArtwork(999999, QStringLiteral("/cache/none.png")));
}

void TestDatabase::testSetAlbumIdentityMovesAlbumAndSongs()
{
    Song a;
    a.path = QStringLiteral("/music/move_a.mp3");
    a.title = QStringLiteral("Move Song A");
    a.artist = QStringLiteral("Old Owner");
    a.album = QStringLiteral("Shared Title");
    Song b;
    b.path = QStringLiteral("/music/move_b.mp3");
    b.title = QStringLiteral("Move Song B");
    b.artist = QStringLiteral("Old Owner");
    b.album = QStringLiteral("Shared Title");
    // A sibling album of the old artist: the move must leave it alone.
    Song keeper;
    keeper.path = QStringLiteral("/music/move_keep.mp3");
    keeper.title = QStringLiteral("Keeper Song");
    keeper.artist = QStringLiteral("Old Owner");
    keeper.album = QStringLiteral("Keeper Album");
    QVERIFY(m_db.addSong(a));
    QVERIFY(m_db.addSong(b));
    QVERIFY(m_db.addSong(keeper));

    // Refused: unchanged identity, empty target artist, empty title,
    // unknown album.
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("Shared Title"),
                                   QStringLiteral("Old Owner"),
                                   QStringLiteral("Shared Title"),
                                   QStringLiteral("Old Owner")));
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("Shared Title"),
                                   QStringLiteral("Old Owner"),
                                   QStringLiteral("Shared Title"), QString()));
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("Shared Title"),
                                   QStringLiteral("Old Owner"), QString(),
                                   QStringLiteral("New Owner")));
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("No Such Album"),
                                   QStringLiteral("Old Owner"),
                                   QStringLiteral("No Such Album"),
                                   QStringLiteral("New Owner")));
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Shared Title"),
                                  QStringLiteral("Old Owner")).size(), 2);

    QVERIFY(m_db.setAlbumIdentity(QStringLiteral("Shared Title"),
                                  QStringLiteral("Old Owner"),
                                  QStringLiteral("Shared Title"),
                                  QStringLiteral("New Owner")));

    // The album row and its songs move together: the drill-down resolves a
    // song's artist from songs.artist_id while the album card reads
    // albums.artist_id, so leaving the two apart would show a card with no
    // songs under it.
    const QList<Song> moved = m_db.getSongsByAlbum(QStringLiteral("Shared Title"),
                                                   QStringLiteral("New Owner"));
    QCOMPARE(moved.size(), 2);
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Shared Title"),
                                  QStringLiteral("Old Owner")).size(), 0);
    for (const Song &song : moved) {
        QCOMPARE(song.artist, QStringLiteral("New Owner"));
        QCOMPARE(editedFlag(song.id), 1); // the move survives a rescan
    }
    for (const Album &album : m_db.getAllAlbums()) {
        if (album.title == QStringLiteral("Shared Title"))
            QCOMPARE(album.artist, QStringLiteral("New Owner"));
    }
    // The sibling album of the old artist is untouched.
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Keeper Album"),
                                  QStringLiteral("Old Owner")).size(), 1);

    // Collision: the target artist already has an album with this title.
    // No database constraint enforces this — the refusal is code only.
    Song twin;
    twin.path = QStringLiteral("/music/move_twin.mp3");
    twin.title = QStringLiteral("Twin Song");
    twin.artist = QStringLiteral("Other Owner");
    twin.album = QStringLiteral("Shared Title");
    QVERIFY(m_db.addSong(twin));
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("Shared Title"),
                                   QStringLiteral("New Owner"),
                                   QStringLiteral("Shared Title"),
                                   QStringLiteral("Other Owner")));
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Shared Title"),
                                  QStringLiteral("New Owner")).size(), 2);

    // Moving the old artist's last album away releases the orphan row.
    QVERIFY(m_db.setAlbumIdentity(QStringLiteral("Keeper Album"),
                                  QStringLiteral("Old Owner"),
                                  QStringLiteral("Keeper Album"),
                                  QStringLiteral("New Owner")));
    for (const Artist &artist : m_db.getAllArtists()) {
        QVERIFY(artist.name != QStringLiteral("Old Owner"));
    }
}

void TestDatabase::testSetAlbumIdentityHandlesACombinedTitleAndArtistChange()
{
    // The trap this pins: the album being edited is "Best" of artist A, and
    // the edit renames it to "Hits" while moving it to artist B. Both
    // intermediate states are taken:
    //   - title first  -> (Hits, A) collides with A's own "Hits";
    //   - artist first -> (Best, B) collides with B's own "Best".
    // The FINAL identity (Hits, B) is free, so the edit must be accepted.
    // This is why the operation is one transaction with one collision check
    // on the final identity instead of two sequential calls.
    Song moved;
    moved.path = QStringLiteral("/music/combined_best.mp3");
    moved.title = QStringLiteral("Combined Song");
    moved.artist = QStringLiteral("Combined A");
    moved.album = QStringLiteral("Best");
    Song aOwn;
    aOwn.path = QStringLiteral("/music/combined_a_hits.mp3");
    aOwn.title = QStringLiteral("A Own Hit");
    aOwn.artist = QStringLiteral("Combined A");
    aOwn.album = QStringLiteral("Hits");
    Song bOwn;
    bOwn.path = QStringLiteral("/music/combined_b_best.mp3");
    bOwn.title = QStringLiteral("B Own Best");
    bOwn.artist = QStringLiteral("Combined B");
    bOwn.album = QStringLiteral("Best");
    QVERIFY(m_db.addSong(moved));
    QVERIFY(m_db.addSong(aOwn));
    QVERIFY(m_db.addSong(bOwn));

    QVERIFY(m_db.setAlbumIdentity(QStringLiteral("Best"),
                                  QStringLiteral("Combined A"),
                                  QStringLiteral("Hits"),
                                  QStringLiteral("Combined B")));

    // The edited album landed on the final identity, with its song.
    const QList<Song> landed = m_db.getSongsByAlbum(QStringLiteral("Hits"),
                                                    QStringLiteral("Combined B"));
    QCOMPARE(landed.size(), 1);
    QCOMPARE(landed.first().id, moved.id);
    QCOMPARE(editedFlag(moved.id), 1);
    // Both neighbours survived untouched: A keeps its own "Hits", B keeps
    // its own "Best" (the method must not have merged with either).
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Hits"),
                                  QStringLiteral("Combined A")).size(), 1);
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Best"),
                                  QStringLiteral("Combined B")).size(), 1);
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Best"),
                                  QStringLiteral("Combined A")).size(), 0);
    // Every album row still carries its own artist.
    for (const Album &album : m_db.getAllAlbums()) {
        if (album.title == QStringLiteral("Hits")
                && album.artist == QStringLiteral("Combined B")) {
            QCOMPARE(album.songCount, 1);
        }
    }

    // The final identity is still refused when it is genuinely taken: the
    // same rename onto an artist that already owns a "Hits".
    Song bHits;
    bHits.path = QStringLiteral("/music/combined_b_hits.mp3");
    bHits.title = QStringLiteral("B Own Hit");
    bHits.artist = QStringLiteral("Combined B");
    bHits.album = QStringLiteral("Hits");
    QVERIFY(m_db.addSong(bHits));
    QVERIFY(!m_db.setAlbumIdentity(QStringLiteral("Hits"),
                                   QStringLiteral("Combined A"),
                                   QStringLiteral("Hits"),
                                   QStringLiteral("Combined B")));
    QCOMPARE(m_db.getSongsByAlbum(QStringLiteral("Hits"),
                                  QStringLiteral("Combined A")).size(), 1);
}

void TestDatabase::testSetAlbumArtworkCoversEverySongAndRollsUp()
{
    Song a;
    a.path = QStringLiteral("/music/cover_a.mp3");
    a.title = QStringLiteral("Cover Song A");
    a.artist = QStringLiteral("Cover Artist");
    a.album = QStringLiteral("Cover Album");
    Song b;
    b.path = QStringLiteral("/music/cover_b.mp3");
    b.title = QStringLiteral("Cover Song B");
    b.artist = QStringLiteral("Cover Artist");
    b.album = QStringLiteral("Cover Album");
    // A sibling album: the cover edit must not leak into it.
    Song other;
    other.path = QStringLiteral("/music/cover_other.mp3");
    other.title = QStringLiteral("Other Song");
    other.artist = QStringLiteral("Cover Artist");
    other.album = QStringLiteral("Other Album");
    QVERIFY(m_db.addSong(a));
    QVERIFY(m_db.addSong(b));
    QVERIFY(m_db.addSong(other));

    const QString picked = QStringLiteral("/cache/album_cover.png");
    QVERIFY(m_db.setAlbumArtwork(QStringLiteral("Cover Album"),
                                 QStringLiteral("Cover Artist"), picked));

    // Every song of the album points at the cover and is flagged edited, so
    // the choice survives a rescan exactly like setArtwork().
    QCOMPARE(m_db.getSong(a.id).artworkPath, picked);
    QCOMPARE(m_db.getSong(b.id).artworkPath, picked);
    QCOMPARE(editedFlag(a.id), 1);
    QCOMPARE(editedFlag(b.id), 1);
    // The sibling album is untouched.
    QCOMPARE(m_db.getSong(other.id).artworkPath, QString());
    QCOMPARE(editedFlag(other.id), 0);

    // The rollup puts the cover on the album card.
    for (const Album &album : m_db.getAllAlbums()) {
        if (album.title == QStringLiteral("Cover Album"))
            QCOMPARE(album.artworkPath, picked);
        else
            QCOMPARE(album.artworkPath, QString());
    }

    // An empty path is the explicit "remove the cover" intent.
    QVERIFY(m_db.setAlbumArtwork(QStringLiteral("Cover Album"),
                                 QStringLiteral("Cover Artist"), QString()));
    QCOMPARE(m_db.getSong(a.id).artworkPath, QString());
    QCOMPARE(m_db.getSong(b.id).artworkPath, QString());

    // Unknown album: refused.
    QVERIFY(!m_db.setAlbumArtwork(QStringLiteral("No Such Album"),
                                  QStringLiteral("Cover Artist"),
                                  QStringLiteral("/cache/none.png")));
}

QTEST_MAIN(TestDatabase)
#include "tst_database.moc"