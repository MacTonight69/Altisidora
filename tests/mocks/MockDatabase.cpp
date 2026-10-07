#include "MockDatabase.h"

MockDatabase::MockDatabase(QObject *parent)
    : QObject(parent)
    , m_songCount(0)
{
}

MockDatabase::~MockDatabase()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool MockDatabase::initialize()
{
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    m_db.setDatabaseName(QStringLiteral(":memory:"));

    if (!m_db.open()) {
        m_lastError = m_db.lastError();
        return false;
    }

    // Create a simple songs table
    QSqlQuery query(m_db);
    bool success = query.exec(QStringLiteral(
        "CREATE TABLE songs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "path TEXT NOT NULL,"
        "title TEXT,"
        "artist TEXT,"
        "album TEXT,"
        "genre TEXT,"
        "year INTEGER,"
        "track_number INTEGER,"
        "disc_number INTEGER,"
        "duration INTEGER,"
        "artwork TEXT,"
        "date_added TEXT,"
        "last_played TEXT,"
        "play_count INTEGER DEFAULT 0,"
        "favorite INTEGER DEFAULT 0"
        ")"
    ));

    if (!success) {
        m_lastError = query.lastError();
    }

    return success;
}

bool MockDatabase::execute(const QString &query)
{
    QSqlQuery q(m_db);
    bool success = q.exec(query);
    if (!success) {
        m_lastError = q.lastError();
    }
    return success;
}

QList<QHash<QString, QVariant>> MockDatabase::query(const QString &queryStr)
{
    QList<QHash<QString, QVariant>> results;
    QSqlQuery q(m_db);

    if (!q.exec(queryStr)) {
        m_lastError = q.lastError();
        return results;
    }

    while (q.next()) {
        QHash<QString, QVariant> row;
        for (int i = 0; i < q.record().count(); ++i) {
            row[q.record().fieldName(i)] = q.value(i);
        }
        results.append(row);
    }

    return results;
}

QSqlError MockDatabase::lastError() const
{
    return m_lastError;
}

QSqlDatabase MockDatabase::database() const
{
    return m_db;
}

void MockDatabase::clear()
{
    execute(QStringLiteral("DELETE FROM songs"));
    m_songCount = 0;
}

bool MockDatabase::insertSong(const QHash<QString, QVariant> &songData)
{
    QStringList columns;
    QStringList placeholders;
    QList<QVariant> values;

    for (auto it = songData.begin(); it != songData.end(); ++it) {
        columns << it.key();
        placeholders << QStringLiteral("?");
        values << it.value();
    }

    QString sql = QStringLiteral("INSERT INTO songs (%1) VALUES (%2)")
                      .arg(columns.join(QStringLiteral(", ")),
                           placeholders.join(QStringLiteral(", ")));

    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const QVariant &val : values) {
        q.addBindValue(val);
    }

    bool success = q.exec();
    if (success) {
        m_songCount++;
    } else {
        m_lastError = q.lastError();
    }
    return success;
}

QList<QHash<QString, QVariant>> MockDatabase::getAllSongs()
{
    return query(QStringLiteral("SELECT * FROM songs"));
}

QList<QHash<QString, QVariant>> MockDatabase::getSongsByArtist(const QString &artist)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM songs WHERE artist = ?"));
    q.addBindValue(artist);

    QList<QHash<QString, QVariant>> results;
    if (q.exec()) {
        while (q.next()) {
            QHash<QString, QVariant> row;
            for (int i = 0; i < q.record().count(); ++i) {
                row[q.record().fieldName(i)] = q.value(i);
            }
            results.append(row);
        }
    } else {
        m_lastError = q.lastError();
    }
    return results;
}

QList<QHash<QString, QVariant>> MockDatabase::getSongsByAlbum(const QString &album)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("SELECT * FROM songs WHERE album = ?"));
    q.addBindValue(album);

    QList<QHash<QString, QVariant>> results;
    if (q.exec()) {
        while (q.next()) {
            QHash<QString, QVariant> row;
            for (int i = 0; i < q.record().count(); ++i) {
                row[q.record().fieldName(i)] = q.value(i);
            }
            results.append(row);
        }
    } else {
        m_lastError = q.lastError();
    }
    return results;
}

bool MockDatabase::deleteSong(int id)
{
    QSqlQuery q(m_db);
    q.prepare(QStringLiteral("DELETE FROM songs WHERE id = ?"));
    q.addBindValue(id);

    bool success = q.exec();
    if (success && q.numRowsAffected() > 0) {
        m_songCount--;
    } else if (!success) {
        m_lastError = q.lastError();
    }
    return success;
}

int MockDatabase::songCount() const
{
    // Query the actual count so it reflects transactions and rollbacks
    QSqlQuery q(m_db);
    if (q.exec(QStringLiteral("SELECT COUNT(*) FROM songs")) && q.next()) {
        return q.value(0).toInt();
    }
    return m_songCount;
}

bool MockDatabase::beginTransaction()
{
    return m_db.transaction();
}

bool MockDatabase::commit()
{
    return m_db.commit();
}

bool MockDatabase::rollback()
{
    return m_db.rollback();
}
