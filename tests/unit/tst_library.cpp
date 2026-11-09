#include <QtTest>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSqlQuery>

#include "../../src/library/LibraryController.h"
#include "../TestHelpers.h"

/**
 * @brief Tests for LibraryController
 *
 * Verifies directory scanning, metadata-driven song ingestion,
 * search and change notification. The database is redirected to
 * Qt's test mode so the user's real library is never touched.
 */
class tst_library : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    void testScanFindsAudioFiles();
    void testScanIgnoresUnsupportedFiles();
    void testRescanDoesNotDuplicate();
    void testSearch();
    void testSearchModelReflectsQuery();
    void testScanSignals();
    void testIgnorePathHidesSong();
    void testIgnoreFolderHidesChildren();
    void testUnignoreRestoresSongs();
    void testRescanSkipsIgnored();
    void testRemoveSongsDeletesFile();
    void testRemoveSongsIgnoresFile();

    // ---- Tag editing (Option D) ----
    void testUpdateSongEmitsLibraryChangedOnce();
    void testUpdateSongRejectsBadInput();
    void testUpdateSongsBatchEmitsOnce();
    void testUpdateSongsAcceptsPartialMaps();
    void testRenameArtistEmitsOnceAndCachesFollow();
    void testSetAlbumIdentityEmitsOnceAndFollowsSongs();
    void testSetArtworkEmitsOnceAndCachesFile();
    void testSetAlbumIdentityEmitsOnceAndMovesOnlyThisAlbum();
    void testSetAlbumIdentityAcceptsACombinedTitleAndArtistChange();
    void testSetAlbumArtworkEmitsOnceAndCoversEverySong();

private:
    void clearTempDir();
    // Insert a bare song row on the controller's shared connection so a
    // test can edit without scanning. Returns 0 on failure (QVERIFY
    // cannot appear in a function with a return value).
    qint64 insertSongRow(const QString &path, const QString &title);
    // Row of the given song id in the full-library render model, or -1.
    int rowOfId(qint64 id) const;

    LibraryController *m_controller = nullptr;
    QTemporaryDir m_tempDir;
};

void tst_library::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("tst_library"));

    QVERIFY(m_tempDir.isValid());
    TestHelpers::removeDatabaseFiles();
    m_controller = new LibraryController(this);
}

void tst_library::cleanupTestCase()
{
    delete m_controller;
    m_controller = nullptr;
}

void tst_library::clearTempDir()
{
    QDir root(m_tempDir.path());
    const auto entries = root.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &info : entries) {
        if (info.isDir()) {
            QDir(info.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

void tst_library::init()
{
    // Every test starts with an empty directory and an empty library.
    // The controller shares the default database connection, so clearing
    // through a plain query on that connection is sufficient.
    clearTempDir();

    QSqlQuery query;
    QVERIFY(query.exec(QStringLiteral("DELETE FROM songs")));
    // Stale "ignored" paths would keep filtering songs in later tests
    // (the database connection is shared across tests).
    QVERIFY(query.exec(QStringLiteral("DELETE FROM ignored_paths")));
    m_controller->refresh();
    QCOMPARE(m_controller->songCount(), 0);
}

void tst_library::testScanFindsAudioFiles()
{
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 5);
    QCOMPARE(files.size(), 5);

    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));

    QCOMPARE(m_controller->songCount(), 5);
    QCOMPARE(m_controller->songs().size(), 5);
}

void tst_library::testScanIgnoresUnsupportedFiles()
{
    // Only non-audio files exist in the directory
    QFile txtFile(m_tempDir.filePath(QStringLiteral("readme.txt")));
    QVERIFY(txtFile.open(QIODevice::WriteOnly));
    txtFile.write("not music");
    txtFile.close();

    QFile listFile(m_tempDir.filePath(QStringLiteral("playlist.m3u")));
    QVERIFY(listFile.open(QIODevice::WriteOnly));
    listFile.write("#EXTM3U");
    listFile.close();

    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));

    QCOMPARE(m_controller->songCount(), 0);
}

