#include <QtTest>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSqlQuery>
#include <QSignalSpy>

#include "../../src/library/LibraryController.h"
#include "../TestHelpers.h"

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

private:
    void removeDatabaseFiles();
    bool waitForScanFinished(LibraryController *controller);
    QVariantMap songByPath(LibraryController *controller, const QString &path) const;

    QTemporaryDir m_tempDir;
};

void tst_library_scan::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("tst_library_scan"));
    QVERIFY(m_tempDir.isValid());
    removeDatabaseFiles();
}

void tst_library_scan::cleanupTestCase()
{
}

void tst_library_scan::removeDatabaseFiles()
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

void tst_library_scan::testFullScanWorkflow()
{
    // Step 1-2: create a music directory with audio files
    const QStringList files = TestHelpers::createTestMusicDirectory(m_tempDir.path(), 5);
    QCOMPARE(files.size(), 5);

    LibraryController controller;

    // Step 3: scan (asynchronous)
    controller.scan(m_tempDir.path());
    QVERIFY(waitForScanFinished(&controller));

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
    QVERIFY(waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 5);
}

void tst_library_scan::testPersistenceAcrossRestart()
{
    // Start from a fresh database so only this session's songs count
    removeDatabaseFiles();

    // Simulated first session: scan a fresh directory
    const QString restartDir = m_tempDir.filePath(QStringLiteral("restart_music"));
    const QStringList files = TestHelpers::createTestMusicDirectory(restartDir, 3);
    QCOMPARE(files.size(), 3);

    {
        LibraryController firstSession;
        firstSession.scan(restartDir);
        QVERIFY(waitForScanFinished(&firstSession));
        QCOMPARE(firstSession.songCount(), 3);
    } // firstSession destroyed -> application restart

    // Simulated second session: a new controller must see the same data
    // after refreshing from the database.
    LibraryController secondSession;
    secondSession.refresh();
    QCOMPARE(secondSession.songCount(), 3);
}

bool tst_library_scan::waitForScanFinished(LibraryController *controller)
{
    QSignalSpy finishedSpy(controller, &LibraryController::scanFinished);
    return finishedSpy.wait(10000);
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
    removeDatabaseFiles();

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
    QVERIFY(waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 1);
    QCOMPARE(controller.songs().at(0).toMap().value(QStringLiteral("path")).toString(),
             longPath);

    // A normal scan also ingests the short clip.
    controller.scan(dir);
    QVERIFY(waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    // Re-scanning never duplicates previously added songs (dedupe by path).
    controller.scan(dir);
    QVERIFY(waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);
}

void tst_library_scan::testRescanOnlyProcessesChangedFiles()
{
    removeDatabaseFiles();

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
    QVERIFY(waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    const QVariantMap changedBefore = songByPath(&controller, changedPath);
    QVERIFY(!changedBefore.isEmpty());
    const qint64 durationBefore =
        changedBefore.value(QStringLiteral("duration")).toLongLong();
    QVERIFY(durationBefore > 0);

    // Rescan with nothing changed: no new rows, no duplicates.
    controller.scan(dir);
    QVERIFY(waitForScanFinished(&controller));
    QCOMPARE(controller.songCount(), 2);

    // Rewrite one file (different frame count => different size/duration and
    // a fresh mtime). The rescan must re-read only that file and refresh its
    // metadata; the unchanged file is left alone.
    QVERIFY(TestHelpers::createMpegAudioFile(changedPath, 1200)); // ~31.3 s

    controller.scan(dir);
    QVERIFY(waitForScanFinished(&controller));
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

QTEST_MAIN(tst_library_scan)
#include "tst_library_scan.moc"
