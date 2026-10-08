#pragma once

#include <QObject>
#include <QVector>
#include <QVariant>
#include <QFutureWatcher>

#include "../models/Song.h"
#include "../models/Artist.h"
#include "../models/Album.h"
#include "../models/Genre.h"
#include "../models/Playlist.h"
#include "../database/DatabaseManager.h"
#include "Scanner.h"
#include "MetadataReader.h"

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
    Q_PROPERTY(QVector<QVariant> genres READ genres NOTIFY libraryChanged)

public:
    explicit LibraryController(QObject *parent = nullptr);

    QVector<QVariant> songs() const;
    QVector<QVariant> albums() const;
    QVector<QVariant> artists() const;
    QVector<QVariant> genres() const;

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
     * in the database are rejected by addSong().
     */
    void scanPersonalMusic();
    void refresh();
    QVector<QVariant> search(const QString &query) const;
    int songCount() const;

    /**
     * @brief Remove songs from the library.
     *
     * The songs are deleted from the database (including their playlist
     * references) and the library is refreshed. Passed IDs are taken from
     * the QML selection and may arrive in any order; only songs that still
     * exist are affected.
     */
    void removeSongs(const QVariantList &ids);

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
    QVariantMap songToVariant(const Song &song) const;
    QVariantMap albumToVariant(const Album &album) const;
    QVariantMap artistToVariant(const Artist &artist) const;
    QVariantMap genreToVariant(const Genre &genre) const;

    DatabaseManager m_db;
    QVector<Song> m_songs;

    // Background scan machinery. Scanner/MetadataReader are used by the
    // worker thread only through local copies created in scan().
    QFutureWatcher<QList<Song>> m_scanWatcher;
    bool m_scanRunning = false;
};