void tst_library::testRescanDoesNotDuplicate()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 3);

    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 3);

    // Scanning the same directory again must not create duplicates
    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 3);
}

void tst_library::testSearch()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 4);
    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 4);

    // File names follow the pattern trackNN_Artist_Album.ext
    const QVector<QVariant> results = m_controller->search(QStringLiteral("track01"));
    QCOMPARE(results.size(), 1);

    // A query matching nothing must return an empty list
    QVERIFY(m_controller->search(QStringLiteral("zzz_no_such_song")).isEmpty());
}

void tst_library::testSearchModelReflectsQuery()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 4);
    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 4);

    SongListModel *searchModel = m_controller->searchModel();
    SongListModel *songsModel = m_controller->songsModel();
    QVERIFY(searchModel);
    QVERIFY(songsModel);
    QCOMPARE(songsModel->count(), 4);
    const int firstFullId = songsModel->songData(0).value("id").toInt();

    // An empty query clears the search model and reports "not searching".
    m_controller->setSearch(QString());
    QVERIFY(!m_controller->searchActive());
    QCOMPARE(searchModel->count(), 0);

    // A matching query fills the search model with the matching rows.
    m_controller->setSearch(QStringLiteral("track01"));
    QVERIFY(m_controller->searchActive());
    QCOMPARE(searchModel->count(), 1);
    const QVariantMap match = searchModel->songData(0);
    QVERIFY(match.value("path").toString().contains(QStringLiteral("track01")));

    // The single search row must carry the same id and title as the matching
    // row in the full-library model (titles fall back to the file name for
    // the tag-less test files).
    int expectedRow = -1;
    for (int i = 0; i < songsModel->count(); ++i) {
        if (songsModel->songData(i).value("path").toString()
                .contains(QStringLiteral("track01"))) {
            expectedRow = i;
            break;
        }
    }
    QVERIFY(expectedRow >= 0);
    const QVariantMap expected = songsModel->songData(expectedRow);
    QCOMPARE(match.value("id").toInt(), expected.value("id").toInt());
    QCOMPARE(match.value("title").toString(), expected.value("title").toString());
    QVERIFY(!match.value("title").toString().isEmpty());

    // A query matching nothing leaves the search active but empty.
    m_controller->setSearch(QStringLiteral("zzz_no_such_song"));
    QVERIFY(m_controller->searchActive());
    QCOMPARE(searchModel->count(), 0);

    // Searching never touches the full-library model.
    QCOMPARE(songsModel->count(), 4);
    QCOMPARE(songsModel->songData(0).value("id").toInt(), firstFullId);
}

void tst_library::testScanSignals()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 2);

    QSignalSpy startedSpy(m_controller, &LibraryController::scanStarted);
    QSignalSpy finishedSpy(m_controller, &LibraryController::scanFinished);
    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(startedSpy.isValid());
    QVERIFY(finishedSpy.isValid());
    QVERIFY(changedSpy.isValid());

    m_controller->scan(m_tempDir.path());

    // scanStarted() is emitted synchronously; the rest arrive with the
    // worker thread completion.
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(finishedSpy.wait(10000));
    QCOMPARE(finishedSpy.count(), 1);
    QVERIFY(changedSpy.count() >= 1);
}

void tst_library::testIgnorePathHidesSong()
{
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 4);
    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 4);

    // Ignoring one song removes it from the library view but not from disk.
    m_controller->ignorePath(files.first());
    QCOMPARE(m_controller->songCount(), 3);
    QCOMPARE(m_controller->ignoredPaths().size(), 1);
    QVERIFY(m_controller->ignoredPaths().contains(files.first()));

    // The remaining songs are the other three.
    const QVector<QVariant> remaining = m_controller->songs();
    QCOMPARE(remaining.size(), 3);
    for (const QVariant &songVar : remaining) {
        QVERIFY(songVar.toMap().value("path").toString() != files.first());
    }
    QCOMPARE(m_controller->search(files.first().section(QLatin1Char('/'), -1)).size(), 0);
}

