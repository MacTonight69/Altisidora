#include <QtTest>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QHash>

#include "../mocks/MockDatabase.h"

/**
 * @brief Tests for database operations
 *
 * Tests CRUD operations, transactions, and error handling
 * for the database module using a mock database.
 */
class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // Connection tests
    void testDatabaseConnection();
    void testDatabaseConnectionFailure();

    // CRUD tests
    void testInsertSong();
    void testInsertMultipleSongs();
    void testGetAllSongs();
    void testGetSongsByArtist();
    void testGetSongsByAlbum();
    void testDeleteSong();
    void testDeleteNonExistentSong();

    // Transaction tests
    void testTransactionCommit();
    void testTransactionRollback();

    // Error handling tests
    void testInvalidQuery();
    void testDuplicateInsert();

    // Data integrity tests
    void testSongCount();
    void testClearDatabase();

private:
    MockDatabase *m_db;
};

void TestDatabase::initTestCase()
{
    m_db = new MockDatabase(this);
    QVERIFY(m_db->initialize());
}

void TestDatabase::cleanupTestCase()
{
    delete m_db;
}

void TestDatabase::testDatabaseConnection()
{
    QSqlDatabase db = m_db->database();
    QVERIFY(db.isOpen());
    QVERIFY(db.isValid());
}

void TestDatabase::testDatabaseConnectionFailure()
{
    // Test that an invalid database path fails gracefully
    QSqlDatabase badDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("bad_connection"));
    badDb.setDatabaseName(QStringLiteral("/nonexistent/path/to/db.sqlite"));

    // This should fail to open
    bool opened = badDb.open();
    QVERIFY(!opened);

    QSqlDatabase::removeDatabase(QStringLiteral("bad_connection"));
}

void TestDatabase::testInsertSong()
{
    QHash<QString, QVariant> song;
    song[QStringLiteral("path")] = QStringLiteral("/music/test.mp3");
    song[QStringLiteral("title")] = QStringLiteral("Test Song");
    song[QStringLiteral("artist")] = QStringLiteral("Test Artist");
    song[QStringLiteral("album")] = QStringLiteral("Test Album");
    song[QStringLiteral("genre")] = QStringLiteral("Rock");
    song[QStringLiteral("year")] = 2024;
    song[QStringLiteral("track_number")] = 1;
    song[QStringLiteral("duration")] = 180000;

    QVERIFY(m_db->insertSong(song));
    QCOMPARE(m_db->songCount(), 1);
}

void TestDatabase::testInsertMultipleSongs()
{
    for (int i = 0; i < 5; ++i) {
        QHash<QString, QVariant> song;
        song[QStringLiteral("path")] = QStringLiteral("/music/song%1.mp3").arg(i);
        song[QStringLiteral("title")] = QStringLiteral("Song %1").arg(i);
        song[QStringLiteral("artist")] = QStringLiteral("Artist %1").arg(i % 2);
        song[QStringLiteral("album")] = QStringLiteral("Album %1").arg(i % 3);

        QVERIFY(m_db->insertSong(song));
    }

    QCOMPARE(m_db->songCount(), 6); // 5 new + 1 from previous test
}

void TestDatabase::testGetAllSongs()
{
    auto songs = m_db->getAllSongs();
    QVERIFY(songs.size() >= 5);

    // Verify that each song has required fields
    for (const auto &song : songs) {
        QVERIFY(song.contains(QStringLiteral("id")));
        QVERIFY(song.contains(QStringLiteral("path")));
        QVERIFY(song.contains(QStringLiteral("title")));
    }
}

void TestDatabase::testGetSongsByArtist()
{
    auto songs = m_db->getSongsByArtist(QStringLiteral("Artist 0"));
    QVERIFY(songs.size() >= 1);

    for (const auto &song : songs) {
        QCOMPARE(song[QStringLiteral("artist")].toString(), QStringLiteral("Artist 0"));
    }
}

