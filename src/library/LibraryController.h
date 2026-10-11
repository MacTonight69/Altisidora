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

public:
    explicit LibraryController(QObject *parent = nullptr);

    QVector<QVariant> songs() const;
    QVector<QVariant> albums() const;
    QVector<QVariant> artists() const;
    SongListModel *songsModel() const;

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
     * (short clips, notification sounds, etc. are skipped); paths already
     * in the database are rejected by addSong(). When @p directory is
     * empty, the user's home folder is used.
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

signals:
    void scanStarted();
    void scanFinished();
    void libraryChanged();

private slots:
    void handleScanFinished();

private:
    QVariantMap albumToVariant(const Album &album) const;
    QVariantMap artistToVariant(const Artist &artist) const;

    DatabaseManager m_db;
    QVector<Song> m_songs;

    // Role-based render model over the same m_songs snapshot (see
    // songsModel()). Kept in sync by refresh()/handleScanFinished().
    SongListModel m_songsModel;

    // QML-facing views of the library (QVariant wrappers). Built once per
    // library change instead of on every property read; QVector is
    // implicitly shared, so returning them is cheap. m_cachesStale is set
    // in refresh()/handleScanFinished() (the only two paths that rebuild
    // m_songs).
    mutable QVector<QVariant> m_cachedSongs;
    mutable QVector<QVariant> m_cachedAlbums;
    mutable QVector<QVariant> m_cachedArtists;
    mutable bool m_cachesStale = true;

    void rebuildCaches() const;

    // Background scan machinery. Scanner/MetadataReader are used by the
    // worker thread only through local copies created in scan().
    QFutureWatcher<QList<Song>> m_scanWatcher;
    bool m_scanRunning = false;
};