void tst_library::testIgnoreFolderHidesChildren()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 2);
    QDir subDir(m_tempDir.filePath(QStringLiteral("compilation")));
    QVERIFY(subDir.mkpath(QStringLiteral(".")));
    TestHelpers::createTestMusicDirectory(subDir.path(), 3);

    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 5);

    // Hiding the folder hides all the songs inside it at once.
    m_controller->ignorePath(subDir.path());
    QCOMPARE(m_controller->songCount(), 2);
}

void tst_library::testUnignoreRestoresSongs()
{
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 3);
    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));

    m_controller->ignorePath(files.first());
    QCOMPARE(m_controller->songCount(), 2);

    // Un-hiding brings the song back without rescanning.
    m_controller->unignorePath(files.first());
    QCOMPARE(m_controller->songCount(), 3);
    QCOMPARE(m_controller->ignoredPaths().size(), 0);
}

void tst_library::testRescanSkipsIgnored()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 2);
    QDir subDir(m_tempDir.filePath(QStringLiteral("soundtrack")));
    QVERIFY(subDir.mkpath(QStringLiteral(".")));
    const QStringList hidden = TestHelpers::createTestMusicDirectory(subDir.path(), 2);

    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 4);

    // Ignore the folder, then rescan: ignored songs are not re-added and
    // the scanner does not even walk into the hidden folder.
    m_controller->ignorePath(subDir.path());
    QCOMPARE(m_controller->songCount(), 2);

    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 2);
    QVERIFY(!hidden.isEmpty());
}

void tst_library::testRemoveSongsDeletesFile()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 2);
    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 2);

    const QVariantMap song = m_controller->songs().first().toMap();
    const qint64 id = song.value("id").toLongLong();
    const QString path = song.value("path").toString();
    QVERIFY(QFile::exists(path));

    // Active mode: the audio file is deleted from disk too.
    QVariantList ids;
    ids << id;
    m_controller->removeSongs(ids, true);

    QVERIFY(!QFile::exists(path));
    QCOMPARE(m_controller->songCount(), 1);
    for (const QVariant &s : m_controller->songs()) {
        QVERIFY(s.toMap().value("id").toLongLong() != id);
    }
}

void tst_library::testRemoveSongsIgnoresFile()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 2);
    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 2);

    const QVariantMap song = m_controller->songs().first().toMap();
    const qint64 id = song.value("id").toLongLong();
    const QString path = song.value("path").toString();
    QVERIFY(QFile::exists(path));

    // Passive mode: the file stays on disk but is registered as ignored.
    QVariantList ids;
    ids << id;
    m_controller->removeSongs(ids, false);

    QVERIFY(QFile::exists(path));
    QCOMPARE(m_controller->songCount(), 1);
    QVERIFY(m_controller->ignoredPaths().contains(path));

    // A rescan must not re-import the ignored path.
    m_controller->scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(m_controller));
    QCOMPARE(m_controller->songCount(), 1);
}

qint64 tst_library::insertSongRow(const QString &path, const QString &title)
{
    QSqlQuery query;
    query.prepare(QStringLiteral("INSERT INTO songs (path, title) VALUES (?, ?)"));
    query.addBindValue(path);
    query.addBindValue(title);
    if (!query.exec()) {
        return 0;
    }
    return query.lastInsertId().toLongLong();
}

int tst_library::rowOfId(qint64 id) const
{
    SongListModel *model = m_controller->songsModel();
    for (int row = 0; row < model->count(); ++row) {
        if (model->songData(row).value(QStringLiteral("id")).toLongLong() == id) {
            return row;
        }
    }
    return -1;
}

