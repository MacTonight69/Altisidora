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
    // Edit the identity of ONE album: rename (title, artist) to
    // (newTitle, newArtist) in ONE transaction. Songs follow through
    // album_id and the album's artwork survives (it is a column of the
    // same row). An empty artist addresses the album with no artist.
    //
    // Title and artist move together on purpose. Applied as two separate
    // steps, the intermediate state would have to be collision-free too:
    // moving "Hits" from artist A to artist B while renaming it "Best" is
    // legal even when A already owns a "Best", but a title-only step would
    // see that sibling and refuse. The collision check therefore runs once,
    // on the FINAL identity, excluding the row being edited so a case-only
    // change of either name is never blocked by finding this very album.
    // Refused (false) on an empty title/newTitle/newArtist, an unchanged
    // identity, an unknown album, or when (newTitle, newArtist) already
    // exists — no database constraint enforces that uniqueness, the
    // refusal is code only.
    bool setAlbumIdentity(const QString &title, const QString &artist,
                          const QString &newTitle, const QString &newArtist);
    // Point a song at an already-cached artwork file and flag the row as
    // edited = 1 (see updateSong()). Returns false for an unknown id or
    // on SQL error. The album artwork rollup runs afterwards in the same
    // transaction, exactly like the scan commit path.
    bool setArtwork(qint64 songId, const QString &imagePath);
    // Set the artwork of every song of the album (title, artist) in ONE
    // transaction, then roll the cover up onto the album row. An empty
    // imagePath is the explicit "remove the cover" intent, the same shape a
    // single-song removal uses (artwork_path = ''). The path is expected to
    // be an already-cached file, exactly like setArtwork(). Refused (false)
    // for an unknown album or an album with no songs.
    bool setAlbumArtwork(const QString &title, const QString &artist,
                         const QString &imagePath);
    // path -> {mtimeMs, size} for every stored song, used to skip re-reading
    // tags for files that have not changed since the last scan.
    QHash<QString, QPair<qint64, qint64>> songFileStats() const;
    // id -> path for every stored song, used by the scan's pruning pass to
    // find the rows whose file no longer exists.
    QList<QPair<qint64, QString>> songIdsAndPaths() const;
    // id -> path for the requested ids, resolved in one (or a few, when the
    // selection exceeds SQLite's variable limit) queries. Ids that are not
    // in the table are simply absent from the map. Used to resolve a
    // delete/hide selection without one getSong() per id.
    QHash<qint64, QString> songPathsByIds(const QList<qint64> &ids) const;
    bool removeSong(qint64 id);
    // Remove several songs in ONE transaction: their playlist references
    // first, then the rows, then a single orphan-entity cleanup. Used by the
    // scan to forget songs whose file disappeared from disk. An empty list is
    // a no-op and returns false.
    bool removeSongs(const QList<qint64> &ids);
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
     * The tag-edit paths call the album-scoped refreshAlbumArtworkFor()
     * instead, because they only touch a handful of albums.
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
    // Insert several hidden paths in ONE transaction (see addIgnoredPath()).
    // Empty strings are skipped; an empty (or all-empty) batch is a
    // successful no-op. is_dir is left at 0 for every entry: it is
    // informational only and never read (see createTables()).
    bool addIgnoredPaths(const QStringList &paths);
    bool removeIgnoredPath(const QString &path);
    QStringList ignoredPaths() const;

private:
    // Ids of the artist/album/genre rows a mutation touched, used to scope
    // the post-mutation artwork rollup and orphan cleanup to what actually
    // changed (the unscoped versions are O(albums) + O(songs) per mutation).
    struct TouchedEntities {
        QList<qint64> albumIds;
        QList<qint64> artistIds;
        QList<qint64> genreIds;
    };

    bool execute(const QString &sql);

    // Transaction-free core of updateSong()/updateSongs(): SQLite has no
    // nested BEGIN, so the surrounding transaction lives in the callers.
    // Appends the row's old and new entity ids to @p touched.
    bool updateSongRow(const Song &song, TouchedEntities *touched);
    // Entity ids referenced by the given songs, read before their rows are
    // deleted so removeSongs() can scope its cleanup.
    TouchedEntities entityIdsForSongs(const QList<qint64> &songIds) const;
    // Roll album covers up for just the given albums (the full
    // refreshAlbumArtwork() is O(albums) and ran on every tag edit).
    void refreshAlbumArtworkFor(const QList<qint64> &albumIds);
    // Best-effort deletion of artists/albums/genres that no longer have any
    // songs, restricted to the entities @p touched can have orphaned.
    void cleanupOrphanEntitiesFor(const TouchedEntities &touched);
    // Deletes the rows of @p table whose id is in @p ids and that no song
    // references through @p column anymore. Chunked for the variable limit.
    void pruneOrphanEntities(const QString &table, const QString &column,
                             const QList<qint64> &ids);

    QSqlDatabase m_db;
};
