#include "DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QDebug>

namespace {

/**
 * SELECT that resolves artist/album/genre names from the foreign keys
 * so the Song struct fields are populated in a single query.
 */
const char *const kSongSelect =
    "SELECT s.*, a.name AS artist_name, al.title AS album_title, g.name AS genre_name "
    "FROM songs s "
    "LEFT JOIN artists a ON s.artist_id = a.id "
    "LEFT JOIN albums al ON s.album_id = al.id "
    "LEFT JOIN genres g ON s.genre_id = g.id ";

/**
 * Clause that excludes songs the user has hidden ("ignored"). An exact
 * path match hides one file; a prefix match hides everything inside an
 * ignored directory. Written to slot into a WHERE clause that already
 * references the songs table as "s" (no leading AND).
 */
const char *const kNotIgnoredClause =
    "NOT EXISTS (SELECT 1 FROM ignored_paths i "
    "WHERE s.path = i.path OR s.path LIKE i.path || '/%')";

Song songFromQuery(const QSqlQuery &query)
{
    Song song;
    song.id = query.value("id").toLongLong();
    song.path = query.value("path").toString();
    song.title = query.value("title").toString();
    song.artist = query.value("artist_name").toString();
    song.album = query.value("album_title").toString();
    song.genre = query.value("genre_name").toString();
    song.year = query.value("year").toInt();
    song.trackNumber = query.value("track_number").toInt();
    song.discNumber = query.value("disc_number").toInt();
    song.duration = query.value("duration").toLongLong();
    song.artworkPath = query.value("artwork_path").toString();
    song.dateAdded = QDateTime::fromString(query.value("date_added").toString(), Qt::ISODate);
    song.lastPlayed = QDateTime::fromString(query.value("last_played").toString(), Qt::ISODate);
    song.playCount = query.value("play_count").toInt();
    song.favorite = query.value("favorite").toInt();
    return song;
}

/** Find (or create) a row in a table keyed by a UNIQUE name column. */
qint64 resolveNamedRow(QSqlDatabase &db, const QString &table, const QString &value)
{
    if (value.isEmpty()) {
        return -1;
    }
    {
        QSqlQuery ins(db);
        ins.prepare(QStringLiteral("INSERT OR IGNORE INTO %1 (name) VALUES (?)").arg(table));
        ins.addBindValue(value);
        ins.exec();
    }
    QSqlQuery sel(db);
    sel.prepare(QStringLiteral("SELECT id FROM %1 WHERE name = ?").arg(table));
    sel.addBindValue(value);
    if (sel.exec() && sel.next()) {
        return sel.value(0).toLongLong();
    }
    return -1;
}

/** Find (or create) the album row matching title + artist. */
qint64 resolveAlbum(QSqlDatabase &db, const QString &title, qint64 artistId, int year)
{
    if (title.isEmpty()) {
        return -1;
    }
    const QVariant artistVar = artistId > 0 ? QVariant(artistId) : QVariant();
    {
        QSqlQuery sel(db);
        sel.prepare(QStringLiteral("SELECT id FROM albums WHERE title = ? AND artist_id IS ?"));
        sel.addBindValue(title);
        sel.addBindValue(artistVar);
        if (sel.exec() && sel.next()) {
            return sel.value(0).toLongLong();
        }
    }
    QSqlQuery ins(db);
    ins.prepare(QStringLiteral("INSERT INTO albums (title, artist_id, year) VALUES (?, ?, ?)"));
    ins.addBindValue(title);
    ins.addBindValue(artistVar);
    ins.addBindValue(year > 0 ? QVariant(year) : QVariant());
    if (ins.exec()) {
        return ins.lastInsertId().toLongLong();
    }
    return -1;
}

} // namespace