// A successful edit fires libraryChanged exactly once, and both the
// render model and the QML-facing songs() cache reflect the new values.
void tst_library::testUpdateSongEmitsLibraryChangedOnce()
{
    const qint64 id = insertSongRow(QStringLiteral("/music/tagged/t11.mp3"),
                                    QStringLiteral("Original Title 11"));
    QVERIFY(id > 0);
    m_controller->refresh();
    QCOMPARE(m_controller->songCount(), 1);

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(changedSpy.isValid());

    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("title")] = QStringLiteral("Edited Title 11");
    map[QStringLiteral("artist")] = QStringLiteral("Edited Artist 11");
    map[QStringLiteral("album")] = QStringLiteral("Edited Album 11");
    QVERIFY(m_controller->updateSong(map));

    QCOMPARE(changedSpy.count(), 1);

    // The render model row shows the new values...
    const int row = rowOfId(id);
    QVERIFY(row >= 0);
    const QVariantMap modelRow = m_controller->songsModel()->songData(row);
    QCOMPARE(modelRow.value(QStringLiteral("title")).toString(),
             QStringLiteral("Edited Title 11"));
    QCOMPARE(modelRow.value(QStringLiteral("artist")).toString(),
             QStringLiteral("Edited Artist 11"));
    QCOMPARE(modelRow.value(QStringLiteral("album")).toString(),
             QStringLiteral("Edited Album 11"));

    // ...and so does the songs() cache QML binds to.
    const QVector<QVariant> songs = m_controller->songs();
    QCOMPARE(songs.size(), 1);
    const QVariantMap song = songs.first().toMap();
    QCOMPARE(song.value(QStringLiteral("title")).toString(),
             QStringLiteral("Edited Title 11"));
    QCOMPARE(song.value(QStringLiteral("artist")).toString(),
             QStringLiteral("Edited Artist 11"));
    QCOMPARE(song.value(QStringLiteral("album")).toString(),
             QStringLiteral("Edited Album 11"));
}

// Bad input is refused with a false return, zero emissions and no change.
void tst_library::testUpdateSongRejectsBadInput()
{
    const qint64 id = insertSongRow(QStringLiteral("/music/tagged/t12.mp3"),
                                    QStringLiteral("Keep Me 12"));
    QVERIFY(id > 0);
    m_controller->refresh();
    QCOMPARE(m_controller->songCount(), 1);

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(changedSpy.isValid());

    // No id at all...
    QVariantMap missingId;
    missingId[QStringLiteral("title")] = QStringLiteral("Nope 12");
    QVERIFY(!m_controller->updateSong(missingId));
    // ...and an id no song carries.
    QVariantMap unknownId;
    unknownId[QStringLiteral("id")] = 999999;
    unknownId[QStringLiteral("title")] = QStringLiteral("Nope 12");
    QVERIFY(!m_controller->updateSong(unknownId));

    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(m_controller->songs().first().toMap()
                 .value(QStringLiteral("title")).toString(),
             QStringLiteral("Keep Me 12"));
}

// A full-map batch (what the selection bar sends) emits once and
// reassigns only the chosen field; every other field of every row comes
// back exactly as stored.
void tst_library::testUpdateSongsBatchEmitsOnce()
{
    for (int i = 1; i <= 3; ++i) {
        const qint64 id = insertSongRow(
            QStringLiteral("/music/tagged/batch13_%1.mp3").arg(i),
            QStringLiteral("Batch Title 13 %1").arg(i));
        QVERIFY(id > 0);
    }
    m_controller->refresh();
    QCOMPARE(m_controller->songCount(), 3);

    const QVector<QVariant> before = m_controller->songs();
    QVariantList batch;
    for (const QVariant &entry : before) {
        QVariantMap map = entry.toMap();
        map[QStringLiteral("album")] = QStringLiteral("Batch Album 13");
        batch.append(map);
    }
    QCOMPARE(batch.size(), 3);

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(m_controller->updateSongs(batch));
    QCOMPARE(changedSpy.count(), 1);

    const QVector<QVariant> after = m_controller->songs();
    QCOMPARE(after.size(), 3);
    for (int i = 0; i < after.size(); ++i) {
        const QVariantMap song = after.at(i).toMap();
        const QVariantMap original = before.at(i).toMap();
        QCOMPARE(song.value(QStringLiteral("album")).toString(),
                 QStringLiteral("Batch Album 13"));
        QCOMPARE(song.value(QStringLiteral("title")).toString(),
                 original.value(QStringLiteral("title")).toString());
        QCOMPARE(song.value(QStringLiteral("id")).toLongLong(),
                 original.value(QStringLiteral("id")).toLongLong());
    }
    QCOMPARE(m_controller->albums().size(), 1);
}

