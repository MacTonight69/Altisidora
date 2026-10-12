#pragma once

#include <QObject>
#include <QVector>
#include <QVariant>
#include <QFutureWatcher>

#include "../models/Song.h"
#include "../models/Artist.h"
#include "../models/Album.h"
#include "../database/DatabaseManager.h"
#include "SongListModel.h"

/**
 * @brief Application-level library controller.
 *
 * Coordinates scanning, metadata extraction and database persistence.
 * QML interacts with this class only.
 */
class LibraryController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVector<QVariant> songs READ songs NOTIFY libraryChanged)
    Q_PROPERTY(QVector<QVariant> albums READ albums NOTIFY libraryChanged)
    Q_PROPERTY(QVector<QVariant> artists READ artists NOTIFY libraryChanged)
    Q_PROPERTY(QStringList ignoredPaths READ ignoredPaths NOTIFY libraryChanged)
    // Render model for the Songs list when it is not filtered by search
    // (delegates then read roles instead of QVariantMap re-wrapping). The
    // pointer is stable for the controller's lifetime: CONSTANT.
    Q_PROPERTY(SongListModel* songsModel READ songsModel CONSTANT)
    // Render model for the filtered Songs list while a search is active. The
    // rows are the same Song snapshots the full model holds, so delegates
    // read roles instead of a per-result QVariantMap array.
    Q_PROPERTY(SongListModel* searchModel READ searchModel CONSTANT)
    // True while the search model holds the active query's results. QML
    // switches the song list between songsModel and searchModel with it.
    Q_PROPERTY(bool searchActive READ searchActive NOTIFY searchChanged)

public:
    explicit LibraryController(QObject *parent = nullptr);

    QVector<QVariant> songs() const;
    QVector<QVariant> albums() const;
    QVector<QVariant> artists() const;
    SongListModel *songsModel() const;
    SongListModel *searchModel() const;
    bool searchActive() const;

    /**
     * @brief Filter the Songs list by @p query and refresh the search model.
     *
     * An empty query clears the search model and reports the library as
     * unfiltered (searchActive() becomes false); any other query rebuilds the
     * rows from searchSongs(). Runs on the main thread: the database
     * connection is main-thread only.
     */
    Q_INVOKABLE void setSearch(const QString &query);

public slots:
    /**
     * @brief Scan a directory in the background.
     *
     * The filesystem walk and metadata extraction run on a worker thread; the
     * results are committed to the database and reported through
     * scanFinished() on the main thread, so the UI stays responsive.
     *
     * This is the manual scan: every supported audio file is ingested
     * regardless of its duration.
     */
    void scan(const QString &directory);

    /**
     * @brief Automatically scan the personal folder in the background.
     *
     * Used on first launch. Only audio longer than 30 seconds is ingested
     * (short clips, notification sounds, etc. are skipped); files whose
     * stored mtime and size are unchanged are not re-read. When @p directory
     * is empty, the user's home folder is used.
     */
    void scanPersonalMusic(const QString &directory = QString());
    void refresh();
    QStringList ignoredPaths() const;
    QVector<QVariant> search(const QString &query) const;
    int songCount() const;

    /**
     * @brief All songs that match an album (and optionally its artist).
     *
     * Used by the album drill-down view; songs are ordered by disc and
     * track number so they behave like a playlist. Non-hidden songs only.
     */
    QVector<QVariant> songsByAlbum(const QString &title, const QString &artist = QString()) const;

    /**
     * @brief All songs that match an artist, in playback order.
     *
     * Used by the artist drill-down view. Non-hidden songs only.
     */
    QVector<QVariant> songsByArtist(const QString &artist) const;

    /**
     * @brief Hide a file or directory without deleting it.
     *
     * The path keeps its database rows (playlists, stats), but it is no
     * longer returned by any song query and future scans skip it.
     */
    void ignorePath(const QString &path);

    /**
     * @brief Stop hiding a previously ignored file or directory.
     */
    void unignorePath(const QString &path);

    /**
     * @brief Remove songs from the library.
     *
     * The songs are always deleted from the database (including their
     * playlist references) and the library is refreshed. Passed IDs are
     * taken from the QML selection and may arrive in any order; only songs
     * that still exist are affected.
     *
     * @param ids Song IDs to remove.
     * @param deleteFiles When true (active mode), the audio file is also
     *        deleted from disk; if the removal fails the path is registered
     *        as ignored so it cannot reappear on the next scan. When false
     *        (passive mode), the file is kept on disk but its path is
     *        registered as ignored, so future scans skip it.
     */
    void removeSongs(const QVariantList &ids, bool deleteFiles);

