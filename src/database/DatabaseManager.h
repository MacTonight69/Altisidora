#pragma once

#include <QSqlDatabase>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QHash>
#include <QPair>

#include "../models/Song.h"
#include "../models/Artist.h"
#include "../models/Album.h"
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

    // Explicitly group multiple writes into one SQLite transaction (each
    // statement in autocommit mode is its own fsync; a scan commits tens of
    // thousands of statements, so wrapping them in one transaction is a
    // large scan-speed win).
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();

    // Schema
    bool createTables();

    // Songs
    bool addSong(Song &song);
    // Insert a new song or, when its path already exists, refresh the
    // tag-derived columns (title, artist/album/genre, year, track/disc,
    // duration, artwork, mtime, size) without ever touching the user data
    // (date_added, last_played, play_count, favorite). song.id is set to the
    // resulting row id. This is the rescan path: it replaces addSong()'s
    // per-song duplicate SELECT.
    bool upsertSong(Song &song);
    // path -> {mtimeMs, size} for every stored song, used to skip re-reading
    // tags for files that have not changed since the last scan.
    QHash<QString, QPair<qint64, qint64>> songFileStats() const;
    bool removeSong(qint64 id);
    Song getSong(qint64 id) const;
    QList<Song> getAllSongs() const;
    QList<Song> searchSongs(const QString &query) const;
    QList<Song> getSongsByAlbum(const QString &title, const QString &artist = QString()) const;
    QList<Song> getSongsByArtist(const QString &artist) const;

    // Artists
    QList<Artist> getAllArtists() const;

    // Albums
    QList<Album> getAllAlbums() const;
    /**
     * @brief Set each album's artwork_path from its first artwork-bearing
     *        song, in disc/track order.
     *
     * Call inside the scan transaction; mirrors getSongsByAlbum() ordering.
     */
    void refreshAlbumArtwork();

    // Playlists
    bool addPlaylist(Playlist &playlist);
    bool updatePlaylist(const Playlist &playlist);
    bool removePlaylist(qint64 id);
    QList<Playlist> getAllPlaylists() const;
    bool addSongToPlaylist(qint64 playlistId, qint64 songId, int position);
    bool removeSongFromPlaylist(qint64 playlistId, qint64 songId);
    QList<Song> getPlaylistSongs(qint64 playlistId) const;

    // Hidden items. A path added here (a file or a whole directory) is
    // kept in the database but excluded from every song query, so it
    // disappears from the lists without being deleted from disk.
    bool addIgnoredPath(const QString &path, bool isDir = false);
    bool removeIgnoredPath(const QString &path);
    QStringList ignoredPaths() const;

private:
    bool execute(const QString &sql);

    QSqlDatabase m_db;
};