// Partial {id, album}-only maps reassign only the album: absent keys
// keep their stored values (the overlay contract that makes a batch
// edit a field-level reassignment, never an equalization).
void tst_library::testUpdateSongsAcceptsPartialMaps()
{
    const qint64 first = insertSongRow(QStringLiteral("/music/tagged/partial14_a.mp3"),
                                       QStringLiteral("Partial A 14"));
    const qint64 second = insertSongRow(QStringLiteral("/music/tagged/partial14_b.mp3"),
                                        QStringLiteral("Partial B 14"));
    QVERIFY(first > 0);
    QVERIFY(second > 0);
    m_controller->refresh();
    QCOMPARE(m_controller->songCount(), 2);

    QVariantMap firstMap;
    firstMap[QStringLiteral("id")] = first;
    firstMap[QStringLiteral("album")] = QStringLiteral("Partial Album 14");
    QVariantMap secondMap;
    secondMap[QStringLiteral("id")] = second;
    secondMap[QStringLiteral("album")] = QStringLiteral("Partial Album 14");
    QVariantList batch;
    batch.append(firstMap);
    batch.append(secondMap);

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(m_controller->updateSongs(batch));
    QCOMPARE(changedSpy.count(), 1);

    const QVector<QVariant> songs = m_controller->songs();
    QCOMPARE(songs.size(), 2);
    for (const QVariant &entry : songs) {
        const QVariantMap song = entry.toMap();
        QCOMPARE(song.value(QStringLiteral("album")).toString(),
                 QStringLiteral("Partial Album 14"));
        const qint64 songId = song.value(QStringLiteral("id")).toLongLong();
        const QString expectedTitle = songId == first
            ? QStringLiteral("Partial A 14") : QStringLiteral("Partial B 14");
        QCOMPARE(song.value(QStringLiteral("title")).toString(), expectedTitle);
    }
}

void tst_library::testRenameArtistEmitsOnceAndCachesFollow()
{
    const qint64 first = insertSongRow(QStringLiteral("/music/tagged/ren15_a.mp3"),
                                       QStringLiteral("Rename A 15"));
    const qint64 second = insertSongRow(QStringLiteral("/music/tagged/ren15_b.mp3"),
                                        QStringLiteral("Rename B 15"));
    QVERIFY(first > 0);
    QVERIFY(second > 0);

    QVariantMap firstMap;
    firstMap[QStringLiteral("id")] = first;
    firstMap[QStringLiteral("artist")] = QStringLiteral("Old Artist 15");
    QVariantMap secondMap;
    secondMap[QStringLiteral("id")] = second;
    secondMap[QStringLiteral("artist")] = QStringLiteral("Old Artist 15");
    QVariantList batch;
    batch.append(firstMap);
    batch.append(secondMap);
    QVERIFY(m_controller->updateSongs(batch));

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(m_controller->renameArtist(QStringLiteral("Old Artist 15"),
                                       QStringLiteral("New Artist 15")));
    QCOMPARE(changedSpy.count(), 1);

    // The artists() cache shows the new name only.
    const QVector<QVariant> artists = m_controller->artists();
    QCOMPARE(artists.size(), 1);
    const QVariantMap artist = artists.first().toMap();
    QCOMPARE(artist.value(QStringLiteral("name")).toString(),
             QStringLiteral("New Artist 15"));
    QCOMPARE(artist.value(QStringLiteral("songCount")).toInt(), 2);

    // Every song followed through the foreign key.
    for (const QVariant &entry : m_controller->songs()) {
        QCOMPARE(entry.toMap().value(QStringLiteral("artist")).toString(),
                 QStringLiteral("New Artist 15"));
    }
}

