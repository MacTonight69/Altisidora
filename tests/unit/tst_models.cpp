#include <QtTest>
#include <QDateTime>

#include "models/Song.h"
#include "models/Album.h"
#include "models/Artist.h"
#include "models/Genre.h"
#include "models/Playlist.h"

/**
 * @brief Tests for the real domain models (src/models).
 *
 * Validates default values, round-trip conversion (toVariantMap /
 * fromVariantMap) and Playlist invariants.
 */
class TestModels : public QObject
{
    Q_OBJECT

private slots:
    void testSongDefaultValues();
    void testSongVariantRoundTrip();
    void testSongFromVariantMapPartial();

    void testAlbumDefaultValues();
    void testAlbumVariantRoundTrip();

    void testArtistDefaultValues();
    void testArtistVariantRoundTrip();

    void testGenreDefaultValues();
    void testGenreVariantRoundTrip();

    void testPlaylistDefaults();
    void testPlaylistConstructor();
    void testPlaylistIsValid();
};

void TestModels::testSongDefaultValues()
{
    Song song;
    QCOMPARE(song.id, 0);
    QVERIFY(song.path.isEmpty());
    QVERIFY(song.title.isEmpty());
    QVERIFY(song.artist.isEmpty());
    QVERIFY(song.album.isEmpty());
    QVERIFY(song.genre.isEmpty());
    QCOMPARE(song.year, 0);
    QCOMPARE(song.trackNumber, 0);
    QCOMPARE(song.discNumber, 0);
    QCOMPARE(song.duration, qint64(0));
    QVERIFY(song.artworkPath.isEmpty());
    QVERIFY(!song.dateAdded.isValid());
    QVERIFY(!song.lastPlayed.isValid());
    QCOMPARE(song.playCount, 0);
    QVERIFY(!song.favorite);
}

void TestModels::testSongVariantRoundTrip()
{
    Song original;
    original.id = 42;
    original.path = QStringLiteral("/music/song.flac");
    original.title = QStringLiteral("My Song");
    original.artist = QStringLiteral("My Artist");
    original.album = QStringLiteral("My Album");
    original.genre = QStringLiteral("Rock");
    original.year = 2024;
    original.trackNumber = 3;
    original.discNumber = 1;
    original.duration = 240000;
    original.artworkPath = QStringLiteral("/artwork/cover.jpg");
    original.dateAdded = QDateTime(QDate(2024, 1, 15), QTime(10, 30));
    original.lastPlayed = QDateTime(QDate(2024, 1, 20), QTime(21, 0));
    original.playCount = 15;
    original.favorite = true;

    const QVariantMap map = original.toVariantMap();
    QCOMPARE(map.size(), 15);

    const Song restored = Song::fromVariantMap(map);
    QCOMPARE(restored.id, original.id);
    QCOMPARE(restored.path, original.path);
    QCOMPARE(restored.title, original.title);
    QCOMPARE(restored.artist, original.artist);
    QCOMPARE(restored.album, original.album);
    QCOMPARE(restored.genre, original.genre);
    QCOMPARE(restored.year, original.year);
    QCOMPARE(restored.trackNumber, original.trackNumber);
    QCOMPARE(restored.discNumber, original.discNumber);
    QCOMPARE(restored.duration, original.duration);
    QCOMPARE(restored.artworkPath, original.artworkPath);
    QCOMPARE(restored.dateAdded, original.dateAdded);
    QCOMPARE(restored.lastPlayed, original.lastPlayed);
    QCOMPARE(restored.playCount, original.playCount);
    QCOMPARE(restored.favorite, original.favorite);
}

void TestModels::testSongFromVariantMapPartial()
{
    QVariantMap map;
    map[QStringLiteral("id")] = 7;
    map[QStringLiteral("path")] = QStringLiteral("/music/partial.mp3");

    // Missing keys fall back to safe defaults instead of crashing.
    const Song song = Song::fromVariantMap(map);
    QCOMPARE(song.id, 7);
    QCOMPARE(song.path, QStringLiteral("/music/partial.mp3"));
    QVERIFY(song.title.isEmpty());
    QCOMPARE(song.year, 0);
    QCOMPARE(song.duration, qint64(0));
    QCOMPARE(song.playCount, 0);
    QVERIFY(!song.favorite);
}

