#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QVariant>
#include <QHash>
#include <QList>

/**
 * @brief Mock database for testing
 *
 * Provides an in-memory database that can be used to test
 * database-dependent components without requiring a real database.
 */
class MockDatabase : public QObject
{
    Q_OBJECT

public:
    explicit MockDatabase(QObject *parent = nullptr);
    ~MockDatabase() override;

    /**
     * @brief Initialize the mock database with a schema
     * @return true if initialization succeeded
     */
    bool initialize();

    /**
     * @brief Execute a query
     * @param query SQL query string
     * @return true if query executed successfully
     */
    bool execute(const QString &query);

    /**
     * @brief Execute a query and return results
     * @param query SQL query string
     * @return List of result rows (each row is a hash of column name to value)
     */
    QList<QHash<QString, QVariant>> query(const QString &queryStr);

    /**
     * @brief Get the last error
     * @return Last SQL error
     */
    QSqlError lastError() const;

    /**
     * @brief Get the underlying database connection
     * @return QSqlDatabase instance
     */
    QSqlDatabase database() const;

    /**
     * @brief Clear all data from the mock database
     */
    void clear();

    /**
     * @brief Insert a song record
     * @param songData Hash of column names to values
     * @return true if insertion succeeded
     */
    bool insertSong(const QHash<QString, QVariant> &songData);

    /**
     * @brief Get all songs
     * @return List of song records
     */
    QList<QHash<QString, QVariant>> getAllSongs();

    /**
     * @brief Get songs by artist
     * @param artist Artist name
     * @return List of matching song records
     */
    QList<QHash<QString, QVariant>> getSongsByArtist(const QString &artist);

    /**
     * @brief Get songs by album
     * @param album Album name
     * @return List of matching song records
     */
    QList<QHash<QString, QVariant>> getSongsByAlbum(const QString &album);

    /**
     * @brief Delete a song by ID
     * @param id Song ID
     * @return true if deletion succeeded
     */
    bool deleteSong(int id);

    /**
     * @brief Get the count of songs
     * @return Number of songs in the database
     */
    int songCount() const;

    /**
     * @brief Begin a transaction
     * @return true if transaction started
     */
    bool beginTransaction();

    /**
     * @brief Commit the current transaction
     * @return true if commit succeeded
     */
    bool commit();

    /**
     * @brief Rollback the current transaction
     * @return true if rollback succeeded
     */
    bool rollback();

private:
    QSqlDatabase m_db;
    QSqlError m_lastError;
    int m_songCount;
};
