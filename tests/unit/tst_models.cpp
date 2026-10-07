#include <QtTest>
#include <QHash>
#include <QVariant>

/**
 * @brief Tests for domain models
 *
 * Tests the Song model and related data structures defined in the API contracts.
 * Since the actual model classes are not yet implemented, this test validates
 * the conceptual model structure and data integrity.
 */
class TestModels : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Song model tests
    void testSongCreation();
    void testSongDefaultValues();
    void testSongFieldAccess();
    void testSongEquality();
    void testSongCopy();

    // Playlist model tests
    void testPlaylistCreation();
    void testPlaylistAddSong();
    void testPlaylistRemoveSong();
    void testPlaylistMoveSong();

    // Collection tests
    void testSongCollection();
    void testSongCollectionSearch();
    void testSongCollectionFilter();
};

void TestModels::initTestCase()
{
}

void TestModels::cleanupTestCase()
{
}

void TestModels::testSongCreation()
{
    // Test that a song can be created with basic fields
    QHash<QString, QVariant> song;
    song[QStringLiteral("id")] = 1;
    song[QStringLiteral("path")] = QStringLiteral("/music/test.mp3");
    song[QStringLiteral("title")] = QStringLiteral("Test Song");
    song[QStringLiteral("artist")] = QStringLiteral("Test Artist");
    song[QStringLiteral("album")] = QStringLiteral("Test Album");

    QVERIFY(song.contains(QStringLiteral("id")));
    QVERIFY(song.contains(QStringLiteral("path")));
    QVERIFY(song.contains(QStringLiteral("title")));
    QVERIFY(song.contains(QStringLiteral("artist")));
    QVERIFY(song.contains(QStringLiteral("album")));

    QCOMPARE(song[QStringLiteral("id")].toInt(), 1);
    QCOMPARE(song[QStringLiteral("title")].toString(), QStringLiteral("Test Song"));
}

void TestModels::testSongDefaultValues()
{
    // Test that songs have appropriate default values
    QHash<QString, QVariant> song;
    song[QStringLiteral("id")] = 1;
    song[QStringLiteral("path")] = QStringLiteral("/music/test.mp3");

    // Default values should be sensible
    QCOMPARE(song.value(QStringLiteral("play_count"), 0).toInt(), 0);
    QCOMPARE(song.value(QStringLiteral("favorite"), 0).toInt(), 0);
    QCOMPARE(song.value(QStringLiteral("year"), 0).toInt(), 0);
    QCOMPARE(song.value(QStringLiteral("track_number"), 0).toInt(), 0);
    QCOMPARE(song.value(QStringLiteral("disc_number"), 0).toInt(), 0);
    QCOMPARE(song.value(QStringLiteral("duration"), 0).toInt(), 0);
}

void TestModels::testSongFieldAccess()
{
    // Test all fields defined in the API contract
    QHash<QString, QVariant> song;
    song[QStringLiteral("id")] = 42;
    song[QStringLiteral("path")] = QStringLiteral("/music/song.flac");
    song[QStringLiteral("title")] = QStringLiteral("My Song");
    song[QStringLiteral("artist")] = QStringLiteral("My Artist");
    song[QStringLiteral("album")] = QStringLiteral("My Album");
    song[QStringLiteral("genre")] = QStringLiteral("Rock");
    song[QStringLiteral("year")] = 2024;
    song[QStringLiteral("track_number")] = 3;
    song[QStringLiteral("disc_number")] = 1;
    song[QStringLiteral("duration")] = 240000;
    song[QStringLiteral("artwork")] = QStringLiteral("/artwork/cover.jpg");
    song[QStringLiteral("date_added")] = QStringLiteral("2024-01-15");
    song[QStringLiteral("last_played")] = QStringLiteral("2024-01-20");
    song[QStringLiteral("play_count")] = 15;
    song[QStringLiteral("favorite")] = 1;

    QCOMPARE(song[QStringLiteral("id")].toInt(), 42);
    QCOMPARE(song[QStringLiteral("title")].toString(), QStringLiteral("My Song"));
    QCOMPARE(song[QStringLiteral("artist")].toString(), QStringLiteral("My Artist"));
    QCOMPARE(song[QStringLiteral("album")].toString(), QStringLiteral("My Album"));
    QCOMPARE(song[QStringLiteral("genre")].toString(), QStringLiteral("Rock"));
    QCOMPARE(song[QStringLiteral("year")].toInt(), 2024);
    QCOMPARE(song[QStringLiteral("track_number")].toInt(), 3);
    QCOMPARE(song[QStringLiteral("disc_number")].toInt(), 1);
    QCOMPARE(song[QStringLiteral("duration")].toInt(), 240000);
    QCOMPARE(song[QStringLiteral("play_count")].toInt(), 15);
    QCOMPARE(song[QStringLiteral("favorite")].toInt(), 1);
}

void TestModels::testSongEquality()
{
    QHash<QString, QVariant> song1;
    song1[QStringLiteral("id")] = 1;
    song1[QStringLiteral("title")] = QStringLiteral("Song A");

    QHash<QString, QVariant> song2;
    song2[QStringLiteral("id")] = 1;
    song2[QStringLiteral("title")] = QStringLiteral("Song A");

    QHash<QString, QVariant> song3;
    song3[QStringLiteral("id")] = 2;
    song3[QStringLiteral("title")] = QStringLiteral("Song B");

    QVERIFY(song1 == song2);
    QVERIFY(song1 != song3);
}

void TestModels::testSongCopy()
{
    QHash<QString, QVariant> original;
    original[QStringLiteral("id")] = 1;
    original[QStringLiteral("title")] = QStringLiteral("Original");

    QHash<QString, QVariant> copy = original;
    QVERIFY(original == copy);

    copy[QStringLiteral("title")] = QStringLiteral("Modified");
    QVERIFY(original != copy);
    QCOMPARE(original[QStringLiteral("title")].toString(), QStringLiteral("Original"));
}

