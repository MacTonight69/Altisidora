#include "LibraryController.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QPair>
#include <QSet>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrent>

#include "Scanner.h"
#include "MetadataReader.h"

namespace {

// Short clips (notification sounds, ringtones, podcast intros...) are not
// real songs: the personal-folder auto-scan skips audio <= this duration.
constexpr qint64 kMinPersonalAudioDurationMs = 30000;

/**
 * @brief Rows that a scan must forget because their file is gone.
 *
 * A scan is also a reconciliation: a file deleted behind the app's back (a
 * file manager, another program, an edit that changed the extension) must not
 * survive as a phantom row. Removal is deliberately conservative, so that an
 * unavailable library never costs the user its metadata. A stored row is
 * reported as missing only when every one of these holds:
 *
 *  1. its path is inside the scanned root: scanning A never forgets the songs
 *     of B, whose files this scan did not look at;
 *  2. the walk did not see the path: a cheap pre-filter, the disk is only
 *     stat'ed for the few rows the walk missed;
 *  3. the file itself does not exist;
 *  4. its parent directory still exists. A whole folder that vanished (an
 *     unmounted drive, an unplugged disk) makes every file below it "missing"
 *     at once; that signature is not a deletion, and the row stays until the
 *     volume is back.
 *
 * A root that does not exist at all (unmounted mount point, deleted folder)
 * prunes nothing: a scan of nothing proves nothing.
 *
 * Runs on the scan worker thread: filesystem work stays off the UI thread.
 */
QList<qint64> missingSongsUnderRoot(
    const QString &root,
    const QVector<Scanner::FileEntry> &entries,
    const QList<QPair<qint64, QString>> &storedSongs)
{
    QList<qint64> missing;
    if (!QFileInfo::exists(root)) {
        return missing;
    }

    QSet<QString> seen;
    seen.reserve(entries.size());
    for (const Scanner::FileEntry &entry : entries) {
        seen.insert(entry.path);
    }

    // Paths under `root` are `root + "/" + ...` (QDirIterator builds them
    // that way), so the separator makes the prefix test exact: root "/music"
    // must not match "/music-archive/song.mp3".
    const QString prefix =
        root.endsWith(QDir::separator()) ? root : root + QDir::separator();
    for (const QPair<qint64, QString> &stored : storedSongs) {
        const QString &path = stored.second;
        if (!path.startsWith(prefix) || seen.contains(path)) {
            continue;
        }
        if (QFileInfo::exists(path)) {
            continue; // hidden, short or otherwise skipped, but still there
        }
        if (!QFileInfo::exists(QFileInfo(path).absolutePath())) {
            continue; // the whole folder is gone: suspended, not deleted
        }
        missing.append(stored.first);
    }
    return missing;
}

} // namespace

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
    , m_songsModel(this)
    , m_searchModel(this)
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    if (!m_db.open(dataPath + "/library.db"))
        qWarning() << "LibraryController: failed to open database";

    connect(&m_scanWatcher, &QFutureWatcher<ScanResult>::finished,
            this, &LibraryController::handleScanFinished);
}

QVector<QVariant> LibraryController::songs() const
{
    if (m_songsStale)
        rebuildSongsCache();
    return m_cachedSongs;
}

QVector<QVariant> LibraryController::albums() const
{
    if (m_albumsStale)
        rebuildAlbumsCache();
    return m_cachedAlbums;
}

QVector<QVariant> LibraryController::artists() const
{
    if (m_artistsStale)
        rebuildArtistsCache();
    return m_cachedArtists;
}

SongListModel *LibraryController::songsModel() const
{
    return const_cast<SongListModel *>(&m_songsModel);
}

SongListModel *LibraryController::searchModel() const
{
    return const_cast<SongListModel *>(&m_searchModel);
}

bool LibraryController::searchActive() const
{
    return m_searchActive;
}

void LibraryController::rebuildSongsCache() const
{
    m_cachedSongs.clear();
    m_cachedSongs.reserve(m_songs.size());
    for (const Song &song : m_songs) {
        m_cachedSongs.append(song.toVariantMap());
    }
    m_songsStale = false;
}

void LibraryController::rebuildAlbumsCache() const
{
    const QList<Album> albums = m_db.getAllAlbums();
    m_cachedAlbums.clear();
    m_cachedAlbums.reserve(albums.size());
    for (const Album &album : albums) {
        m_cachedAlbums.append(albumToVariant(album));
    }
    m_albumsStale = false;
}