void tst_library::testSetAlbumIdentityEmitsOnceAndFollowsSongs()
{
    const qint64 id = insertSongRow(QStringLiteral("/music/tagged/album16.mp3"),
                                    QStringLiteral("Album Song 16"));
    QVERIFY(id > 0);
    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("artist")] = QStringLiteral("Album Artist 16");
    map[QStringLiteral("album")] = QStringLiteral("Old Album 16");
    QVERIFY(m_controller->updateSong(map));

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(m_controller->setAlbumIdentity(QStringLiteral("Old Album 16"),
                                           QStringLiteral("Album Artist 16"),
                                           QStringLiteral("New Album 16"),
                                           QStringLiteral("Album Artist 16")));
    QCOMPARE(changedSpy.count(), 1);

    const QVector<QVariant> albums = m_controller->albums();
    QCOMPARE(albums.size(), 1);
    const QVariantMap album = albums.first().toMap();
    QCOMPARE(album.value(QStringLiteral("title")).toString(),
             QStringLiteral("New Album 16"));
    QCOMPARE(album.value(QStringLiteral("artist")).toString(),
             QStringLiteral("Album Artist 16"));
    QCOMPARE(album.value(QStringLiteral("songCount")).toInt(), 1);

    // The song row followed through the foreign key.
    QCOMPARE(m_controller->songs().first().toMap()
                 .value(QStringLiteral("album")).toString(),
             QStringLiteral("New Album 16"));
}

void tst_library::testSetArtworkEmitsOnceAndCachesFile()
{
    const qint64 id = insertSongRow(QStringLiteral("/music/tagged/art17.mp3"),
                                    QStringLiteral("Artwork Song 17"));
    QVERIFY(id > 0);
    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("artist")] = QStringLiteral("Artwork Artist 17");
    map[QStringLiteral("album")] = QStringLiteral("Artwork Album 17");
    QVERIFY(m_controller->updateSong(map));

    // A real (tiny) PNG standing in for the user's picked cover.
    const QString pickedPath = m_tempDir.filePath(QStringLiteral("picked_cover17.png"));
    {
        QFile picked(pickedPath);
        QVERIFY(picked.open(QIODevice::WriteOnly));
        QVERIFY(picked.write(QByteArray::fromHex(TestHelpers::kRedPngHex)) > 0);
        picked.close();
    }

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(m_controller->setArtwork(static_cast<int>(id), pickedPath));
    QCOMPARE(changedSpy.count(), 1);

    const QVariantMap song = m_controller->songs().first().toMap();
    const QString artwork = song.value(QStringLiteral("artwork")).toString();
    QVERIFY(!artwork.isEmpty());
    QVERIFY(QFile::exists(artwork));
    // The image was copied into the app's content-hash artwork cache, not
    // referenced in place.
    const QString artworkDir = QStandardPaths::writableLocation(
                                   QStandardPaths::AppDataLocation)
                               + QStringLiteral("/artwork");
    QVERIFY(artwork.startsWith(artworkDir + QLatin1Char('/')));
    QVERIFY(artwork != pickedPath);

    // The album rollup exposed the same cover, like at scan time.
    QCOMPARE(m_controller->albums().first().toMap()
                 .value(QStringLiteral("artwork")).toString(), artwork);
}

