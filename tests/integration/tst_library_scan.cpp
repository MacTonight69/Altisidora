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

private:
    void removeDatabaseFiles();
    bool waitForScanFinished(LibraryController *controller);

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

QTEST_MAIN(tst_library_scan)
#include "tst_library_scan.moc"