void TestDatabase::testGetSongsByAlbum()
{
    auto songs = m_db->getSongsByAlbum(QStringLiteral("Album 0"));
    QVERIFY(songs.size() >= 1);

    for (const auto &song : songs) {
        QCOMPARE(song[QStringLiteral("album")].toString(), QStringLiteral("Album 0"));
    }
}

void TestDatabase::testDeleteSong()
{
    auto songs = m_db->getAllSongs();
    QVERIFY(!songs.isEmpty());

    int idToDelete = songs.first()[QStringLiteral("id")].toInt();
    int countBefore = m_db->songCount();

    QVERIFY(m_db->deleteSong(idToDelete));
    QCOMPARE(m_db->songCount(), countBefore - 1);
}

void TestDatabase::testDeleteNonExistentSong()
{
    int countBefore = m_db->songCount();
    QVERIFY(m_db->deleteSong(99999));
    QCOMPARE(m_db->songCount(), countBefore);
}

void TestDatabase::testTransactionCommit()
{
    QVERIFY(m_db->beginTransaction());

    QHash<QString, QVariant> song;
    song[QStringLiteral("path")] = QStringLiteral("/music/transaction.mp3");
    song[QStringLiteral("title")] = QStringLiteral("Transaction Song");
    song[QStringLiteral("artist")] = QStringLiteral("Transaction Artist");

    QVERIFY(m_db->insertSong(song));
    QVERIFY(m_db->commit());

    // Verify the song was committed
    auto songs = m_db->getSongsByArtist(QStringLiteral("Transaction Artist"));
    QCOMPARE(songs.size(), 1);
}

void TestDatabase::testTransactionRollback()
{
    int countBefore = m_db->songCount();

    QVERIFY(m_db->beginTransaction());

    QHash<QString, QVariant> song;
    song[QStringLiteral("path")] = QStringLiteral("/music/rollback.mp3");
    song[QStringLiteral("title")] = QStringLiteral("Rollback Song");
    song[QStringLiteral("artist")] = QStringLiteral("Rollback Artist");

    QVERIFY(m_db->insertSong(song));
    QVERIFY(m_db->rollback());

    // Verify the song was rolled back
    QCOMPARE(m_db->songCount(), countBefore);
}

void TestDatabase::testInvalidQuery()
{
    QList<QHash<QString, QVariant>> results = m_db->query(QStringLiteral("SELECT * FROM nonexistent_table"));
    QVERIFY(results.isEmpty());
    QVERIFY(m_db->lastError().isValid());
}

void TestDatabase::testDuplicateInsert()
{
    // Insert a song
    QHash<QString, QVariant> song;
    song[QStringLiteral("path")] = QStringLiteral("/music/duplicate.mp3");
    song[QStringLiteral("title")] = QStringLiteral("Duplicate Song");
    song[QStringLiteral("artist")] = QStringLiteral("Duplicate Artist");

    QVERIFY(m_db->insertSong(song));

    // Insert the same song again (should succeed - no unique constraint on path)
    QVERIFY(m_db->insertSong(song));

    auto songs = m_db->getSongsByArtist(QStringLiteral("Duplicate Artist"));
    QCOMPARE(songs.size(), 2);
}

void TestDatabase::testSongCount()
{
    int count = m_db->songCount();
    QVERIFY(count > 0);

    // Insert one more
    QHash<QString, QVariant> song;
    song[QStringLiteral("path")] = QStringLiteral("/music/count.mp3");
    song[QStringLiteral("title")] = QStringLiteral("Count Song");
    song[QStringLiteral("artist")] = QStringLiteral("Count Artist");

    QVERIFY(m_db->insertSong(song));
    QCOMPARE(m_db->songCount(), count + 1);
}

void TestDatabase::testClearDatabase()
{
    m_db->clear();
    QCOMPARE(m_db->songCount(), 0);

    auto songs = m_db->getAllSongs();
    QVERIFY(songs.isEmpty());
}

QTEST_MAIN(TestDatabase)
#include "tst_database.moc"
