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
    void testUpdateSong();
    void testRemoveSong();
    void testRemoveSongCleansPlaylistReferences();
    void testRemoveSongCleansOrphanEntities();
    void testSearchSongs();
    void testGetAllArtistsAggregates();
    void testGetAllAlbumsAggregates();
    void testGetAllGenresAggregates();
    void testPlaylistLifecycle();
    void testPlaylistSongs();
    void testUpdatePlayCount();
    void testSetFavorite();
    void testIgnoredPathsLifecycle();
    void testIgnoredFileHiddenFromQueries();
    void testIgnoredDirectoryHidesChildren();

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

    QVERIFY(m_db.schemaVersion() >= 1);
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

void TestDatabase::testUpdateSong()
{
    Song song;
    song.path = QStringLiteral("/music/upd.mp3");
    song.title = QStringLiteral("Before");
    QVERIFY(m_db.addSong(song));

    song.title = QStringLiteral("After");
    song.favorite = true;
    QVERIFY(m_db.updateSong(song));

    const Song stored = m_db.getSong(song.id);
    QCOMPARE(stored.title, QStringLiteral("After"));
    QVERIFY(stored.favorite);
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
    QCOMPARE(m_db.getAllGenres().size(), 1);

    QVERIFY(m_db.removeSong(song.id));

    // Artists/albums/genres without songs are cleaned up.
    QCOMPARE(m_db.getAllArtists().size(), 0);
    QCOMPARE(m_db.getAllAlbums().size(), 0);
    QCOMPARE(m_db.getAllGenres().size(), 0);
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

void TestDatabase::testGetAllGenresAggregates()
{
    Song rock1;
    rock1.path = QStringLiteral("/music/rock1.mp3");
    rock1.genre = QStringLiteral("Rock");
    Song rock2;
    rock2.path = QStringLiteral("/music/rock2.mp3");
    rock2.genre = QStringLiteral("Rock");
    Song pop1;
    pop1.path = QStringLiteral("/music/pop1.mp3");
    pop1.genre = QStringLiteral("Pop");

    QVERIFY(m_db.addSong(rock1));
    QVERIFY(m_db.addSong(rock2));
    QVERIFY(m_db.addSong(pop1));

    const QList<Genre> genres = m_db.getAllGenres();
    QCOMPARE(genres.size(), 2);
    for (const Genre &genre : genres) {
        if (genre.name == QStringLiteral("Rock")) {
            QCOMPARE(genre.songCount, 2);
        } else {
            QCOMPARE(genre.name, QStringLiteral("Pop"));
            QCOMPARE(genre.songCount, 1);
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

void TestDatabase::testUpdatePlayCount()
{
    Song song;
    song.path = QStringLiteral("/music/count.mp3");
    QVERIFY(m_db.addSong(song));

    QVERIFY(m_db.updatePlayCount(song.id));
    QVERIFY(m_db.updatePlayCount(song.id));

    const Song stored = m_db.getSong(song.id);
    QCOMPARE(stored.playCount, 2);
    QVERIFY(stored.lastPlayed.isValid());
}

void TestDatabase::testSetFavorite()
{
    Song song;
    song.path = QStringLiteral("/music/fav.mp3");
    QVERIFY(m_db.addSong(song));

    QVERIFY(m_db.setFavorite(song.id, true));
    QVERIFY(m_db.getSong(song.id).favorite);

    QVERIFY(m_db.setFavorite(song.id, false));
    QVERIFY(!m_db.getSong(song.id).favorite);
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
    QCOMPARE(m_db.getAllGenres().first().songCount, 1);

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

QTEST_MAIN(TestDatabase)
#include "tst_database.moc"