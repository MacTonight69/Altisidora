#pragma once

#include <QSqlDatabase>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "../models/Song.h"
#include "../models/Artist.h"
#include "../models/Album.h"
#include "../models/Genre.h"
#include "../models/Playlist.h"

/**
 * @brief Manages SQLite database for the music library.
 *
 * All database access must go through this class. QML must NOT
 * access the database directly.
 */
class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    bool open(const QString &path);
    void close();
    bool isOpen() const;

    // Schema
    bool createTables();
    bool migrate(int version);
    int schemaVersion() const;

    // Songs
    bool addSong(Song &song);
    bool updateSong(const Song &song);
    bool removeSong(qint64 id);
    Song getSong(qint64 id) const;
    QList<Song> getAllSongs() const;
    QList<Song> searchSongs(const QString &query) const;
    QList<Song> getSongsByAlbum(const QString &title, const QString &artist = QString()) const;
    QList<Song> getSongsByArtist(const QString &artist) const;

    // Artists
    bool addArtist(Artist &artist);
    QList<Artist> getAllArtists() const;

    // Albums
    bool addAlbum(Album &album);
    QList<Album> getAllAlbums() const;

    // Genres
    bool addGenre(Genre &genre);
    QList<Genre> getAllGenres() const;

    // Playlists
    bool addPlaylist(Playlist &playlist);
    bool updatePlaylist(const Playlist &playlist);
    bool removePlaylist(qint64 id);
    QList<Playlist> getAllPlaylists() const;
    bool addSongToPlaylist(qint64 playlistId, qint64 songId, int position);
    bool removeSongFromPlaylist(qint64 playlistId, qint64 songId);
    QList<Song> getPlaylistSongs(qint64 playlistId) const;

    // Statistics
    bool updatePlayCount(qint64 songId);
    bool setFavorite(qint64 songId, bool favorite);

    // Hidden items. A path added here (a file or a whole directory) is
    // kept in the database but excluded from every song query, so it
    // disappears from the lists without being deleted from disk.
    bool addIgnoredPath(const QString &path, bool isDir = false);
    bool removeIgnoredPath(const QString &path);
    QStringList ignoredPaths() const;

private:
    bool execute(const QString &sql);
    bool versionTableExists();

    QSqlDatabase m_db;
};
