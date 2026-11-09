#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlQuery>
#include <QSignalSpy>

#include "../../src/library/LibraryController.h"
#include "../TestHelpers.h"

#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/tbytevector.h>
#include <taglib/tstring.h>

/**
 * @brief Integration test for the library scan workflow.
 *
 * Covers the end-to-end flow:
 *   1. Start with an empty library.
 *   2. Configure a music directory.
 *   3. Scan it.
 *   4. Verify the songs were ingested.
 *   5. Destroy the controller (simulates an application restart).
 *   6. Verify the library persisted in the database.
 */
class tst_library_scan : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testFullScanWorkflow();
    void testPersistenceAcrossRestart();
    void testPersonalScanFilterAndDedupe();
    void testRescanOnlyProcessesChangedFiles();
    void testScanExtractsEmbeddedArtwork();
    void testEditSurvivesRescan();
    void testScanForgetsDeletedFiles();
    void testScanRemovesMissingSongsOutsideTheScannedRoot();
    void testScanRemovesSongsWhenTheirFolderIsGone();
    void testScanKeepsHiddenSongs();

private:
    QVariantMap songByPath(LibraryController *controller, const QString &path) const;
    // Write ID3v2 title/artist/album tags to a real MPEG file (the
    // file-rewrite half of the edit-vs-rescan story).
    bool tagMpegFile(const QString &path, const QString &title,
                     const QString &artist, const QString &album);

    QTemporaryDir m_tempDir;
};

void tst_library_scan::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("tst_library_scan"));
    QVERIFY(m_tempDir.isValid());
    TestHelpers::removeDatabaseFiles();
}

void tst_library_scan::cleanupTestCase()
{
}

void tst_library_scan::testFullScanWorkflow()
{
    // Step 1-2: create a music directory with audio files
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 5);
    QCOMPARE(files.size(), 5);

    LibraryController controller;

    // Step 3: scan (asynchronous)
    controller.scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(&controller));

    // Step 4: songs were ingested
    QCOMPARE(controller.songCount(), 5);
    QCOMPARE(controller.songs().size(), 5);

    // Every ingested song must carry its path
    const QVector<QVariant> songs = controller.songs();
    for (const QVariant &entry : songs) {
        const QVariantMap map = entry.toMap();
        QVERIFY(!map.value(QStringLiteral("path")).toString().isEmpty());
    }

    // Non-audio files in the same tree must not be ingested
    QFile txtFile(m_tempDir.filePath(QStringLiteral("notes.txt")));
    QVERIFY(txtFile.open(QIODevice::WriteOnly));
    txtFile.write("not music");
    txtFile.close();

    controller.scan(m_tempDir.path());
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 5);
}

void tst_library_scan::testPersistenceAcrossRestart()
{
    // Start from a fresh database so only this session's songs count
    TestHelpers::removeDatabaseFiles();

    // Simulated first session: scan a fresh directory
    const QString restartDir = m_tempDir.filePath(QStringLiteral("restart_music"));
    const QStringList files = TestHelpers::createTestMusicDirectory(restartDir, 3);
    QCOMPARE(files.size(), 3);

    {
        LibraryController firstSession;
        firstSession.scan(restartDir);
        QVERIFY(TestHelpers::waitForScanFinished(&firstSession));
        QCOMPARE(firstSession.songCount(), 3);
    } // firstSession destroyed -> application restart

    // Simulated second session: a new controller must see the same data
    // after refreshing from the database.
    LibraryController secondSession;
    secondSession.refresh();
    QCOMPARE(secondSession.songCount(), 3);
}

QVariantMap tst_library_scan::songByPath(LibraryController *controller,
                                         const QString &path) const
{
    const QVector<QVariant> songs = controller->songs();
    for (const QVariant &entry : songs) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("path")).toString() == path)
            return map;
    }
    return {};
}