void TestModels::testAlbumDefaultValues()
{
    Album album;
    QCOMPARE(album.id, 0);
    QVERIFY(album.title.isEmpty());
    QVERIFY(album.artist.isEmpty());
    QCOMPARE(album.year, 0);
    QCOMPARE(album.songCount, 0);
    QVERIFY(album.artworkPath.isEmpty());
}

void TestModels::testAlbumVariantRoundTrip()
{
    Album original;
    original.id = 11;
    original.title = QStringLiteral("Dark Side of the Moon");
    original.artist = QStringLiteral("Pink Floyd");
    original.year = 1973;
    original.songCount = 10;
    original.artworkPath = QStringLiteral("/artwork/dsotm.jpg");

    const Album restored = Album::fromVariantMap(original.toVariantMap());
    QCOMPARE(restored.id, original.id);
    QCOMPARE(restored.title, original.title);
    QCOMPARE(restored.artist, original.artist);
    QCOMPARE(restored.year, original.year);
    QCOMPARE(restored.songCount, original.songCount);
    QCOMPARE(restored.artworkPath, original.artworkPath);
}

void TestModels::testArtistDefaultValues()
{
    Artist artist;
    QCOMPARE(artist.id, 0);
    QVERIFY(artist.name.isEmpty());
    QCOMPARE(artist.songCount, 0);
    QCOMPARE(artist.albumCount, 0);
}

void TestModels::testArtistVariantRoundTrip()
{
    Artist original;
    original.id = 3;
    original.name = QStringLiteral("Queen");
    original.songCount = 15;
    original.albumCount = 4;

    const Artist restored = Artist::fromVariantMap(original.toVariantMap());
    QCOMPARE(restored.id, original.id);
    QCOMPARE(restored.name, original.name);
    QCOMPARE(restored.songCount, original.songCount);
    QCOMPARE(restored.albumCount, original.albumCount);
}

void TestModels::testGenreDefaultValues()
{
    Genre genre;
    QCOMPARE(genre.id, 0);
    QVERIFY(genre.name.isEmpty());
    QCOMPARE(genre.songCount, 0);
}

void TestModels::testGenreVariantRoundTrip()
{
    Genre original;
    original.id = 9;
    original.name = QStringLiteral("Jazz");
    original.songCount = 23;

    const Genre restored = Genre::fromVariantMap(original.toVariantMap());
    QCOMPARE(restored.id, original.id);
    QCOMPARE(restored.name, original.name);
    QCOMPARE(restored.songCount, original.songCount);
}

void TestModels::testPlaylistDefaults()
{
    Playlist playlist;
    QCOMPARE(playlist.id, qint64(0));
    QVERIFY(playlist.name.isEmpty());
    QVERIFY(playlist.description.isEmpty());
    QVERIFY(!playlist.dateCreated.isValid());
    QCOMPARE(playlist.songCount, 0);
    QVERIFY(!playlist.isValid());
}

void TestModels::testPlaylistConstructor()
{
    const QDateTime created(QDate(2024, 5, 1), QTime(12, 0));
    Playlist playlist(5, QStringLiteral("Workout"), QStringLiteral("Gym"),
                      created, 12);

    QCOMPARE(playlist.id, qint64(5));
    QCOMPARE(playlist.name, QStringLiteral("Workout"));
    QCOMPARE(playlist.description, QStringLiteral("Gym"));
    QCOMPARE(playlist.dateCreated, created);
    QCOMPARE(playlist.songCount, 12);
    QVERIFY(playlist.isValid());
}

void TestModels::testPlaylistIsValid()
{
    Playlist emptyName;
    emptyName.id = 1;
    QVERIFY(!emptyName.isValid());

    Playlist zeroId;
    zeroId.name = QStringLiteral("No id");
    QVERIFY(!zeroId.isValid());

    Playlist valid;
    valid.id = 1;
    valid.name = QStringLiteral("Valid");
    QVERIFY(valid.isValid());
}

QTEST_MAIN(TestModels)
#include "tst_models.moc"