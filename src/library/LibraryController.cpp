#include "LibraryController.h"

#include <QStandardPaths>
#include <QDir>
#include <QtConcurrent>

namespace {

// Short clips (notification sounds, ringtones, podcast intros...) are not
// real songs: the personal-folder auto-scan skips audio <= this duration.
constexpr qint64 kMinPersonalAudioDurationMs = 30000;

} // namespace

LibraryController::LibraryController(QObject *parent)
    : QObject(parent)
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    m_db.open(dataPath + "/library.db");

    connect(&m_scanWatcher, &QFutureWatcher<QList<Song>>::finished,
            this, &LibraryController::handleScanFinished);
}

QVector<QVariant> LibraryController::songs() const
{
    QVector<QVariant> result;
    for (const Song &song : m_songs) {
        result.append(songToVariant(song));
    }
    return result;
}

QVector<QVariant> LibraryController::albums() const
{
    QVector<QVariant> result;
    for (const Album &album : m_db.getAllAlbums()) {
        result.append(albumToVariant(album));
    }
    return result;
}

QVector<QVariant> LibraryController::artists() const
{
    QVector<QVariant> result;
    for (const Artist &artist : m_db.getAllArtists()) {
        result.append(artistToVariant(artist));
    }
    return result;
}

QVector<QVariant> LibraryController::genres() const
{
    QVector<QVariant> result;
    for (const Genre &genre : m_db.getAllGenres()) {
        result.append(genreToVariant(genre));
    }
    return result;
}

void LibraryController::scan(const QString &directory)
{
    startScan(directory, 0);
}

void LibraryController::scanPersonalMusic()
{
    startScan(QDir::homePath(), kMinPersonalAudioDurationMs);
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
    m_scanWatcher.setFuture(QtConcurrent::run([directory, minDurationMs]() {
        Scanner scanner;
        MetadataReader reader;
        QList<Song> songs;
        const QVector<QString> files = scanner.scan(directory);
        songs.reserve(files.size());
        for (const QString &filePath : files) {
            const Song song = reader.read(filePath);
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
    for (const Song &song : found) {
        // addSong() needs a non-const Song& (it stores the generated id);
        // duplicate paths are silently rejected.
        Song mutableSong = song;
        m_db.addSong(mutableSong);
    }

    m_songs = m_db.getAllSongs().toVector();
    m_scanRunning = false;
    emit libraryChanged();
    emit scanFinished();
}

void LibraryController::refresh()
{
    m_songs = m_db.getAllSongs().toVector();
    emit libraryChanged();
}

QVector<QVariant> LibraryController::search(const QString &query) const
{
    QVector<QVariant> result;
    for (const Song &song : m_db.searchSongs(query)) {
        result.append(songToVariant(song));
    }
    return result;
}

int LibraryController::songCount() const
{
    return m_songs.size();
}

QVariantMap LibraryController::songToVariant(const Song &song) const
{
    QVariantMap map;
    map["id"] = song.id;
    map["path"] = song.path;
    map["title"] = song.title;
    map["artist"] = song.artist;
    map["album"] = song.album;
    map["genre"] = song.genre;
    map["year"] = song.year;
    map["trackNumber"] = song.trackNumber;
    map["discNumber"] = song.discNumber;
    map["duration"] = song.duration;
    map["artwork"] = song.artworkPath;
    map["dateAdded"] = song.dateAdded;
    map["lastPlayed"] = song.lastPlayed;
    map["playCount"] = song.playCount;
    map["favorite"] = song.favorite;
    return map;
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

QVariantMap LibraryController::genreToVariant(const Genre &genre) const
{
    QVariantMap map;
    map["id"] = genre.id;
    map["name"] = genre.name;
    map["songCount"] = genre.songCount;
    return map;
}