void tst_library_scan::testPersonalScanFilterAndDedupe()
{
    TestHelpers::removeDatabaseFiles();

    const QString dir = m_tempDir.filePath(QStringLiteral("duration_filter"));
    QDir().mkpath(dir);

    // A short clip (~1 s) and a real track (~31 s).
    const QString shortPath = dir + QStringLiteral("/clip.mp3");
    const QString longPath = dir + QStringLiteral("/full.mp3");
    QVERIFY(TestHelpers::createMpegAudioFile(shortPath, 40));
    QVERIFY(TestHelpers::createMpegAudioFile(longPath, 1200));

    LibraryController controller;

    // startScan(dir, 30000) mimics the personal-folder auto-scan: only
    // audio longer than 30 s passes the filter.
    controller.startScan(dir, 30000);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 1);
    QCOMPARE(controller.songs().at(0).toMap().value(QStringLiteral("path")).toString(),
             longPath);

    // A normal scan also ingests the short clip.
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    // Re-scanning never duplicates previously added songs (dedupe by path).
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);
}

void tst_library_scan::testRescanOnlyProcessesChangedFiles()
{
    TestHelpers::removeDatabaseFiles();

    const QString dir = m_tempDir.filePath(QStringLiteral("rescan_music"));
    QDir().mkpath(dir);

    // Real MPEG files with known, different durations: rewriting a file with
    // a different frame count changes both its size and its duration.
    const QString changedPath = dir + QStringLiteral("/changed.mp3");
    const QString stablePath = dir + QStringLiteral("/stable.mp3");
    QVERIFY(TestHelpers::createMpegAudioFile(changedPath, 400)); // ~10.4 s
    QVERIFY(TestHelpers::createMpegAudioFile(stablePath, 800));  // ~20.9 s

    LibraryController controller;

    // First scan: both files are new and get ingested.
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    const QVariantMap changedBefore = songByPath(&controller, changedPath);
    QVERIFY(!changedBefore.isEmpty());
    const qint64 durationBefore =
        changedBefore.value(QStringLiteral("duration")).toLongLong();
    QVERIFY(durationBefore > 0);

    // Rescan with nothing changed: no new rows, no duplicates.
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    // Rewrite one file (different frame count => different size/duration and
    // a fresh mtime). The rescan must re-read only that file and refresh its
    // metadata; the unchanged file is left alone.
    QVERIFY(TestHelpers::createMpegAudioFile(changedPath, 1200)); // ~31.3 s

    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    const QVariantMap changedAfter = songByPath(&controller, changedPath);
    QVERIFY(!changedAfter.isEmpty());
    const qint64 durationAfter =
        changedAfter.value(QStringLiteral("duration")).toLongLong();
    QVERIFY(durationAfter != durationBefore);
    QVERIFY(durationAfter > durationBefore);

    // Exactly one row per path: the rescan neither duplicated nor dropped any.
    QVERIFY(!songByPath(&controller, stablePath).isEmpty());
    int changedRows = 0;
    int stableRows = 0;
    for (const QVariant &entry : controller.songs()) {
        const QString path = entry.toMap().value(QStringLiteral("path")).toString();
        if (path == changedPath)
            ++changedRows;
        if (path == stablePath)
            ++stableRows;
    }
    QCOMPARE(changedRows, 1);
    QCOMPARE(stableRows, 1);
}

void tst_library_scan::testScanExtractsEmbeddedArtwork()
{
    TestHelpers::removeDatabaseFiles();

    const QString dir = m_tempDir.filePath(QStringLiteral("artwork_music"));
    QDir().mkpath(dir);
    const QString path = dir + QStringLiteral("/with_cover.mp3");
    QVERIFY(TestHelpers::createMpegAudioFile(path, 40)); // ~1 s

    // Tag the file and embed a front-cover picture.
    const QByteArray art = QByteArray::fromHex(TestHelpers::kRedPngHex);
    {
        TagLib::MPEG::File mf(path.toStdString().c_str());
        QVERIFY(mf.isValid());
        TagLib::ID3v2::Tag *tag = mf.ID3v2Tag(true);
        tag->setTitle(TagLib::String("Covered", TagLib::String::UTF8));
        tag->setArtist(TagLib::String("Artist A", TagLib::String::UTF8));
        tag->setAlbum(TagLib::String("Album X", TagLib::String::UTF8));
        auto *pic = new TagLib::ID3v2::AttachedPictureFrame();
        pic->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
        pic->setMimeType("image/png");
        pic->setPicture(TagLib::ByteVector(art.constData(),
                                           static_cast<unsigned int>(art.size())));
        tag->addFrame(pic);
        QVERIFY(mf.save());
    }

    LibraryController controller;
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 1);

    // The ingested song row carries the extracted artwork path.
    const QVariantMap song = songByPath(&controller, path);
    QVERIFY(!song.isEmpty());
    const QString artwork = song.value(QStringLiteral("artwork")).toString();
    QVERIFY(!artwork.isEmpty());
    QVERIFY(QFile::exists(artwork));

    // The album rollup exposed the same cover for the album card.
    const QVector<QVariant> albums = controller.albums();
    QCOMPARE(albums.size(), 1);
    QCOMPARE(albums.first().toMap().value(QStringLiteral("artwork")).toString(),
             artwork);
}

