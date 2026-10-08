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
    void testScanSignals();
    void testIgnorePathHidesSong();
    void testIgnoreFolderHidesChildren();
    void testUnignoreRestoresSongs();
    void testRescanSkipsIgnored();

private:
    void removeDatabaseFiles();
    void clearTempDir();
    bool waitForScanFinished();

    LibraryController *m_controller = nullptr;
    QTemporaryDir m_tempDir;
};

void tst_library::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("tst_library"));

    QVERIFY(m_tempDir.isValid());
    removeDatabaseFiles();
    m_controller = new LibraryController(this);
}

void tst_library::cleanupTestCase()
{
    delete m_controller;
    m_controller = nullptr;
}

void tst_library::removeDatabaseFiles()
{
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);

    const QStringList suffixes = {
        QString(),
        QStringLiteral("-journal"),
        QStringLiteral("-wal"),
        QStringLiteral("-shm")
    };
    for (const QString &suffix : suffixes) {
        QFile::remove(dataPath + QStringLiteral("/library.db") + suffix);
    }
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
    QVERIFY(waitForScanFinished());

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
    QVERIFY(waitForScanFinished());

    QCOMPARE(m_controller->songCount(), 0);
}

void tst_library::testRescanDoesNotDuplicate()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 3);

    m_controller->scan(m_tempDir.path());
    QVERIFY(waitForScanFinished());
    QCOMPARE(m_controller->songCount(), 3);

    // Scanning the same directory again must not create duplicates
    m_controller->scan(m_tempDir.path());
    QVERIFY(waitForScanFinished());
    QCOMPARE(m_controller->songCount(), 3);
}

void tst_library::testSearch()
{
    TestHelpers::createTestMusicDirectory(m_tempDir.path(), 4);
    m_controller->scan(m_tempDir.path());
    QVERIFY(waitForScanFinished());
    QCOMPARE(m_controller->songCount(), 4);

    // File names follow the pattern trackNN_Artist_Album.ext
    const QVector<QVariant> results = m_controller->search(QStringLiteral("track01"));
    QCOMPARE(results.size(), 1);

    // A query matching nothing must return an empty list
    QVERIFY(m_controller->search(QStringLiteral("zzz_no_such_song")).isEmpty());
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

bool tst_library::waitForScanFinished()
{
    // The scan already started; waiting on the spy lets the event loop
    // deliver the queued scanFinished() from the worker thread.
    QSignalSpy finishedSpy(m_controller, &LibraryController::scanFinished);
    return finishedSpy.wait(10000);
}

void tst_library::testIgnorePathHidesSong()
{
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 4);
    m_controller->scan(m_tempDir.path());
    QVERIFY(waitForScanFinished());
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
    QVERIFY(waitForScanFinished());
    QCOMPARE(m_controller->songCount(), 5);

    // Hiding the folder hides all the songs inside it at once.
    m_controller->ignorePath(subDir.path());
    QCOMPARE(m_controller->songCount(), 2);
}

void tst_library::testUnignoreRestoresSongs()
{
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 3);
    m_controller->scan(m_tempDir.path());
    QVERIFY(waitForScanFinished());

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
    QVERIFY(waitForScanFinished());
    QCOMPARE(m_controller->songCount(), 4);

    // Ignore the folder, then rescan: ignored songs are not re-added and
    // the scanner does not even walk into the hidden folder.
    m_controller->ignorePath(subDir.path());
    QCOMPARE(m_controller->songCount(), 2);

    m_controller->scan(m_tempDir.path());
    QVERIFY(waitForScanFinished());
    QCOMPARE(m_controller->songCount(), 2);
    QVERIFY(!hidden.isEmpty());
}

QTEST_MAIN(tst_library)
#include "tst_library.moc"
