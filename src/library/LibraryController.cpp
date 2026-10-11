#include "LibraryController.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrent>

#include "Scanner.h"
#include "MetadataReader.h"

namespace {

// Short clips (notification sounds, ringtones, podcast intros...) are not
// real songs: the personal-folder auto-scan skips audio <= this duration.
constexpr qint64 kMinPersonalAudioDurationMs = 30000;

} // namespace

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
    , m_songsModel(this)
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    m_db.open(dataPath + "/library.db");

    connect(&m_scanWatcher, &QFutureWatcher<QList<Song>>::finished,
            this, &LibraryController::handleScanFinished);
}

QVector<QVariant> LibraryController::songs() const
{
    if (m_cachesStale)
        rebuildCaches();
    return m_cachedSongs;
}

QVector<QVariant> LibraryController::albums() const
{
    if (m_cachesStale)
        rebuildCaches();
    return m_cachedAlbums;
}

QVector<QVariant> LibraryController::artists() const
{
    if (m_cachesStale)
        rebuildCaches();
    return m_cachedArtists;
}

SongListModel *LibraryController::songsModel() const
{
    return const_cast<SongListModel *>(&m_songsModel);
}

void LibraryController::rebuildCaches() const
{
    m_cachedSongs.clear();
    m_cachedSongs.reserve(m_songs.size());
    for (const Song &song : m_songs) {
        m_cachedSongs.append(song.toVariantMap());
    }

    const QList<Album> albums = m_db.getAllAlbums();
    m_cachedAlbums.clear();
    m_cachedAlbums.reserve(albums.size());
    for (const Album &album : albums) {
        m_cachedAlbums.append(albumToVariant(album));
    }

    const QList<Artist> artists = m_db.getAllArtists();
    m_cachedArtists.clear();
    m_cachedArtists.reserve(artists.size());
    for (const Artist &artist : artists) {
        m_cachedArtists.append(artistToVariant(artist));
    }

    m_cachesStale = false;
}

void LibraryController::scan(const QString &directory)
{
    startScan(directory, 0);
}

void LibraryController::scanPersonalMusic(const QString &directory)
{
    const QString dir = directory.isEmpty() ? QDir::homePath() : directory;
    startScan(dir, kMinPersonalAudioDurationMs);
}

void LibraryController::startScan(const QString &directory, qint64 minDurationMs)
{
    if (m_scanRunning) {
        return;
    }
    m_scanRunning = true;
    emit scanStarted();

    // File walking and metadata extraction are CPU/IO-bound and thread-safe:
    // run them off the UI thread with local copies of Scanner/MetadataReader.
    // Metadata extraction is parallelised further below. Ignored paths are
    // captured at scan start; new hide/unhide choices apply from the next
    // scan on.
    const QStringList ignoredPaths = m_db.ignoredPaths();
    m_scanWatcher.setFuture(QtConcurrent::run([directory, minDurationMs, ignoredPaths]() {
        Scanner scanner;
        const QVector<QString> files = scanner.scan(directory, ignoredPaths);

        // Extract tags in parallel: TagLib parsing is CPU-bound and each
        // read() builds its own TagLib::FileRef (MetadataReader holds no
        // state), so the work shards cleanly across cores. Use a dedicated
        // pool rather than the global one: this lambda already runs on a
        // global-pool thread, and blockingMapped() on that same pool would
        // deadlock on a single-core machine (its only thread is the one
        // blocked here, waiting for the map tasks it just queued).
        QThreadPool pool;
        pool.setMaxThreadCount(qMax(1, QThread::idealThreadCount()));

        const QList<QString> fileList = files.toList();
        const QList<Song> read = QtConcurrent::blockingMapped<QList<Song>>(
            &pool, fileList.cbegin(), fileList.cend(),
            [](const QString &filePath) {
                MetadataReader reader; // one per call: no shared state
                return reader.read(filePath);
            });

        QList<Song> songs;
        songs.reserve(read.size());
        for (const Song &song : read) {
            if (song.path.isEmpty()) {
                continue;
            }
            // minDurationMs <= 0: ingest everything (manual scans). For the
            // personal-folder auto-scan, skip audio shorter than the limit.
            if (minDurationMs > 0 && song.duration <= minDurationMs) {
                continue;
            }
            songs.append(song);
        }
        return songs;
    }));
}

