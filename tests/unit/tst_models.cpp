#include <QtTest>
#include <QDateTime>

#include "models/Song.h"
#include "models/Album.h"
#include "models/Artist.h"
#include "models/Playlist.h"
#include "library/SongListModel.h"

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
    void testSongListModel();
    void testSongListModelIncremental();
    void testSongListModelSort();
    void testSongListModelPathsForIds();

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

void TestModels::testSongListModel()
{
    SongListModel model;

    // A fresh model has no rows.
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.rowOfId(1), -1);
    QVERIFY(model.songData(0).isEmpty());

    QVector<Song> songs;
    Song s1;
    s1.id = 1;
    s1.path = QStringLiteral("/music/one.flac");
    s1.title = QStringLiteral("One");
    s1.artist = QStringLiteral("Artist A");
    s1.album = QStringLiteral("Album X");
    s1.genre = QStringLiteral("Rock");
    s1.year = 2020;
    s1.trackNumber = 1;
    s1.discNumber = 1;
    s1.duration = 190000;
    s1.artworkPath = QStringLiteral("/art/one.jpg");
    s1.favorite = true;
    s1.playCount = 9;
    s1.lastPlayed = QDateTime(QDate(2024, 3, 1), QTime(8, 0));
    Song s2;
    s2.id = 7;
    s2.title = QStringLiteral("Two");
    s2.duration = 210000;
    songs << s1 << s2;

    model.setSongs(songs);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.count(), 2);

    // toVariantList() is the queue-building path: one map per stored song.
    const QVariantList variants = model.toVariantList();
    QCOMPARE(variants.size(), 2);
    QCOMPARE(variants.at(0).toMap().value(QStringLiteral("id")).toInt(), 1);
    QCOMPARE(variants.at(1).toMap().value(QStringLiteral("title")).toString(), s2.title);

    // Role access matches the QML-facing map names.
    const QModelIndex top = model.index(0);
    QCOMPARE(model.data(top, SongListModel::IdRole).toInt(), 1);
    QCOMPARE(model.data(top, SongListModel::TitleRole).toString(), s1.title);
    QCOMPARE(model.data(top, SongListModel::ArtistRole).toString(), s1.artist);
    QCOMPARE(model.data(top, SongListModel::AlbumRole).toString(), s1.album);
    QCOMPARE(model.data(top, SongListModel::GenreRole).toString(), s1.genre);
    QCOMPARE(model.data(top, SongListModel::YearRole).toInt(), 2020);
    QCOMPARE(model.data(top, SongListModel::TrackNumberRole).toInt(), 1);
    QCOMPARE(model.data(top, SongListModel::DiscNumberRole).toInt(), 1);
    QCOMPARE(model.data(top, SongListModel::DurationRole).toLongLong(), qint64(190000));
    QCOMPARE(model.data(top, SongListModel::ArtworkRole).toString(), s1.artworkPath);
    QCOMPARE(model.data(top, SongListModel::FavoriteRole).toBool(), true);
    QCOMPARE(model.data(top, SongListModel::PlayCountRole).toInt(), 9);
    QCOMPARE(model.data(top, SongListModel::LastPlayedRole).toDateTime(), s1.lastPlayed);
    QCOMPARE(model.data(model.index(1), SongListModel::TitleRole).toString(), s2.title);

    // Invalid rows return a null QVariant, never a crash.
    QVERIFY(!model.data(QModelIndex(), SongListModel::TitleRole).isValid());
    QVERIFY(!model.data(model.index(-1), SongListModel::TitleRole).isValid());
    QVERIFY(!model.data(model.index(5), SongListModel::TitleRole).isValid());

    // Helpers for QML scripts.
    QCOMPARE(model.rowOfId(7), 1);
    QCOMPARE(model.rowOfId(999), -1);
    QCOMPARE(model.songData(1).value(QStringLiteral("title")).toString(), s2.title);
    QVERIFY(model.songData(9).isEmpty());

    // roleNames() is what Qt Quick uses to expose roles as model.<name>.
    const QHash<int, QByteArray> roles = model.roleNames();
    QCOMPARE(roles.value(SongListModel::TitleRole), QByteArray("title"));
    QCOMPARE(roles.value(SongListModel::IdRole), QByteArray("id"));
    QCOMPARE(roles.value(SongListModel::FavoriteRole), QByteArray("favorite"));
    QCOMPARE(roles.value(SongListModel::PlayCountRole), QByteArray("playCount"));
    QCOMPARE(roles.value(SongListModel::LastPlayedRole), QByteArray("lastPlayed"));
}

void TestModels::testSongListModelPathsForIds()
{
    SongListModel model;
    auto make = [](int id, const QString &path) {
        Song song;
        song.id = id;
        song.path = path;
        return song;
    };
    QVector<Song> songs;
    songs << make(1, QStringLiteral("/music/one.flac"))
          << make(2, QStringLiteral("/music/two.flac"))
          << make(3, QString()); // no path: never yielded
    model.setSongs(songs);

    // Resolves the requested ids in row order; unknown ids are ignored.
    QCOMPARE(model.pathsForIds(QVariantList{3, 1, 999}),
             QStringList{QStringLiteral("/music/one.flac")});

    // An empty request is an empty result.
    QVERIFY(model.pathsForIds(QVariantList()).isEmpty());
}