DatabaseManager::DatabaseManager()
{
    // Share the process-wide default SQLite connection instead of re-adding it:
    // re-adding a connection name that already exists invalidates the other
    // manager's handle ("old connection removed"), breaking the controllers
    // (library, playlists) that each own a DatabaseManager for the same file.
    if (QSqlDatabase::contains()) {
        m_db = QSqlDatabase::database();
    } else {
        m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
    }
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const QString &path)
{
    m_db.setDatabaseName(path);
    if (!m_db.open()) {
        qWarning() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }
    if (!createTables()) {
        qWarning() << "Failed to create tables:" << m_db.lastError().text();
        return false;
    }
    return true;
}

void DatabaseManager::close()
{
    if (m_db.isOpen()) {
        m_db.close();
    }
}

bool DatabaseManager::isOpen() const
{
    return m_db.isOpen();
}

bool DatabaseManager::beginTransaction()
{
    return m_db.transaction();
}

bool DatabaseManager::commitTransaction()
{
    return m_db.commit();
}

bool DatabaseManager::rollbackTransaction()
{
    return m_db.rollback();
}

bool DatabaseManager::execute(const QString &sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        qWarning() << "SQL error:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::createTables()
{
    // Artists
    execute("CREATE TABLE IF NOT EXISTS artists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT UNIQUE NOT NULL)");

    // Albums
    execute("CREATE TABLE IF NOT EXISTS albums ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "title TEXT NOT NULL, "
            "artist_id INTEGER, "
            "year INTEGER, "
            "artwork_path TEXT, "
            "FOREIGN KEY(artist_id) REFERENCES artists(id))");

    // Genres
    execute("CREATE TABLE IF NOT EXISTS genres ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT UNIQUE NOT NULL)");

    // Songs
    execute("CREATE TABLE IF NOT EXISTS songs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "path TEXT UNIQUE NOT NULL, "
            "title TEXT, "
            "artist_id INTEGER, "
            "album_id INTEGER, "
            "genre_id INTEGER, "
            "year INTEGER, "
            "track_number INTEGER, "
            "disc_number INTEGER, "
            "duration INTEGER, "
            "artwork_path TEXT, "
            "date_added TEXT, "
            "last_played TEXT, "
            "play_count INTEGER DEFAULT 0, "
            "favorite INTEGER DEFAULT 0, "
            "FOREIGN KEY(artist_id) REFERENCES artists(id), "
            "FOREIGN KEY(album_id) REFERENCES albums(id), "
            "FOREIGN KEY(genre_id) REFERENCES genres(id))");

    // Playlists
    execute("CREATE TABLE IF NOT EXISTS playlists ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "name TEXT NOT NULL, "
            "description TEXT, "
            "date_created TEXT)");

    // Playlist songs
    execute("CREATE TABLE IF NOT EXISTS playlist_songs ("
            "playlist_id INTEGER, "
            "song_id INTEGER, "
            "position INTEGER, "
            "PRIMARY KEY(playlist_id, song_id), "
            "FOREIGN KEY(playlist_id) REFERENCES playlists(id), "
            "FOREIGN KEY(song_id) REFERENCES songs(id))");

    // Hidden items. The user can hide files or whole directories without
    // deleting them: the rows stay in the database, only the queries and
    // the scanner exclude them. is_dir is informational (the filter works
    // on plain path prefixes).
    execute("CREATE TABLE IF NOT EXISTS ignored_paths ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, "
            "path TEXT UNIQUE NOT NULL, "
            "is_dir INTEGER DEFAULT 0, "
            "date_added TEXT)");

    // Schema version
    execute("CREATE TABLE IF NOT EXISTS schema_version ("
            "version INTEGER)");
    execute("INSERT OR IGNORE INTO schema_version (version) VALUES (1)");

    return true;
}

bool DatabaseManager::addSong(Song &song)
{
    if (song.path.isEmpty()) {
        return false;
    }

    // Reject duplicate paths before touching the lookup tables so that
    // rescanning a directory does not leave orphan artist/album rows.
    {
        QSqlQuery dup(m_db);
        dup.prepare(QStringLiteral("SELECT id FROM songs WHERE path = ?"));
        dup.addBindValue(song.path);
        if (dup.exec() && dup.next()) {
            return false;
        }
    }

    const qint64 artistId = resolveNamedRow(m_db, QStringLiteral("artists"), song.artist);
    const qint64 albumId = resolveAlbum(m_db, song.album, artistId, song.year);
    const qint64 genreId = resolveNamedRow(m_db, QStringLiteral("genres"), song.genre);

    QSqlQuery query(m_db);
    query.prepare("INSERT INTO songs (path, title, artist_id, album_id, genre_id, year, track_number, disc_number, duration, artwork_path, date_added, last_played, play_count, favorite) "
                  "VALUES (:path, :title, :artist_id, :album_id, :genre_id, :year, :track_number, :disc_number, :duration, :artwork_path, :date_added, :last_played, :play_count, :favorite)");
    query.bindValue(":path", song.path);
    query.bindValue(":title", song.title);
    query.bindValue(":artist_id", artistId > 0 ? QVariant(artistId) : QVariant());
    query.bindValue(":album_id", albumId > 0 ? QVariant(albumId) : QVariant());
    query.bindValue(":genre_id", genreId > 0 ? QVariant(genreId) : QVariant());
    query.bindValue(":year", song.year);
    query.bindValue(":track_number", song.trackNumber);
    query.bindValue(":disc_number", song.discNumber);
    query.bindValue(":duration", song.duration);
    query.bindValue(":artwork_path", song.artworkPath);
    query.bindValue(":date_added", song.dateAdded.toString(Qt::ISODate));
    query.bindValue(":last_played", song.lastPlayed.toString(Qt::ISODate));
    query.bindValue(":play_count", song.playCount);
    query.bindValue(":favorite", song.favorite ? 1 : 0);

    if (!query.exec()) {
        qWarning() << "Failed to add song:" << query.lastError().text();
        return false;
    }
    song.id = query.lastInsertId().toLongLong();
    return true;
}

bool DatabaseManager::removeSong(qint64 id)
{
    // playlist_songs has no ON DELETE CASCADE and SQLite does not enforce
    // foreign keys by default, so the song's playlist rows must be removed
    // first to avoid orphaned references.
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM playlist_songs WHERE song_id=:id");
    query.bindValue(":id", id);
    if (!query.exec()) {
        return false;
    }

    query.prepare("DELETE FROM songs WHERE id=:id");
    query.bindValue(":id", id);
    if (!query.exec()) {
        return false;
    }

    // Drop artists/albums/genres that no longer have any songs (best-effort
    // cleanup; the views already hide orphans via HAVING song_count > 0).
    QSqlQuery cleanup(m_db);
    cleanup.exec("DELETE FROM artists WHERE id NOT IN "
                 "(SELECT DISTINCT artist_id FROM songs WHERE artist_id IS NOT NULL)");
    cleanup.exec("DELETE FROM albums WHERE id NOT IN "
                 "(SELECT DISTINCT album_id FROM songs WHERE album_id IS NOT NULL)");
    cleanup.exec("DELETE FROM genres WHERE id NOT IN "
                 "(SELECT DISTINCT genre_id FROM songs WHERE genre_id IS NOT NULL)");

    return true;
}

Song DatabaseManager::getSong(qint64 id) const
{
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSongSelect) + QStringLiteral("WHERE s.id = :id"));
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        return songFromQuery(query);
    }
    return Song();
}