public:
    /**
     * @brief Start a background scan with an optional minimum duration.
     *
     * @param directory Root directory to scan recursively
     * @param minDurationMs Only keep songs longer than this many
     *        milliseconds; a value <= 0 disables the filter
     */
    void startScan(const QString &directory, qint64 minDurationMs);

    // ---- Tag editing (Option D) ----
    // Main-thread only (the DB connection is main-thread only); false +
    // qWarning on bad input, never throw into QML; on success caches/model
    // rebuild and libraryChanged() fires exactly once.
    //
    // The maps use the QML-facing keys of Song::toVariantMap() with
    // overlay semantics: keys absent from the map keep their stored
    // values, so a batch that reassigns only "album" never equalizes the
    // other fields of the selected songs.
    Q_INVOKABLE bool updateSong(const QVariantMap &songMap);
    Q_INVOKABLE bool updateSongs(const QVariantList &songMaps);
    Q_INVOKABLE bool renameArtist(const QString &from, const QString &to);
    Q_INVOKABLE bool setArtwork(int songId, const QString &imagePath);
    // Album-scoped identity edit used by the album drill-down: rename this
    // album and/or move it (and only it) to another artist. Title and artist
    // are one atomic backend call, so a combined edit cannot be refused by
    // an intermediate state that the final identity never has. The global
    // artist rename lives in renameArtist(). Refused when
    // title/newTitle/newArtist is empty, the identity is unchanged, the
    // album is unknown, or (newTitle, newArtist) already exists.
    Q_INVOKABLE bool setAlbumIdentity(const QString &title, const QString &artist,
                                      const QString &newTitle,
                                      const QString &newArtist);
    // Album-scoped cover edit: point every song of the album at the picked
    // image (validated and cached first, exactly like setArtwork()), or, with
    // an empty imagePath, clear the cover. Refused for an unknown album.
    Q_INVOKABLE bool setAlbumArtwork(const QString &title, const QString &artist,
                                     const QString &imagePath);

signals:
    void scanStarted();
    void scanFinished();
    void libraryChanged();
    // searchActive() changed (or the search results were rebuilt).
    void searchChanged();

private slots:
    void handleScanFinished();

private:
    QVariantMap albumToVariant(const Album &album) const;
    QVariantMap artistToVariant(const Artist &artist) const;
    // Fill *song from the stored row behind map["id"], then overwrite
    // only the fields the map carries (overlay semantics: absent keys
    // keep the stored values, explicit empty strings clear a field).
    // False when the map has no usable id or the id is unknown.
    bool fillSongFromMap(const QVariantMap &map, Song *song) const;

    DatabaseManager m_db;
    QVector<Song> m_songs;

    // Role-based render model over the same m_songs snapshot (see
    // songsModel()). Kept in sync by refresh()/handleScanFinished().
    SongListModel m_songsModel;

    // Role-based render model over the current search results (see
    // searchModel()). Rebuilt by setSearch() and cleared when the query is
    // empty; m_searchActive mirrors whether it currently holds a query.
    SongListModel m_searchModel;
    bool m_searchActive = false;

    // QML-facing views of the library (QVariant wrappers). Built lazily per
    // property on first read after a library change; QVector is implicitly
    // shared, so returning them is cheap. Each flag is set in
    // refresh()/handleScanFinished() (the only two paths that rebuild
    // m_songs).
    mutable QVector<QVariant> m_cachedSongs;
    mutable QVector<QVariant> m_cachedAlbums;
    mutable QVector<QVariant> m_cachedArtists;
    mutable bool m_songsStale = true;
    mutable bool m_albumsStale = true;
    mutable bool m_artistsStale = true;

    void rebuildSongsCache() const;
    void rebuildAlbumsCache() const;
    void rebuildArtistsCache() const;

    // Background scan machinery. Scanner/MetadataReader are used by the
    // worker thread only through local copies created in scan().
    QFutureWatcher<QList<Song>> m_scanWatcher;
    bool m_scanRunning = false;
};
