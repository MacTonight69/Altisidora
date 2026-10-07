#include "DatabaseManager.h"

#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QDateTime>
#include <QDebug>

DatabaseManager::DatabaseManager()
{
    m_db = QSqlDatabase::addDatabase("QSQLITE");
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

bool DatabaseManager::execute(const QString &sql)
{
    QSqlQuery query(m_db);
    if (!query.exec(sql)) {
        qWarning() << "SQL error:" << query.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::versionTableExists()
{
    QSqlQuery query(m_db);
    return query.exec("SELECT name FROM sqlite_master WHERE type='table' AND name='schema_version'") && query.next();
}

int DatabaseManager::schemaVersion() const
{
    QSqlQuery query(m_db);
    if (query.exec("SELECT version FROM schema_version") && query.next()) {
        return query.value(0).toInt();
    }
    return 0;
}

bool DatabaseManager::migrate(int version)
{
    Q_UNUSED(version)
    // TODO: Implement migration logic
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

    // Schema version
    execute("CREATE TABLE IF NOT EXISTS schema_version ("
            "version INTEGER)");
    execute("INSERT OR IGNORE INTO schema_version (version) VALUES (1)");

    return true;
}

bool DatabaseManager::addSong(Song &song)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO songs (path, title, artist_id, album_id, genre_id, year, track_number, disc_number, duration, artwork_path, date_added, last_played, play_count, favorite) "
                  "VALUES (:path, :title, :artist_id, :album_id, :genre_id, :year, :track_number, :disc_number, :duration, :artwork_path, :date_added, :last_played, :play_count, :favorite)");
    query.bindValue(":path", song.path);
    query.bindValue(":title", song.title);
    // Simplified: using text fields for artist/album/genre instead of IDs for now
    query.bindValue(":artist_id", QVariant());
    query.bindValue(":album_id", QVariant());
    query.bindValue(":genre_id", QVariant());
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

bool DatabaseManager::updateSong(const Song &song)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE songs SET title=:title, year=:year, track_number=:track_number, disc_number=:disc_number, duration=:duration, artwork_path=:artwork_path, last_played=:last_played, play_count=:play_count, favorite=:favorite WHERE id=:id");
    query.bindValue(":title", song.title);
    query.bindValue(":year", song.year);
    query.bindValue(":track_number", song.trackNumber);
    query.bindValue(":disc_number", song.discNumber);
    query.bindValue(":duration", song.duration);
    query.bindValue(":artwork_path", song.artworkPath);
    query.bindValue(":last_played", song.lastPlayed.toString(Qt::ISODate));
    query.bindValue(":play_count", song.playCount);
    query.bindValue(":favorite", song.favorite ? 1 : 0);
    query.bindValue(":id", song.id);
    return query.exec();
}

bool DatabaseManager::removeSong(qint64 id)
{
    QSqlQuery query(m_db);
    query.prepare("DELETE FROM songs WHERE id=:id");
    query.bindValue(":id", id);
    return query.exec();
}

Song DatabaseManager::getSong(qint64 id) const
{
    QSqlQuery query(m_db);
    query.prepare("SELECT * FROM songs WHERE id=:id");
    query.bindValue(":id", id);
    if (query.exec() && query.next()) {
        Song song;
        song.id = query.value("id").toLongLong();
        song.path = query.value("path").toString();
        song.title = query.value("title").toString();
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
    return Song();
}

QList<Song> DatabaseManager::getAllSongs() const
{
    QList<Song> songs;
    QSqlQuery query(m_db);
    if (query.exec("SELECT * FROM songs")) {
        while (query.next()) {
            Song song;
            song.id = query.value("id").toLongLong();
            song.path = query.value("path").toString();
            song.title = query.value("title").toString();
            song.year = query.value("year").toInt();
            song.trackNumber = query.value("track_number").toInt();
            song.discNumber = query.value("disc_number").toInt();
            song.duration = query.value("duration").toLongLong();
            song.artworkPath = query.value("artwork_path").toString();
            song.dateAdded = QDateTime::fromString(query.value("date_added").toString(), Qt::ISODate);
            song.lastPlayed = QDateTime::fromString(query.value("last_played").toString(), Qt::ISODate);
            song.playCount = query.value("play_count").toInt();
            song.favorite = query.value("favorite").toInt();
            songs.append(song);
        }
    }
    return songs;
}

QList<Song> DatabaseManager::searchSongs(const QString &query) const
{
    QList<Song> songs;
    QSqlQuery sqlQuery(m_db);
    sqlQuery.prepare("SELECT * FROM songs WHERE title LIKE :query OR path LIKE :query");
    sqlQuery.bindValue(":query", "%" + query + "%");
    if (sqlQuery.exec()) {
        while (sqlQuery.next()) {
            Song song;
            song.id = sqlQuery.value("id").toLongLong();
            song.path = sqlQuery.value("path").toString();
            song.title = sqlQuery.value("title").toString();
            song.year = sqlQuery.value("year").toInt();
            song.trackNumber = sqlQuery.value("track_number").toInt();
            song.discNumber = sqlQuery.value("disc_number").toInt();
            song.duration = sqlQuery.value("duration").toLongLong();
            song.artworkPath = sqlQuery.value("artwork_path").toString();
            song.dateAdded = QDateTime::fromString(sqlQuery.value("date_added").toString(), Qt::ISODate);
            song.lastPlayed = QDateTime::fromString(sqlQuery.value("last_played").toString(), Qt::ISODate);
            song.playCount = sqlQuery.value("play_count").toInt();
            song.favorite = sqlQuery.value("favorite").toInt();
            songs.append(song);
        }
    }
    return songs;
}

bool DatabaseManager::addArtist(Artist &artist)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO artists (name) VALUES (:name)");
    query.bindValue(":name", artist.name);
    if (query.exec()) {
        artist.id = query.lastInsertId().toLongLong();
        return true;
    }
    return false;
}

QList<Artist> DatabaseManager::getAllArtists() const
{
    QList<Artist> artists;
    QSqlQuery query(m_db);
    if (query.exec("SELECT * FROM artists")) {
        while (query.next()) {
            Artist artist;
            artist.id = query.value("id").toLongLong();
            artist.name = query.value("name").toString();
            artists.append(artist);
        }
    }
    return artists;
}

bool DatabaseManager::addAlbum(Album &album)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO albums (title, year, artwork_path) VALUES (:title, :year, :artwork_path)");
    query.bindValue(":title", album.title);
    query.bindValue(":year", album.year);
    query.bindValue(":artwork_path", album.artworkPath);
    if (query.exec()) {
        album.id = query.lastInsertId().toLongLong();
        return true;
    }
    return false;
}