bool tst_library_scan::tagMpegFile(const QString &path, const QString &title,
                                   const QString &artist, const QString &album)
{
    TagLib::MPEG::File mf(path.toStdString().c_str());
    if (!mf.isValid()) {
        return false;
    }
    TagLib::ID3v2::Tag *tag = mf.ID3v2Tag(true);
    tag->setTitle(TagLib::String(title.toStdString(), TagLib::String::UTF8));
    tag->setArtist(TagLib::String(artist.toStdString(), TagLib::String::UTF8));
    tag->setAlbum(TagLib::String(album.toStdString(), TagLib::String::UTF8));
    return mf.save();
}

// The tag-editing headline: a user edit must survive rescans.
//   (a) an unchanged file is fingerprint-skipped, so the edit trivially
//       survives;
//   (b) a file rewritten with DIFFERENT tags is re-read, but the CASE
//       guard in upsertSong() keeps the edited row's curated columns,
//       while an unedited control song takes its new file tags;
//   (c) the rescan refreshed the fingerprint, so a third scan skips
//       again — no permanent re-read loop (the v3 artwork lesson).
void tst_library_scan::testEditSurvivesRescan()
{
    TestHelpers::removeDatabaseFiles();

    const QString dir = m_tempDir.filePath(QStringLiteral("edit_rescan"));
    QDir().mkpath(dir);
    const QString editedPath = dir + QStringLiteral("/edited.mp3");
    const QString controlPath = dir + QStringLiteral("/control.mp3");
    QVERIFY(TestHelpers::createMpegAudioFile(editedPath, 400));
    QVERIFY(TestHelpers::createMpegAudioFile(controlPath, 400));

    // Real tags before the first scan, so the library holds file-derived
    // values the edit must override.
    QVERIFY(tagMpegFile(editedPath, QStringLiteral("Scan Title"),
                         QStringLiteral("Scan Artist"), QStringLiteral("Scan Album")));
    QVERIFY(tagMpegFile(controlPath, QStringLiteral("Control Scan Title"),
                         QStringLiteral("Control Artist"),
                         QStringLiteral("Control Scan Album")));

    LibraryController controller;
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    const QVariantMap scannedEdited = songByPath(&controller, editedPath);
    QVERIFY(!scannedEdited.isEmpty());
    QCOMPARE(scannedEdited.value(QStringLiteral("title")).toString(),
             QStringLiteral("Scan Title"));

    // The user corrects one song's title and album (a field-level edit
    // through the QML-facing API).
    QVariantMap edit;
    edit[QStringLiteral("id")] = scannedEdited.value(QStringLiteral("id"));
    edit[QStringLiteral("title")] = QStringLiteral("Edited Title");
    edit[QStringLiteral("album")] = QStringLiteral("Edited Album");
    QVERIFY(controller.updateSong(edit));

    // (a) Rescan with unchanged files: the fingerprint check skips them
    // and the edit is intact.
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    {
        const QVariantMap song = songByPath(&controller, editedPath);
        QCOMPARE(song.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Edited Title"));
        QCOMPARE(song.value(QStringLiteral("album")).toString(),
                 QStringLiteral("Edited Album"));
    }

    // (b) Rewrite both files with different tags. The rescan re-reads
    // them (fresh mtime/size), but the edited row keeps the user's
    // values while the unedited control row takes its new file tags.
    QVERIFY(tagMpegFile(editedPath, QStringLiteral("Retagged Title"),
                         QStringLiteral("Scan Artist"),
                         QStringLiteral("Retagged Album")));
    QVERIFY(tagMpegFile(controlPath, QStringLiteral("Control Retagged Title"),
                         QStringLiteral("Control Artist"),
                         QStringLiteral("Control Retagged Album")));

    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    {
        const QVariantMap editedSong = songByPath(&controller, editedPath);
        QCOMPARE(editedSong.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Edited Title"));
        QCOMPARE(editedSong.value(QStringLiteral("album")).toString(),
                 QStringLiteral("Edited Album"));

        const QVariantMap controlSong = songByPath(&controller, controlPath);
        QCOMPARE(controlSong.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Control Retagged Title"));
        QCOMPARE(controlSong.value(QStringLiteral("album")).toString(),
                 QStringLiteral("Control Retagged Album"));
    }

    // The rescan refreshed the edited file's fingerprint even though its
    // tag columns stayed frozen: the stored mtime/size match the file
    // again, so the next scan skips it — no permanent re-read loop.
    {
        QSqlQuery query;
        query.prepare(QStringLiteral("SELECT mtime, size FROM songs WHERE path = ?"));
        query.addBindValue(editedPath);
        QVERIFY(query.exec());
        QVERIFY(query.next());
        const QFileInfo info(editedPath);
        QCOMPARE(query.value(0).toLongLong(),
                 info.lastModified().toMSecsSinceEpoch());
        QCOMPARE(query.value(1).toLongLong(), info.size());
    }

    // (c) Third scan, nothing changed: every value sits exactly where
    // (b) left it.
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);
    {
        const QVariantMap editedSong = songByPath(&controller, editedPath);
        QCOMPARE(editedSong.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Edited Title"));
        QCOMPARE(editedSong.value(QStringLiteral("album")).toString(),
                 QStringLiteral("Edited Album"));
        const QVariantMap controlSong = songByPath(&controller, controlPath);
        QCOMPARE(controlSong.value(QStringLiteral("title")).toString(),
                 QStringLiteral("Control Retagged Title"));
    }
}

// A scan is also a reconciliation: a file that disappeared behind the app's
// back (deleted in a file manager or by another program) must not stay in the
// library as a phantom row.
void tst_library_scan::testScanForgetsDeletedFiles()
{
    TestHelpers::removeDatabaseFiles();

    const QString dir = m_tempDir.filePath(QStringLiteral("prune_music"));
    QDir().mkpath(dir);
    const QStringList files = TestHelpers::createTestMusicDirectory(dir, 3);
    QCOMPARE(files.size(), 3);

    LibraryController controller;
    // scanFinished() carries how many rows were dropped, so the UI can say
    // it instead of the removal happening silently.
    QSignalSpy removedSpy(&controller, &LibraryController::scanFinished);

    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 3);
    // First scan only ingests: nothing was dropped, and it says so.
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.first().size(), 1);
    QCOMPARE(removedSpy.first().at(0).toInt(), 0);

    // One file is removed outside the application.
    QVERIFY(QFile::remove(files.at(1)));
    QVERIFY(!QFile::exists(files.at(1)));

    removedSpy.clear();
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));

    // The scan reports exactly the one row it dropped.
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.first().size(), 1);
    QCOMPARE(removedSpy.first().at(0).toInt(), 1);

    // The row is gone, not merely filtered: the other two survive and no
    // query returns the deleted path any more.
    QCOMPARE(controller.songCount(), 2);
    QVERIFY(songByPath(&controller, files.at(1)).isEmpty());
    QVERIFY(!songByPath(&controller, files.at(0)).isEmpty());
    QVERIFY(!songByPath(&controller, files.at(2)).isEmpty());

    // ...and it is gone from the database too, so a restart does not bring it
    // back.
    LibraryController reopened;
    reopened.refresh();
    QCOMPARE(reopened.songCount(), 2);
    QVERIFY(songByPath(&reopened, files.at(1)).isEmpty());
}