void tst_library::testSetAlbumIdentityEmitsOnceAndMovesOnlyThisAlbum()
{
    // Two songs on the album being moved, one on a sibling album of the
    // same artist: only the first album's tracks may follow.
    const qint64 movedA = insertSongRow(QStringLiteral("/music/tagged/move18a.mp3"),
                                        QStringLiteral("Move Song 18A"));
    const qint64 movedB = insertSongRow(QStringLiteral("/music/tagged/move18b.mp3"),
                                        QStringLiteral("Move Song 18B"));
    const qint64 keeper = insertSongRow(QStringLiteral("/music/tagged/move18c.mp3"),
                                        QStringLiteral("Keeper Song 18"));
    QVERIFY(movedA > 0 && movedB > 0 && keeper > 0);
    const QList<qint64> movedIds = {movedA, movedB};
    for (const qint64 id : movedIds) {
        QVariantMap map;
        map[QStringLiteral("id")] = id;
        map[QStringLiteral("artist")] = QStringLiteral("Old Owner 18");
        map[QStringLiteral("album")] = QStringLiteral("Moved Album 18");
        QVERIFY(m_controller->updateSong(map));
    }
    {
        QVariantMap map;
        map[QStringLiteral("id")] = keeper;
        map[QStringLiteral("artist")] = QStringLiteral("Old Owner 18");
        map[QStringLiteral("album")] = QStringLiteral("Keeper Album 18");
        QVERIFY(m_controller->updateSong(map));
    }

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    // Refused without emitting: unchanged identity, empty target artist,
    // unknown album.
    QVERIFY(!m_controller->setAlbumIdentity(QStringLiteral("Moved Album 18"),
                                            QStringLiteral("Old Owner 18"),
                                            QStringLiteral("Moved Album 18"),
                                            QStringLiteral("Old Owner 18")));
    QVERIFY(!m_controller->setAlbumIdentity(QStringLiteral("Moved Album 18"),
                                            QStringLiteral("Old Owner 18"),
                                            QStringLiteral("Moved Album 18"), QString()));
    QVERIFY(!m_controller->setAlbumIdentity(QStringLiteral("No Such Album 18"),
                                            QStringLiteral("Old Owner 18"),
                                            QStringLiteral("No Such Album 18"),
                                            QStringLiteral("New Owner 18")));
    QCOMPARE(changedSpy.count(), 0);

    QVERIFY(m_controller->setAlbumIdentity(QStringLiteral("Moved Album 18"),
                                           QStringLiteral("Old Owner 18"),
                                           QStringLiteral("Moved Album 18"),
                                           QStringLiteral("New Owner 18")));
    QCOMPARE(changedSpy.count(), 1);

    // The albums cache moved the card to the new artist and left the
    // sibling album under the old one.
    bool sawMoved = false;
    bool sawKeeper = false;
    for (const QVariant &entry : m_controller->albums()) {
        const QVariantMap album = entry.toMap();
        if (album.value(QStringLiteral("title")).toString()
                == QStringLiteral("Moved Album 18")) {
            QCOMPARE(album.value(QStringLiteral("artist")).toString(),
                     QStringLiteral("New Owner 18"));
            QCOMPARE(album.value(QStringLiteral("songCount")).toInt(), 2);
            sawMoved = true;
        } else {
            QCOMPARE(album.value(QStringLiteral("artist")).toString(),
                     QStringLiteral("Old Owner 18"));
            sawKeeper = true;
        }
    }
    QVERIFY(sawMoved);
    QVERIFY(sawKeeper);

    // Every song followed through its own row (the drill-down resolves the
    // artist from songs.artist_id).
    for (const QVariant &entry : m_controller->songs()) {
        const QVariantMap song = entry.toMap();
        const bool onMovedAlbum =
            song.value(QStringLiteral("album")).toString()
            == QStringLiteral("Moved Album 18");
        QCOMPARE(song.value(QStringLiteral("artist")).toString(),
                 onMovedAlbum ? QStringLiteral("New Owner 18")
                              : QStringLiteral("Old Owner 18"));
    }
}