QList<Album> DatabaseManager::getAllAlbums() const
{
    QList<Album> albums;
    QSqlQuery query(m_db);
    if (query.exec("SELECT * FROM albums")) {
        while (query.next()) {
            Album album;
            album.id = query.value("id").toLongLong();
            album.title = query.value("title").toString();
            album.year = query.value("year").toInt();
            album.artworkPath = query.value("artwork_path").toString();
            albums.append(album);
        }
    }
    return albums;
}

bool DatabaseManager::addGenre(Genre &genre)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT OR IGNORE INTO genres (name) VALUES (:name)");
    query.bindValue(":name", genre.name);
    if (query.exec()) {
        genre.id = query.lastInsertId().toLongLong();
        return true;
    }
    return false;
}

QList<Genre> DatabaseManager::getAllGenres() const
{
    QList<Genre> genres;
    QSqlQuery query(m_db);
    if (query.exec("SELECT * FROM genres")) {
        while (query.next()) {
            Genre genre;
            genre.id = query.value("id").toLongLong();
            genre.name = query.value("name").toString();
            genres.append(genre);
        }
    }
    return genres;
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
    if (query.exec("SELECT * FROM playlists")) {
        while (query.next()) {
            Playlist playlist;
            playlist.id = query.value("id").toLongLong();
            playlist.name = query.value("name").toString();
            playlist.description = query.value("description").toString();
            playlist.dateCreated = QDateTime::fromString(query.value("date_created").toString(), Qt::ISODate);
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
    query.prepare("SELECT s.* FROM songs s "
                  "JOIN playlist_songs ps ON s.id = ps.song_id "
                  "WHERE ps.playlist_id = :playlist_id "
                  "ORDER BY ps.position");
    query.bindValue(":playlist_id", playlistId);
    if (query.exec()) {
        while (query.next()) {
            Song song;
            song.id = query.value("id").toLongLong();
            song.path = query.value("path").toString();
            song.title = query.value("title").toString();
            song.year = query.value("year").toInt();
            song.trackNumber = query.value("track_number").toInt();
            song.discNumber = query.value("disc_number").toInt();
            song.duration = query.value("duration").toLongLong();
            song.artworkPath = query.value("artwork_path").toString();
            song.dateAdded = QDateTime::fromString(query.value("date_added").toString(), Qt::ISODate);
            song.lastPlayed = QDateTime::fromString(query.value("last_played").toString(), Qt::ISODate);
            song.playCount = query.value("play_count").toInt();
            song.favorite = query.value("favorite").toInt();
            songs.append(song);
        }
    }
    return songs;
}

bool DatabaseManager::updatePlayCount(qint64 songId)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE songs SET play_count = play_count + 1, last_played = :last_played WHERE id = :id");
    query.bindValue(":last_played", QDateTime::currentDateTime().toString(Qt::ISODate));
    query.bindValue(":id", songId);
    return query.exec();
}

bool DatabaseManager::setFavorite(qint64 songId, bool favorite)
{
    QSqlQuery query(m_db);
    query.prepare("UPDATE songs SET favorite = :favorite WHERE id = :id");
    query.bindValue(":favorite", favorite ? 1 : 0);
    query.bindValue(":id", songId);
    return query.exec();
}