// Reconciliation covers the whole library, not only the folder being walked:
// a row whose file disappeared must not survive just because another folder
// was scanned. This is what cleans up stale paths after reorganizing music.
void tst_library_scan::testScanRemovesMissingSongsOutsideTheScannedRoot()
{
    TestHelpers::removeDatabaseFiles();

    const QString dirA = m_tempDir.filePath(QStringLiteral("scope_a"));
    const QString dirB = m_tempDir.filePath(QStringLiteral("scope_b"));
    QDir().mkpath(dirA);
    QDir().mkpath(dirB);
    QCOMPARE(TestHelpers::createTestMusicDirectory(dirA, 2).size(), 2);
    const QStringList inB = TestHelpers::createTestMusicDirectory(dirB, 1);
    QCOMPARE(inB.size(), 1);

    LibraryController controller;
    QSignalSpy removedSpy(&controller, &LibraryController::scanFinished);
    controller.scan(dirA);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    controller.scan(dirB);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 3);

    // B's only file disappears. Scanning A still walks only A, but the
    // reconciliation checks every stored path and drops B's phantom row.
    QVERIFY(QFile::remove(inB.first()));

    removedSpy.clear();
    controller.scan(dirA);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);
    QVERIFY(songByPath(&controller, inB.first()).isEmpty());
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.first().at(0).toInt(), 1);

    // The row is gone from the database too, so a restart does not bring it
    // back.
    LibraryController reopened;
    reopened.refresh();
    QCOMPARE(reopened.songCount(), 2);
    QVERIFY(songByPath(&reopened, inB.first()).isEmpty());
}