void TestModels::testSongListModelIncremental()
{
    SongListModel model;

    // Helper: a song whose only identity is id + title (path derived from it).
    auto make = [](int id, const QString &title) {
        Song s;
        s.id = id;
        s.title = title;
        s.path = QStringLiteral("/music/") + title + QStringLiteral(".flac");
        return s;
    };

    QVector<Song> seed;
    seed << make(1, QStringLiteral("Alpha"))
         << make(2, QStringLiteral("Bravo"))
         << make(3, QStringLiteral("Charlie"));
    model.setSongs(seed);
    QCOMPARE(model.rowCount(), 3);

    // A rescan that changes one existing song's non-sort data (duration) and
    // adds one new song, with every other row keeping its relative order.
    // Expect row-level signals, never a full reset.
    QVector<Song> scan;
    scan << make(1, QStringLiteral("Alpha"))
         << make(2, QStringLiteral("Bravo"))
         << make(9, QStringLiteral("Delta"))
         << make(3, QStringLiteral("Charlie"));
    scan[1].duration = 12345; // id 2 keeps its place, only its data changes

    QSignalSpy insertedSpy(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removedSpy(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy changedSpy(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy resetSpy(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countSpy(&model, &SongListModel::countChanged);

    model.applySnapshot(scan);

    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.count(), 4);
    QCOMPARE(insertedSpy.count(), 1); // only the new song
    QCOMPARE(removedSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 1);  // only id 2 changed
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(countSpy.count(), 1);

    QCOMPARE(model.data(model.index(0), SongListModel::IdRole).toInt(), 1);
    QCOMPARE(model.data(model.index(1), SongListModel::IdRole).toInt(), 2);
    QCOMPARE(model.data(model.index(2), SongListModel::IdRole).toInt(), 9);
    QCOMPARE(model.data(model.index(3), SongListModel::IdRole).toInt(), 3);
    QCOMPARE(model.data(model.index(1), SongListModel::DurationRole).toInt(), 12345);

    // Removal + insertion: drop ids 1 and 9, add id 5 at the front.
    QVector<Song> prune;
    prune << make(5, QStringLiteral("Aardvark"))
          << make(2, QStringLiteral("Bravo"))
          << make(3, QStringLiteral("Charlie"));
    insertedSpy.clear();
    removedSpy.clear();
    resetSpy.clear();
    model.applySnapshot(prune);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(insertedSpy.count(), 1);
    QCOMPARE(removedSpy.count(), 2);
    QCOMPARE(resetSpy.count(), 0);
    QCOMPARE(model.data(model.index(0), SongListModel::IdRole).toInt(), 5);
    QCOMPARE(model.data(model.index(1), SongListModel::IdRole).toInt(), 2);
    QCOMPARE(model.data(model.index(2), SongListModel::IdRole).toInt(), 3);

    // A kept id whose sort key changed (title) must move: fall back to a full
    // reset rather than a wrong in-place update.
    QVector<Song> retitled;
    retitled << make(2, QStringLiteral("Bravo"))
             << make(3, QStringLiteral("Charlie"))
             << make(5, QStringLiteral("Zulu"));
    resetSpy.clear();
    model.applySnapshot(retitled);
    QCOMPARE(resetSpy.count(), 1);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(2), SongListModel::IdRole).toInt(), 5);
}

void TestModels::testSongListModelSort()
{
    SongListModel model;

    auto make = [](int id, const QString &title, int playCount) {
        Song s;
        s.id = id;
        s.title = title;
        s.playCount = playCount;
        return s;
    };

    QVector<Song> songs;
    songs << make(1, QStringLiteral("Charlie"), 3)
          << make(2, QStringLiteral("Alpha"), 1)
          << make(3, QStringLiteral("Bravo"), 2);
    model.setSongs(songs);

    // Ascending by title.
    model.sort(SongListModel::TitleRole);
    QCOMPARE(model.data(model.index(0), SongListModel::TitleRole).toString(),
             QStringLiteral("Alpha"));
    QCOMPARE(model.data(model.index(1), SongListModel::TitleRole).toString(),
             QStringLiteral("Bravo"));
    QCOMPARE(model.data(model.index(2), SongListModel::TitleRole).toString(),
             QStringLiteral("Charlie"));

    // Descending by playCount.
    model.sort(SongListModel::PlayCountRole, Qt::DescendingOrder);
    QCOMPARE(model.data(model.index(0), SongListModel::PlayCountRole).toInt(), 3);
    QCOMPARE(model.data(model.index(1), SongListModel::PlayCountRole).toInt(), 2);
    QCOMPARE(model.data(model.index(2), SongListModel::PlayCountRole).toInt(), 1);

    // Empty and single-row models are a no-op.
    SongListModel tiny;
    tiny.sort(SongListModel::TitleRole);
    QCOMPARE(tiny.rowCount(), 0);
    Song solo;
    solo.id = 1;
    solo.title = QStringLiteral("Solo");
    QVector<Song> soloList;
    soloList << solo;
    tiny.setSongs(soloList);
    tiny.sort(SongListModel::TitleRole);
    QCOMPARE(tiny.rowCount(), 1);
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