QList<Song> DatabaseManager::getAllSongs() const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    const QString sql = QString::fromLatin1(kSongSelect) +
                        QStringLiteral("WHERE ") + QString::fromLatin1(kNotIgnoredClause) +
                        QStringLiteral(" ORDER BY s.title, s.path");
    if (query.exec(sql)) {
        while (query.next()) {
            songs.append(songFromQuery(query));
        }
    }
    return songs;
}

QList<Song> DatabaseManager::searchSongs(const QString &query) const
{
    QList<Song> songs;
    QSqlQuery sqlQuery(m_db);
    sqlQuery.prepare(QString::fromLatin1(kSongSelect) +
                     QStringLiteral("WHERE (s.title LIKE :query OR s.path LIKE :query) AND ") +
                     QString::fromLatin1(kNotIgnoredClause) +
                     QStringLiteral(" ORDER BY s.title, s.path"));
    sqlQuery.bindValue(":query", "%" + query + "%");
    if (sqlQuery.exec()) {
        while (sqlQuery.next()) {
            songs.append(songFromQuery(sqlQuery));
        }
    }
    return songs;
}

QList<Song> DatabaseManager::getSongsByAlbum(const QString &title, const QString &artist) const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    QString where = QStringLiteral("WHERE al.title = :title");
    if (!artist.isEmpty())
        where += QStringLiteral(" AND a.name = :artist");
    where += QStringLiteral(" AND ") + QString::fromLatin1(kNotIgnoredClause);
    query.prepare(QString::fromLatin1(kSongSelect) + where +
                  QStringLiteral(" ORDER BY s.disc_number, s.track_number, s.title"));
    query.bindValue(":title", title);
    if (!artist.isEmpty())
        query.bindValue(":artist", artist);
    if (query.exec()) {
        while (query.next()) {
            songs.append(songFromQuery(query));
        }
    }
    return songs;
}

