#include <QtTest>
#include <QDateTime>

#include "models/Song.h"
#include "models/Album.h"
#include "models/Artist.h"
#include "models/Playlist.h"

/**
 * @brief Tests for the real domain models (src/models).
 *
 * Validates default values and the canonical Song::toVariantMap() mapping.
 */
class TestModels : public QObject
{
    Q_OBJECT

private slots:
    void testSongDefaultValues();
    void testSongToVariantMap();

    void testAlbumDefaultValues();

    void testArtistDefaultValues();

    void testPlaylistDefaults();
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

void TestModels::testSongToVariantMap()
{
    Song song;
    song.id = 42;
    song.path = QStringLiteral("/music/song.flac");
    song.title = QStringLiteral("My Song");
    song.artist = QStringLiteral("My Artist");
    song.album = QStringLiteral("My Album");
    song.genre = QStringLiteral("Rock");
    song.year = 2024;
    song.trackNumber = 3;
    song.discNumber = 1;
    song.duration = 240000;
    song.artworkPath = QStringLiteral("/artwork/cover.jpg");
    song.dateAdded = QDateTime(QDate(2024, 1, 15), QTime(10, 30));
    song.lastPlayed = QDateTime(QDate(2024, 1, 20), QTime(21, 0));
    song.playCount = 15;
    song.favorite = true;

    // The canonical mapping exposes exactly the keys QML binds to.
    const QVariantMap map = song.toVariantMap();
    QCOMPARE(map.size(), 15);
    QCOMPARE(map.value(QStringLiteral("id")).toInt(), 42);
    QCOMPARE(map.value(QStringLiteral("path")).toString(), song.path);
    QCOMPARE(map.value(QStringLiteral("title")).toString(), song.title);
    QCOMPARE(map.value(QStringLiteral("artist")).toString(), song.artist);
    QCOMPARE(map.value(QStringLiteral("album")).toString(), song.album);
    QCOMPARE(map.value(QStringLiteral("genre")).toString(), song.genre);
    QCOMPARE(map.value(QStringLiteral("year")).toInt(), song.year);
    QCOMPARE(map.value(QStringLiteral("trackNumber")).toInt(), song.trackNumber);
    QCOMPARE(map.value(QStringLiteral("discNumber")).toInt(), song.discNumber);
    QCOMPARE(map.value(QStringLiteral("duration")).toLongLong(), song.duration);
    // The QML-facing key is "artwork", not the struct/DB name artworkPath.
    QCOMPARE(map.value(QStringLiteral("artwork")).toString(), song.artworkPath);
    QCOMPARE(map.value(QStringLiteral("dateAdded")).toDateTime(), song.dateAdded);
    QCOMPARE(map.value(QStringLiteral("lastPlayed")).toDateTime(), song.lastPlayed);
    QCOMPARE(map.value(QStringLiteral("playCount")).toInt(), song.playCount);
    QCOMPARE(map.value(QStringLiteral("favorite")).toBool(), song.favorite);
    QVERIFY(!map.contains(QStringLiteral("artworkPath")));
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

void TestModels::testArtistDefaultValues()
{
    Artist artist;
    QCOMPARE(artist.id, 0);
    QVERIFY(artist.name.isEmpty());
    QCOMPARE(artist.songCount, 0);
    QCOMPARE(artist.albumCount, 0);
}

void TestModels::testPlaylistDefaults()
{
    Playlist playlist;
    QCOMPARE(playlist.id, qint64(0));
    QVERIFY(playlist.name.isEmpty());
    QVERIFY(playlist.description.isEmpty());
    QVERIFY(!playlist.dateCreated.isValid());
    QCOMPARE(playlist.songCount, 0);
}

QTEST_MAIN(TestModels)
#include "tst_models.moc"
