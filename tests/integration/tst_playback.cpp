#include <QtTest>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>
#include <QDir>

#include "../TestHelpers.h"
#include "library/LibraryController.h"
#include "audio/PlayerController.h"
#include "audio/PlaybackState.h"

/**
 * @brief Integration test for the playback data flow.
 *
 * Exercises the end-to-end pipeline: real folder scan -> real SQLite
 * persistence via LibraryController -> PlayerController.loadSong() with
 * the library metadata, without depending on an actual audio backend.
 * Actual audio playback requires a media backend and is out of CI scope.
 */
class tst_playback : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testScanLibraryAndLoadSongIntoPlayer();

private:
    QTemporaryDir m_musicDir;
};

void tst_playback::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("tst_playback"));
    QVERIFY(m_musicDir.isValid());
    TestHelpers::removeDatabaseFiles();
}

void tst_playback::cleanupTestCase()
{
    QStandardPaths::setTestModeEnabled(false);
}

void tst_playback::testScanLibraryAndLoadSongIntoPlayer()
{
    // 1. Create a folder with audio files.
    const QStringList files = TestHelpers::createTestMusicDirectory(m_musicDir.path(), 2);
    QCOMPARE(files.size(), 2);

    // 2. Scan it with the real LibraryController (Scanner + TagLib + SQLite).
    LibraryController library;
    library.scan(m_musicDir.path());

    // Scanning is asynchronous; wait for the worker to finish.
    QSignalSpy scanSpy(&library, &LibraryController::scanFinished);
    QVERIFY(scanSpy.wait(10000));

    const auto songs = library.songs();
    QCOMPARE(songs.size(), 2);

    // Re-scanning must not duplicate entries.
    library.scan(m_musicDir.path());
    QVERIFY(scanSpy.wait(10000));
    QCOMPARE(library.songs().size(), 2);

    // 3. Every song exposed to QML must carry a usable path.
    const QVariantMap first = songs.first().toMap();
    const QString firstPath = first.value(QStringLiteral("path")).toString();
    QVERIFY(!firstPath.isEmpty());
    QVERIFY(QFile::exists(firstPath));

    // 4. Load the song into the real PlayerController keeping metadata.
    PlayerController player;
    QSignalSpy songSpy(&player, &PlayerController::currentSongChanged);

    player.loadSong(first);

    QCOMPARE(player.playbackState(), PlaybackState::Loading);
    QVERIFY(songSpy.count() >= 1);
    const QVariantMap current = player.currentSong();
    QCOMPARE(current.value(QStringLiteral("path")).toString(), firstPath);
    QCOMPARE(current.value(QStringLiteral("title")).toString(),
             first.value(QStringLiteral("title")).toString());
}

QTEST_MAIN(tst_playback)
#include "tst_playback.moc"