#include "SongListModel.h"

SongListModel::SongListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SongListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_songs.size();
}

QVariant SongListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size())
        return QVariant();

    const Song &song = m_songs.at(index.row());
    switch (role) {
    case IdRole:           return song.id;
    case PathRole:         return song.path;
    case TitleRole:        return song.title;
    case ArtistRole:       return song.artist;
    case AlbumRole:        return song.album;
    case GenreRole:        return song.genre;
    case YearRole:         return song.year;
    case TrackNumberRole:  return song.trackNumber;
    case DiscNumberRole:   return song.discNumber;
    case DurationRole:     return song.duration;
    case ArtworkRole:      return song.artworkPath;
    case FavoriteRole:     return song.favorite;
    default:               return QVariant();
    }
}

QHash<int, QByteArray> SongListModel::roleNames() const
{
    // Same names the old QVariantMap lists exposed, so delegates keep
    // reading model.title / model.artist / ... unchanged.
    return {
        {IdRole,          "id"},
        {PathRole,        "path"},
        {TitleRole,       "title"},
        {ArtistRole,      "artist"},
        {AlbumRole,       "album"},
        {GenreRole,       "genre"},
        {YearRole,        "year"},
        {TrackNumberRole, "trackNumber"},
        {DiscNumberRole,  "discNumber"},
        {DurationRole,    "duration"},
        {ArtworkRole,     "artwork"},
        {FavoriteRole,    "favorite"},
    };
}

void SongListModel::setSongs(const QVector<Song> &songs)
{
    beginResetModel();
    m_songs = songs;
    endResetModel();
}

QVariantMap SongListModel::songData(int row) const
{
    if (row < 0 || row >= m_songs.size())
        return QVariantMap();
    return m_songs.at(row).toVariantMap();
}

int SongListModel::rowOfId(int id) const
{
    for (int i = 0; i < m_songs.size(); ++i) {
        if (m_songs.at(i).id == id)
            return i;
    }
    return -1;
}