QList<Song> DatabaseManager::getSongsByArtist(const QString &artist) const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSongSelect) +
                  QStringLiteral("WHERE a.name = :artist AND ") +
                  QString::fromLatin1(kNotIgnoredClause) +
                  QStringLiteral(" ORDER BY s.album_id, s.disc_number, s.track_number, s.title"));
    query.bindValue(":artist", artist);
    if (query.exec()) {
        while (query.next()) {
            songs.append(songFromQuery(query));
        }
    }
    return songs;
}

QList<Artist> DatabaseManager::getAllArtists() const
{
    QList<Artist> artists;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral(
            "SELECT a.id, a.name, "
            "COUNT(DISTINCT s.id) AS song_count, "
            "COUNT(DISTINCT al.id) AS album_count "
            "FROM artists a "
            "LEFT JOIN songs s ON s.artist_id = a.id AND ") +
            QString::fromLatin1(kNotIgnoredClause) +
            QStringLiteral(
            "LEFT JOIN albums al ON al.artist_id = a.id AND ") +
            QString::fromLatin1(kNotIgnoredClause) +
            QStringLiteral(
            "GROUP BY a.id, a.name "
            "HAVING song_count > 0 "
            "ORDER BY a.name"))) {
        while (query.next()) {
            Artist artist;
            artist.id = query.value("id").toLongLong();
            artist.name = query.value("name").toString();
            artist.songCount = query.value("song_count").toInt();
            artist.albumCount = query.value("album_count").toInt();
            artists.append(artist);
        }
    }
    return artists;
}

QList<Album> DatabaseManager::getAllAlbums() const
{
    QList<Album> albums;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral(
            "SELECT al.id, al.title, al.year, al.artwork_path, "
            "a.name AS artist_name, COUNT(s.id) AS song_count "
            "FROM albums al "
            "LEFT JOIN artists a ON al.artist_id = a.id "
            "LEFT JOIN songs s ON s.album_id = al.id AND ") +
            QString::fromLatin1(kNotIgnoredClause) +
            QStringLiteral(
            "GROUP BY al.id "
            "HAVING song_count > 0 "
            "ORDER BY al.title"))) {
        while (query.next()) {
            Album album;
            album.id = query.value("id").toLongLong();
            album.title = query.value("title").toString();
            album.artist = query.value("artist_name").toString();
            album.year = query.value("year").toInt();
            album.songCount = query.value("song_count").toInt();
            album.artworkPath = query.value("artwork_path").toString();
            albums.append(album);
        }
    }
    return albums;
}

