#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "../models/Song.h"

/**
 * @brief List model over a snapshot of the song library.
 *
 * Owns a QVector<Song> (the same snapshots LibraryController keeps) and
 * exposes the Song fields as named roles, so QML delegates read
 * model.title / model.artist / ... instead of re-wrapping every song into
 * a QVariantMap per binding evaluation. Role-based bindings also work for
 * plain JS arrays (model.<prop> proxies to the array element), which lets
 * the search path keep using arrays until it is migrated too.
 */
class SongListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        PathRole,
        TitleRole,
        ArtistRole,
        AlbumRole,
        GenreRole,
        YearRole,
        TrackNumberRole,
        DiscNumberRole,
        DurationRole,
        ArtworkRole,
        FavoriteRole,
    };

    explicit SongListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSongs(const QVector<Song> &songs);

    // For QML scripts that still need a map (e.g. pushing into the Player
    // queue) or a row lookup by song id.
    Q_INVOKABLE QVariantMap songData(int row) const;
    Q_INVOKABLE int rowOfId(int id) const;

private:
    QVector<Song> m_songs;
};