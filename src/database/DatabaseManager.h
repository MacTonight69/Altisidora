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
    // per-song duplicate SELECT. Rows flagged edited = 1 (user-curated,
    // see updateSong()) keep their tag-derived columns and only have
    // mtime/size refreshed.
    bool upsertSong(Song &song);

    // ---- Tag editing (Option D) ----
    // Update the tag-derived columns (title, artist, album, genre, year,
    // duration, artwork) of one song the user edited, resolving names
    // through the same lookup paths as the scan (creating artist/album/
    // genre rows when the new names do not exist). The row is flagged
    // edited = 1 so later rescans keep these values instead of
    // overwriting them from the file's tags. User data (date_added,
    // last_played, play_count, favorite) is never touched; track_number
    // and disc_number are not user-editable and stay as the scan wrote
    // them. Returns false for an unknown id or on SQL error.
    bool updateSong(const Song &song);
    // Batch version: ONE transaction around every row, so a single bad
    // row (unknown id, SQL error) rolls the whole batch back — a
    // half-applied batch edit must never reach the database. The album
    // artwork rollup and the orphan-entity cleanup run once, after every
    // row succeeded. An empty list returns false.
    bool updateSongs(const QList<Song> &songs);
    // Rename an artist; songs follow through artist_id. Refused (false)
    // when either name is empty, the names are identical or the target
    // name already exists — merging two artists is a separate, explicit
    // feature. Also false when no artist carries the old name.
    bool renameArtist(const QString &from, const QString &to);
    // Rename the album (title, artist) to newTitle; songs follow through
    // album_id and the album's artwork survives (it is a column of the
    // renamed row). An empty artist addresses the album with no artist.
    // Refused (false) on empty title/newTitle, identical names, an
    // unknown album, or when (newTitle, artist) already exists (no
    // database constraint enforces that uniqueness — code only).
    bool renameAlbum(const QString &title, const QString &artist,
                     const QString &newTitle);
    // Point a song at an already-cached artwork file and flag the row as
    // edited = 1 (see updateSong()). Returns false for an unknown id or
    // on SQL error. The album artwork rollup runs afterwards in the same
    // transaction, exactly like the scan commit path.
    bool setArtwork(qint64 songId, const QString &imagePath);
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

    // Transaction-free core of updateSong()/updateSongs(): SQLite has no
    // nested BEGIN, so the surrounding transaction lives in the callers.
    bool updateSongRow(const Song &song);
    // Best-effort deletion of artists/albums/genres that no longer have
    // any songs (shared by removeSong() and the tag-edit paths).
    void cleanupOrphanEntities();

    QSqlDatabase m_db;
};