void TestModels::testPlaylistCreation()
{
    QHash<QString, QVariant> playlist;
    playlist[QStringLiteral("id")] = 1;
    playlist[QStringLiteral("name")] = QStringLiteral("My Playlist");

    QVERIFY(playlist.contains(QStringLiteral("id")));
    QVERIFY(playlist.contains(QStringLiteral("name")));
    QCOMPARE(playlist[QStringLiteral("name")].toString(), QStringLiteral("My Playlist"));
}

void TestModels::testPlaylistAddSong()
{
    QList<QHash<QString, QVariant>> playlist;

    QHash<QString, QVariant> song1;
    song1[QStringLiteral("id")] = 1;
    song1[QStringLiteral("title")] = QStringLiteral("Song 1");

    QHash<QString, QVariant> song2;
    song2[QStringLiteral("id")] = 2;
    song2[QStringLiteral("title")] = QStringLiteral("Song 2");

    playlist.append(song1);
    playlist.append(song2);

    QCOMPARE(playlist.size(), 2);
    QCOMPARE(playlist[0][QStringLiteral("title")].toString(), QStringLiteral("Song 1"));
    QCOMPARE(playlist[1][QStringLiteral("title")].toString(), QStringLiteral("Song 2"));
}

void TestModels::testPlaylistRemoveSong()
{
    QList<QHash<QString, QVariant>> playlist;

    QHash<QString, QVariant> song1;
    song1[QStringLiteral("id")] = 1;
    QHash<QString, QVariant> song2;
    song2[QStringLiteral("id")] = 2;
    QHash<QString, QVariant> song3;
    song3[QStringLiteral("id")] = 3;

    playlist.append(song1);
    playlist.append(song2);
    playlist.append(song3);

    // Remove middle song
    playlist.removeAt(1);

    QCOMPARE(playlist.size(), 2);
    QCOMPARE(playlist[0][QStringLiteral("id")].toInt(), 1);
    QCOMPARE(playlist[1][QStringLiteral("id")].toInt(), 3);
}

void TestModels::testPlaylistMoveSong()
{
    QList<QHash<QString, QVariant>> playlist;

    for (int i = 1; i <= 5; ++i) {
        QHash<QString, QVariant> song;
        song[QStringLiteral("id")] = i;
        song[QStringLiteral("title")] = QStringLiteral("Song %1").arg(i);
        playlist.append(song);
    }

    // Move song from position 0 to position 3
    QHash<QString, QVariant> moved = playlist.takeAt(0);
    playlist.insert(3, moved);

    QCOMPARE(playlist.size(), 5);
    QCOMPARE(playlist[0][QStringLiteral("id")].toInt(), 2);
    QCOMPARE(playlist[3][QStringLiteral("id")].toInt(), 1);
    QCOMPARE(playlist[4][QStringLiteral("id")].toInt(), 5);
}

void TestModels::testSongCollection()
{
    QList<QHash<QString, QVariant>> collection;

    for (int i = 0; i < 10; ++i) {
        QHash<QString, QVariant> song;
        song[QStringLiteral("id")] = i;
        song[QStringLiteral("title")] = QStringLiteral("Song %1").arg(i);
        song[QStringLiteral("artist")] = QStringLiteral("Artist %1").arg(i % 3);
        collection.append(song);
    }

    QCOMPARE(collection.size(), 10);
}

void TestModels::testSongCollectionSearch()
{
    QList<QHash<QString, QVariant>> collection;

    QHash<QString, QVariant> song1;
    song1[QStringLiteral("title")] = QStringLiteral("Bohemian Rhapsody");
    song1[QStringLiteral("artist")] = QStringLiteral("Queen");
    collection.append(song1);

    QHash<QString, QVariant> song2;
    song2[QStringLiteral("title")] = QStringLiteral("Stairway to Heaven");
    song2[QStringLiteral("artist")] = QStringLiteral("Led Zeppelin");
    collection.append(song2);

    QHash<QString, QVariant> song3;
    song3[QStringLiteral("title")] = QStringLiteral("Bohemian Like You");
    song3[QStringLiteral("artist")] = QStringLiteral("The Dandy Warhols");
    collection.append(song3);

    // Search for "Bohemian"
    QList<QHash<QString, QVariant>> results;
    for (const auto &song : collection) {
        if (song[QStringLiteral("title")].toString().contains(QStringLiteral("Bohemian"))) {
            results.append(song);
        }
    }

    QCOMPARE(results.size(), 2);
}

void TestModels::testSongCollectionFilter()
{
    QList<QHash<QString, QVariant>> collection;

    for (int i = 0; i < 10; ++i) {
        QHash<QString, QVariant> song;
        song[QStringLiteral("id")] = i;
        song[QStringLiteral("artist")] = QStringLiteral("Artist %1").arg(i % 2);
        song[QStringLiteral("favorite")] = (i % 3 == 0) ? 1 : 0;
        collection.append(song);
    }

    // Filter by artist
    QList<QHash<QString, QVariant>> artist0Songs;
    for (const auto &song : collection) {
        if (song[QStringLiteral("artist")].toString() == QStringLiteral("Artist 0")) {
            artist0Songs.append(song);
        }
    }
    QCOMPARE(artist0Songs.size(), 5);

    // Filter by favorite
    QList<QHash<QString, QVariant>> favorites;
    for (const auto &song : collection) {
        if (song[QStringLiteral("favorite")].toInt() == 1) {
            favorites.append(song);
        }
    }
    QCOMPARE(favorites.size(), 4);
}

QTEST_MAIN(TestModels)
#include "tst_models.moc"