// Runs on the main thread (delivered by the event loop).
void LibraryController::handleScanFinished()
{
    const QList<Song> found = m_scanWatcher.result();
    if (!found.isEmpty()) {
        // One transaction for the whole batch: in autocommit every addSong()
        // statement would be its own fsync (~6 statements per song).
        if (!m_db.beginTransaction())
            qWarning() << "LibraryController: failed to start scan transaction";
        for (const Song &song : found) {
            // addSong() needs a non-const Song& (it stores the generated id);
            // duplicate paths are silently rejected.
            Song mutableSong = song;
            m_db.addSong(mutableSong);
        }
        if (!m_db.commitTransaction())
            qWarning() << "LibraryController: failed to commit scan transaction";
    }

    m_songs = m_db.getAllSongs().toVector();
    m_cachesStale = true;
    m_songsModel.setSongs(m_songs);
    m_scanRunning = false;
    emit libraryChanged();
    emit scanFinished();
}

void LibraryController::refresh()
{
    m_songs = m_db.getAllSongs().toVector();
    m_cachesStale = true;
    m_songsModel.setSongs(m_songs);
    emit libraryChanged();
}

QVector<QVariant> LibraryController::search(const QString &query) const
{
    QVector<QVariant> result;
    for (const Song &song : m_db.searchSongs(query)) {
        result.append(song.toVariantMap());
    }
    return result;
}

QVector<QVariant> LibraryController::songsByAlbum(const QString &title,
                                                  const QString &artist) const
{
    QVector<QVariant> result;
    for (const Song &song : m_db.getSongsByAlbum(title, artist)) {
        result.append(song.toVariantMap());
    }
    return result;
}

QVector<QVariant> LibraryController::songsByArtist(const QString &artist) const
{
    QVector<QVariant> result;
    for (const Song &song : m_db.getSongsByArtist(artist)) {
        result.append(song.toVariantMap());
    }
    return result;
}

int LibraryController::songCount() const
{
    return m_songs.size();
}

void LibraryController::removeSongs(const QVariantList &ids, bool deleteFiles)
{
    if (ids.isEmpty()) {
        return;
    }
    for (const QVariant &id : ids) {
        const Song song = m_db.getSong(id.toLongLong());
        if (!song.path.isEmpty()) {
            if (deleteFiles) {
                // Active mode: delete the audio file from disk. If the OS
                // refuses (permissions, busy file...), register the path as
                // ignored so a later scan cannot silently re-import it.
                if (!QFile::remove(song.path)) {
                    m_db.addIgnoredPath(song.path, false);
                }
            } else {
                // Passive mode: keep the file on disk but make future scans
                // skip it through the existing hidden-items mechanism.
                m_db.addIgnoredPath(song.path, false);
            }
        }
        m_db.removeSong(id.toLongLong());
    }
    refresh();
}

QStringList LibraryController::ignoredPaths() const
{
    return m_db.ignoredPaths();
}

void LibraryController::ignorePath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    const bool isDir = QFileInfo(path).isDir();
    if (m_db.addIgnoredPath(path, isDir)) {
        refresh();
    }
}

void LibraryController::unignorePath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    if (m_db.removeIgnoredPath(path)) {
        refresh();
    }
}

QVariantMap LibraryController::albumToVariant(const Album &album) const
{
    QVariantMap map;
    map["id"] = album.id;
    map["title"] = album.title;
    map["artist"] = album.artist;
    map["year"] = album.year;
    map["songCount"] = album.songCount;
    map["artwork"] = album.artworkPath;
    return map;
}

QVariantMap LibraryController::artistToVariant(const Artist &artist) const
{
    QVariantMap map;
    map["id"] = artist.id;
    map["name"] = artist.name;
    map["albumCount"] = artist.albumCount;
    map["songCount"] = artist.songCount;
    return map;
}