bool DatabaseManager::addPlaylist(Playlist &playlist)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO playlists (name, description, date_created) VALUES (:name, :description, :date_created)");
    query.bindValue(":name", playlist.name);
    query.bindValue(":description", playlist.description);
    query.bindValue(":date_created", playlist.dateCreated.toString(Qt::ISODate));
    if (query.exec()) {
        playlist.id = query.lastInsertId().toLongLong();
        return true;
    }
    return false;
}

bool DatabaseManager::updatePlaylist(const Playlist &playlist)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE playlists SET name=:name, description=:description WHERE id=:id");
    query.bindValue(":name", playlist.name);
    query.bindValue(":description", playlist.description);
    query.bindValue(":id", playlist.id);
    return query.exec();
}

bool DatabaseManager::removePlaylist(qint64 id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM playlist_songs WHERE playlist_id=:id");
    query.bindValue(":id", id);
    query.exec();

    query.prepare("DELETE FROM playlists WHERE id=:id");
    query.bindValue(":id", id);
    return query.exec();
}

QList<Playlist> DatabaseManager::getAllPlaylists() const
{
    QList<Playlist> playlists;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral(
            "SELECT p.id, p.name, p.description, p.date_created, "
            "COUNT(ps.song_id) AS song_count "
            "FROM playlists p "
            "LEFT JOIN playlist_songs ps ON ps.playlist_id = p.id "
            "GROUP BY p.id "
            "ORDER BY p.name"))) {
        while (query.next()) {
            Playlist playlist;
            playlist.id = query.value("id").toLongLong();
            playlist.name = query.value("name").toString();
            playlist.description = query.value("description").toString();
            playlist.dateCreated = QDateTime::fromString(query.value("date_created").toString(), Qt::ISODate);
            playlist.songCount = query.value("song_count").toInt();
            playlists.append(playlist);
        }
    }
    return playlists;
}

bool DatabaseManager::addSongToPlaylist(qint64 playlistId, qint64 songId, int position)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO playlist_songs (playlist_id, song_id, position) VALUES (:playlist_id, :song_id, :position)");
    query.bindValue(":playlist_id", playlistId);
    query.bindValue(":song_id", songId);
    query.bindValue(":position", position);
    return query.exec();
}

bool DatabaseManager::removeSongFromPlaylist(qint64 playlistId, qint64 songId)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM playlist_songs WHERE playlist_id=:playlist_id AND song_id=:song_id");
    query.bindValue(":playlist_id", playlistId);
    query.bindValue(":song_id", songId);
    return query.exec();
}

QList<Song> DatabaseManager::getPlaylistSongs(qint64 playlistId) const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSongSelect) +
                  QStringLiteral("JOIN playlist_songs ps ON s.id = ps.song_id "
                                 "WHERE ps.playlist_id = :playlist_id AND ") +
                  QString::fromLatin1(kNotIgnoredClause) +
                  QStringLiteral(" ORDER BY ps.position, ps.rowid"));
    query.bindValue(":playlist_id", playlistId);
    if (query.exec()) {
        while (query.next()) {
            songs.append(songFromQuery(query));
        }
    }
    return songs;
}

bool DatabaseManager::addIgnoredPath(const QString &path, bool isDir)
{
    if (path.isEmpty()) {
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO ignored_paths (path, is_dir, date_added) "
                  "VALUES (:path, :is_dir, :date_added)");
    query.bindValue(":path", path);
    query.bindValue(":is_dir", isDir ? 1 : 0);
    query.bindValue(":date_added", QDateTime::currentDateTime().toString(Qt::ISODate));
    return query.exec();
}

bool DatabaseManager::removeIgnoredPath(const QString &path)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM ignored_paths WHERE path = :path");
    query.bindValue(":path", path);
    return query.exec();
}

QStringList DatabaseManager::ignoredPaths() const
{
    QStringList paths;
    QSqlQuery query(m_db);
    if (query.exec(QStringLiteral("SELECT path FROM ignored_paths ORDER BY path"))) {
        while (query.next()) {
            paths.append(query.value(0).toString());
        }
    }
    return paths;
}