// A folder that disappears (deleted, or moved as part of a reorganization)
// must not leave phantom rows behind. The parent of those paths is gone too,
// which is exactly the case the old root-scoped guard refused to prune; now
// the scan reconciles them away.
void tst_library_scan::testScanRemovesSongsWhenTheirFolderIsGone()
{
    TestHelpers::removeDatabaseFiles();

    const QString root = m_tempDir.filePath(QStringLiteral("volume"));
    QDir().mkpath(root);
    const QStringList inRoot = TestHelpers::createTestMusicDirectory(root, 1);
    QCOMPARE(inRoot.size(), 1);
    const QString removable = root + QStringLiteral("/removable");
    QDir().mkpath(removable);
    const QStringList onVolume = TestHelpers::createTestMusicDirectory(removable, 2);
    QCOMPARE(onVolume.size(), 2);

    LibraryController controller;
    QSignalSpy removedSpy(&controller, &LibraryController::scanFinished);
    controller.scan(root);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 3);

    // The whole folder disappears, as if it were deleted or moved away.
    QVERIFY(QDir(removable).removeRecursively());

    removedSpy.clear();
    controller.scan(root);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    // Only the song still on disk survives; the two rows under the vanished
    // folder are pruned.
    QCOMPARE(controller.songCount(), 1);
    QVERIFY(!songByPath(&controller, inRoot.first()).isEmpty());
    QVERIFY(songByPath(&controller, onVolume.at(0)).isEmpty());
    QVERIFY(songByPath(&controller, onVolume.at(1)).isEmpty());
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.first().at(0).toInt(), 2);

    // A scan of a root that does not exist proves nothing and must not prune
    // the remaining library.
    controller.scan(removable);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 1);
    QVERIFY(!songByPath(&controller, inRoot.first()).isEmpty());
}

// Hiding a song is not deleting it: the file is still there, so the scan
// keeps its row (the walk merely skips it).
void tst_library_scan::testScanKeepsHiddenSongs()
{
    TestHelpers::removeDatabaseFiles();

    const QString dir = m_tempDir.filePath(QStringLiteral("prune_hidden"));
    QDir().mkpath(dir);
    const QStringList files = TestHelpers::createTestMusicDirectory(dir, 2);
    QCOMPARE(files.size(), 2);

    LibraryController controller;
    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    const QString hidden = files.first();
    controller.ignorePath(hidden);
    QCOMPARE(controller.songCount(), 1);

    controller.scan(dir);
    QVERIFY(TestHelpers::waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 1);

    // Unhiding proves the row was never pruned.
    controller.unignorePath(hidden);
    QCOMPARE(controller.songCount(), 2);
    QVERIFY(!songByPath(&controller, hidden).isEmpty());
}

QTEST_MAIN(tst_library_scan)
#include "tst_library_scan.moc"