void tst_library::testSetAlbumIdentityAcceptsACombinedTitleAndArtistChange()
{
    // A combined edit must arrive at the backend as ONE call with the
    // arguments in order — (title, artist) -> (newTitle, newArtist). The
    // database regression test owns the full semantics; this one pins the
    // controller passthrough (an argument-order slip would silently edit
    // the wrong album) and the single refresh.
    const qint64 id = insertSongRow(QStringLiteral("/music/tagged/combined19.mp3"),
                                    QStringLiteral("Combined Song 19"));
    QVERIFY(id > 0);
    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("artist")] = QStringLiteral("Combined Artist 19");
    map[QStringLiteral("album")] = QStringLiteral("Combined Album 19");
    QVERIFY(m_controller->updateSong(map));

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(m_controller->setAlbumIdentity(QStringLiteral("Combined Album 19"),
                                           QStringLiteral("Combined Artist 19"),
                                           QStringLiteral("Renamed Album 19"),
                                           QStringLiteral("New Artist 19")));
    QCOMPARE(changedSpy.count(), 1);

    const QVariantMap album = m_controller->albums().first().toMap();
    QCOMPARE(album.value(QStringLiteral("title")).toString(),
             QStringLiteral("Renamed Album 19"));
    QCOMPARE(album.value(QStringLiteral("artist")).toString(),
             QStringLiteral("New Artist 19"));
    const QVariantMap song = m_controller->songs().first().toMap();
    QCOMPARE(song.value(QStringLiteral("album")).toString(),
             QStringLiteral("Renamed Album 19"));
    QCOMPARE(song.value(QStringLiteral("artist")).toString(),
             QStringLiteral("New Artist 19"));
}

void tst_library::testSetAlbumArtworkEmitsOnceAndCoversEverySong()
{
    const qint64 first = insertSongRow(QStringLiteral("/music/tagged/cover18a.mp3"),
                                       QStringLiteral("Cover Song 18A"));
    const qint64 second = insertSongRow(QStringLiteral("/music/tagged/cover18b.mp3"),
                                        QStringLiteral("Cover Song 18B"));
    QVERIFY(first > 0 && second > 0);
    const QList<qint64> ids = {first, second};
    for (const qint64 id : ids) {
        QVariantMap map;
        map[QStringLiteral("id")] = id;
        map[QStringLiteral("artist")] = QStringLiteral("Cover Artist 18");
        map[QStringLiteral("album")] = QStringLiteral("Cover Album 18");
        QVERIFY(m_controller->updateSong(map));
    }

    // A real (tiny) PNG standing in for the user's picked cover.
    const QString pickedPath = m_tempDir.filePath(QStringLiteral("album_cover18.png"));
    {
        QFile picked(pickedPath);
        QVERIFY(picked.open(QIODevice::WriteOnly));
        QVERIFY(picked.write(QByteArray::fromHex(TestHelpers::kRedPngHex)) > 0);
        picked.close();
    }

    QSignalSpy changedSpy(m_controller, &LibraryController::libraryChanged);
    QVERIFY(m_controller->setAlbumArtwork(QStringLiteral("Cover Album 18"),
                                          QStringLiteral("Cover Artist 18"),
                                          pickedPath));
    QCOMPARE(changedSpy.count(), 1);

    // Every song of the album points at the same cached copy, never at the
    // user's original file.
    QString cached;
    for (const QVariant &entry : m_controller->songs()) {
        const QString artwork = entry.toMap()
                                    .value(QStringLiteral("artwork")).toString();
        QVERIFY(!artwork.isEmpty());
        QVERIFY(QFile::exists(artwork));
        QVERIFY(artwork != pickedPath);
        if (cached.isEmpty())
            cached = artwork;
        else
            QCOMPARE(artwork, cached);
    }
    // The album card shows the same cover.
    QCOMPARE(m_controller->albums().first().toMap()
                 .value(QStringLiteral("artwork")).toString(), cached);

    // An empty path clears the cover for the album.
    QVERIFY(m_controller->setAlbumArtwork(QStringLiteral("Cover Album 18"),
                                          QStringLiteral("Cover Artist 18"),
                                          QString()));
    QCOMPARE(changedSpy.count(), 2);
    for (const QVariant &entry : m_controller->songs()) {
        QVERIFY(entry.toMap()
                    .value(QStringLiteral("artwork")).toString().isEmpty());
    }
    QVERIFY(m_controller->albums().first().toMap()
                .value(QStringLiteral("artwork")).toString().isEmpty());

    // Unknown album: refused without touching the library.
    QVERIFY(!m_controller->setAlbumArtwork(QStringLiteral("No Such Album 18"),
                                           QStringLiteral("Cover Artist 18"),
                                           pickedPath));
    QCOMPARE(changedSpy.count(), 2);
}

QTEST_MAIN(tst_library)
#include "tst_library.moc"
