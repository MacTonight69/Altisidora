#include "PlaylistController.h"

#include <QStandardPaths>
#include <QDir>

PlaylistController::PlaylistController(QObject *parent)
    : QObject(parent)
{
    QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);
    m_db.open(dataPath + "/library.db");
}

QVector<QVariant> PlaylistController::playlists() const
{
    QVector<QVariant> result;
    for (const Playlist &pl : m_db.getAllPlaylists()) {
        QVariantMap map;
        map["id"] = pl.id;
        map["name"] = pl.name;
        map["description"] = pl.description;
        map["dateCreated"] = pl.dateCreated;
        map["songCount"] = pl.songCount;
        result.append(map);
    }
    return result;
}

void PlaylistController::createPlaylist(const QString &name)
{
    Playlist pl;
    pl.name = name;
    pl.dateCreated = QDateTime::currentDateTime();
    if (m_db.addPlaylist(pl)) {
        emit playlistsChanged();
    }
}

void PlaylistController::deletePlaylist(qint64 id)
{
    if (m_db.removePlaylist(id)) {
        emit playlistsChanged();
    }
}

void PlaylistController::renamePlaylist(qint64 id, const QString &newName)
{
    Playlist pl;
    pl.id = id;
    pl.name = newName;
    if (m_db.updatePlaylist(pl)) {
        emit playlistsChanged();
    }
}

void PlaylistController::addSong(qint64 playlistId, qint64 songId)
{
    m_db.addSongToPlaylist(playlistId, songId, 0);
    emit playlistsChanged();
}

void PlaylistController::removeSong(qint64 playlistId, qint64 songId)
{
    m_db.removeSongFromPlaylist(playlistId, songId);
    emit playlistsChanged();
}

QVector<QVariant> PlaylistController::getPlaylistSongs(qint64 playlistId) const
{
    QVector<QVariant> result;
    for (const Song &song : m_db.getPlaylistSongs(playlistId)) {
        QVariantMap map;
        map["id"] = song.id;
        map["path"] = song.path;
        map["title"] = song.title;
        map["artist"] = song.artist;
        map["album"] = song.album;
        map["duration"] = song.duration;
        result.append(map);
    }
    return result;
}
