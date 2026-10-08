#pragma once

#include <QObject>
#include <QVector>
#include <QVariant>

#include "../models/Playlist.h"
#include "../database/DatabaseManager.h"

/**
 * @brief Application-level playlist controller.
 */
class PlaylistController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVector<QVariant> playlists READ playlists NOTIFY playlistsChanged)

public:
    explicit PlaylistController(QObject *parent = nullptr);

    QVector<QVariant> playlists() const;

public slots:
    void createPlaylist(const QString &name);
    void deletePlaylist(qint64 id);
    void renamePlaylist(qint64 id, const QString &newName);
    void addSong(qint64 playlistId, qint64 songId);
    /**
     * @brief Add several songs to a playlist in one call.
     *
     * Inserts the songs in order and emits playlistsChanged() a single time
     * for the whole batch. QML passes the array of selected song ids.
     */
    void addSongs(qint64 playlistId, const QVariantList &songIds);
    void removeSong(qint64 playlistId, qint64 songId);
    QVector<QVariant> getPlaylistSongs(qint64 playlistId) const;

signals:
    void playlistsChanged();

private:
    DatabaseManager m_db;
};