void LibraryController::rebuildArtistsCache() const
{
    const QList<Artist> artists = m_db.getAllArtists();
    m_cachedArtists.clear();
    m_cachedArtists.reserve(artists.size());
    for (const Artist &artist : artists) {
        m_cachedArtists.append(artistToVariant(artist));
    }
    m_artistsStale = false;
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
    // Metadata extraction is parallelised further below. Ignored paths, the
    // stored file fingerprints and the stored rows (for the pruning pass) are
    // captured at scan start (they live in the database, which may only be
    // touched from this, the main, thread); new hide/unhide choices apply from
    // the next scan on.
    const QString root = QDir::cleanPath(directory);
    const QStringList ignoredPaths = m_db.ignoredPaths();
    const QHash<QString, QPair<qint64, qint64>> fileStats = m_db.songFileStats();
    const QList<QPair<qint64, QString>> storedSongs = m_db.songIdsAndPaths();
    m_scanWatcher.setFuture(QtConcurrent::run(
        [root, minDurationMs, ignoredPaths, fileStats, storedSongs]() {
            Scanner scanner;
            const QVector<Scanner::FileEntry> entries =
                scanner.scanEntries(root, ignoredPaths);

            // Read tags only for files that are new or changed. A file whose
            // stored mtime AND size both match its on-disk values is already
            // up to date, so its tags are neither read nor re-written;
            // unchanged files never reach the database.
            QVector<Scanner::FileEntry> toRead;
            toRead.reserve(entries.size());
            for (const Scanner::FileEntry &entry : entries) {
                const auto it = fileStats.constFind(entry.path);
                if (it != fileStats.constEnd()
                    && it.value().first == entry.mtimeMs
                    && it.value().second == entry.size) {
                    continue; // unchanged
                }
                toRead.append(entry);
            }

            // Extract tags in parallel: TagLib parsing is CPU-bound and each
            // read() builds its own TagLib::FileRef (MetadataReader holds no
            // state), so the work shards cleanly across cores. Use a dedicated
            // pool rather than the global one: this lambda already runs on a
            // global-pool thread, and blockingMapped() on that same pool would
            // deadlock on a single-core machine (its only thread is the one
            // blocked here, waiting for the map tasks it just queued).
            QThreadPool pool;
            pool.setMaxThreadCount(qMax(1, QThread::idealThreadCount()));

            const QList<Scanner::FileEntry> readList = toRead.toList();
            const QList<Song> read = QtConcurrent::blockingMapped<QList<Song>>(
                &pool, readList.cbegin(), readList.cend(),
                [](const Scanner::FileEntry &entry) {
                    MetadataReader reader; // one per call: no shared state
                    Song song = reader.read(entry.path);
                    song.mtimeMs = entry.mtimeMs;
                    song.size = entry.size;
                    return song;
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

            ScanResult result;
            result.songs = songs;
            result.missingIds = missingSongsUnderRoot(root, entries, storedSongs);
            return result;
        }));
}

// Runs on the main thread (delivered by the event loop).
void LibraryController::handleScanFinished()
{
    const ScanResult result = m_scanWatcher.result();

    // Songs whose file disappeared go first: the upserts and the album
    // artwork rollup below then run on the final set of songs, so a cover
    // never rolls up from a row that is about to be dropped.
    int removed = 0;
    if (!result.missingIds.isEmpty()) {
        if (m_db.removeSongs(result.missingIds))
            removed = result.missingIds.size();
        else
            qWarning() << "LibraryController: failed to remove missing songs";
    }

    const QList<Song> &found = result.songs;
    if (!found.isEmpty()) {
        // One transaction for the whole batch: in autocommit every upsert
        // statement would be its own fsync (~6 statements per song).
        if (!m_db.beginTransaction())
            qWarning() << "LibraryController: failed to start scan transaction";
        for (const Song &song : found) {
            // upsertSong() needs a non-const Song& (it stores the row id):
            // new paths are inserted, existing ones update only their
            // tag-derived columns (user data is preserved).
            Song mutableSong = song;
            m_db.upsertSong(mutableSong);
        }
        // Album covers roll up from the freshly written songs' artwork, in
        // the same transaction so the library never shows covers without
        // their songs (or vice versa).
        m_db.refreshAlbumArtwork();
        if (!m_db.commitTransaction())
            qWarning() << "LibraryController: failed to commit scan transaction";
    }

    m_songs = m_db.getAllSongs().toVector();
    m_songsStale = true;
    m_albumsStale = true;
    m_artistsStale = true;
    m_songsModel.applySnapshot(m_songs);
    m_scanRunning = false;
    emit libraryChanged();
    emit scanFinished(removed);
}

void LibraryController::refresh()
{
    m_songs = m_db.getAllSongs().toVector();
    m_songsStale = true;
    m_albumsStale = true;
    m_artistsStale = true;
    m_songsModel.applySnapshot(m_songs);
    emit libraryChanged();
}

void LibraryController::setSearch(const QString &query)
{
    if (query.isEmpty()) {
        m_searchModel.setSongs({});
        m_searchActive = false;
        emit searchChanged();
        return;
    }
    m_searchModel.setSongs(m_db.searchSongs(query).toVector());
    m_searchActive = true;
    emit searchChanged();
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
    QList<qint64> songIds;
    songIds.reserve(ids.size());
    for (const QVariant &id : ids) {
        songIds.append(id.toLongLong());
    }

    // Resolve every path in ONE query and delete the whole selection in ONE
    // transaction. The old per-id loop issued a getSong() and a removeSong()
    // transaction — each with its own full orphan-entity cleanup — per song,
    // which made a multi-selection O(N^2) in the database.
    const QHash<qint64, QString> paths = m_db.songPathsByIds(songIds);
    QStringList toIgnore;
    for (const qint64 id : songIds) {
        const QString path = paths.value(id);
        if (path.isEmpty()) {
            continue; // unknown id: nothing on disk to touch
        }
        if (deleteFiles) {
            // Active mode: delete the audio file from disk. If the OS
            // refuses (permissions, busy file...), register the path as
            // ignored so a later scan cannot silently re-import it.
            if (!QFile::remove(path)) {
                toIgnore.append(path);
            }
        } else {
            // Passive mode: keep the file on disk but make future scans
            // skip it through the existing hidden-items mechanism.
            toIgnore.append(path);
        }
    }
    if (!toIgnore.isEmpty()) {
        m_db.addIgnoredPaths(toIgnore);
    }
    m_db.removeSongs(songIds);
    refresh();
}

bool LibraryController::fillSongFromMap(const QVariantMap &map, Song *song) const
{
    const qint64 id = map.value(QStringLiteral("id")).toLongLong();
    if (id <= 0) {
        return false;
    }
    // Start from the stored row so the keys the map omits keep their
    // current values — this is what makes a batch edit a field-level
    // reassignment instead of an equalization of every field. A missing
    // row returns the default Song (id 0), which the check below
    // rejects.
    *song = m_db.getSong(id);
    if (song->id != id) {
        return false;
    }
    if (map.contains(QStringLiteral("title")))
        song->title = map.value(QStringLiteral("title")).toString();
    if (map.contains(QStringLiteral("artist")))
        song->artist = map.value(QStringLiteral("artist")).toString();
    if (map.contains(QStringLiteral("album")))
        song->album = map.value(QStringLiteral("album")).toString();
    if (map.contains(QStringLiteral("genre")))
        song->genre = map.value(QStringLiteral("genre")).toString();
    if (map.contains(QStringLiteral("year")))
        song->year = map.value(QStringLiteral("year")).toInt();
    if (map.contains(QStringLiteral("duration")))
        song->duration = map.value(QStringLiteral("duration")).toLongLong();
    if (map.contains(QStringLiteral("artwork")))
        song->artworkPath = map.value(QStringLiteral("artwork")).toString();
    return true;
}

bool LibraryController::updateSong(const QVariantMap &songMap)
{
    Song song;
    if (!fillSongFromMap(songMap, &song)) {
        qWarning() << "LibraryController::updateSong: missing or unknown song id";
        return false;
    }
    if (!m_db.updateSong(song)) {
        qWarning() << "LibraryController::updateSong: database update failed for id"
                   << song.id;
        return false;
    }
    // refresh() is the single stale-flags + applySnapshot + libraryChanged
    // sequence; it must not be duplicated here.
    refresh();
    return true;
}

bool LibraryController::updateSongs(const QVariantList &songMaps)
{
    if (songMaps.isEmpty()) {
        qWarning() << "LibraryController::updateSongs: empty batch";
        return false;
    }
    // Fill every song before touching the database so one invalid entry
    // aborts the whole batch with no write at all (the database side is
    // additionally atomic: one transaction per batch).
    QList<Song> songs;
    songs.reserve(songMaps.size());
    for (const QVariant &entry : songMaps) {
        Song song;
        if (!fillSongFromMap(entry.toMap(), &song)) {
            qWarning() << "LibraryController::updateSongs: missing or unknown song id in batch";
            return false;
        }
        songs.append(song);
    }
    if (!m_db.updateSongs(songs)) {
        qWarning() << "LibraryController::updateSongs: database update failed";
        return false;
    }
    refresh();
    return true;
}

bool LibraryController::renameArtist(const QString &from, const QString &to)
{
    if (from.isEmpty() || to.isEmpty()) {
        qWarning() << "LibraryController::renameArtist: names must not be empty";
        return false;
    }
    if (!m_db.renameArtist(from, to)) {
        qWarning() << "LibraryController::renameArtist: refused or failed:" << from << "->" << to;
        return false;
    }
    refresh();
    return true;
}

bool LibraryController::setAlbumIdentity(const QString &title, const QString &artist,
                                         const QString &newTitle,
                                         const QString &newArtist)
{
    if (title.isEmpty() || newTitle.isEmpty() || newArtist.isEmpty()) {
        qWarning() << "LibraryController::setAlbumIdentity: names must not be empty";
        return false;
    }
    if (title == newTitle && artist == newArtist) {
        qWarning() << "LibraryController::setAlbumIdentity: unchanged identity";
        return false;
    }
    if (!m_db.setAlbumIdentity(title, artist, newTitle, newArtist)) {
        qWarning() << "LibraryController::setAlbumIdentity: refused or failed:"
                   << title << "/" << artist << "->" << newTitle << "/" << newArtist;
        return false;
    }
    refresh();
    return true;
}

bool LibraryController::setArtwork(int songId, const QString &imagePath)
{
    if (songId <= 0 || imagePath.isEmpty() || !QFileInfo::exists(imagePath)) {
        qWarning() << "LibraryController::setArtwork: bad song id or image path";
        return false;
    }
    // Cache the picked image with the scan-time pipeline first, so the
    // stored path points at the app-managed cache file (content-hash
    // deduped with extracted covers), never at the user's original.
    const QString cachedPath = MetadataReader::cacheArtwork(imagePath);
    if (cachedPath.isEmpty()) {
        qWarning() << "LibraryController::setArtwork: failed to cache image" << imagePath;
        return false;
    }
    if (!m_db.setArtwork(songId, cachedPath)) {
        qWarning() << "LibraryController::setArtwork: database update failed for id" << songId;
        return false;
    }
    refresh();
    return true;
}

bool LibraryController::setAlbumArtwork(const QString &title, const QString &artist,
                                        const QString &imagePath)
{
    if (title.isEmpty()) {
        qWarning() << "LibraryController::setAlbumArtwork: empty album title";
        return false;
    }
    // A picked image goes through the scan-time cache first, so the album
    // never references the user's original file in place; an empty path is
    // the explicit "remove the cover" intent and skips the cache.
    QString cachedPath;
    if (!imagePath.isEmpty()) {
        if (!QFileInfo::exists(imagePath)) {
            qWarning() << "LibraryController::setAlbumArtwork: bad image path" << imagePath;
            return false;
        }
        cachedPath = MetadataReader::cacheArtwork(imagePath);
        if (cachedPath.isEmpty()) {
            qWarning() << "LibraryController::setAlbumArtwork: failed to cache image"
                       << imagePath;
            return false;
        }
    }
    if (!m_db.setAlbumArtwork(title, artist, cachedPath)) {
        qWarning() << "LibraryController::setAlbumArtwork: database update failed";
        return false;
    }
    refresh();
    return true;
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

void LibraryController::ignorePaths(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return;
    }
    // One insert batch and one refresh for the whole selection; the QML
    // "hide selection" flow used to call ignorePath() once per song, which
    // reloaded the whole library per song.
    if (m_db.addIgnoredPaths(paths)) {
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
