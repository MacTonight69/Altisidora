#include "SongListModel.h"

#include <algorithm>

#include <QSet>

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
    return fieldFor(song, static_cast<Role>(role));
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
        {PlayCountRole,   "playCount"},
        {LastPlayedRole,  "lastPlayed"},
    };
}

int SongListModel::count() const
{
    return m_songs.size();
}

void SongListModel::setSongs(const QVector<Song> &songs)
{
    beginResetModel();
    m_songs = songs;
    endResetModel();
    emit countChanged();
}

QVariant SongListModel::fieldFor(const Song &song, Role role)
{
    switch (role) {
    case IdRole:          return song.id;
    case PathRole:        return song.path;
    case TitleRole:       return song.title;
    case ArtistRole:      return song.artist;
    case AlbumRole:       return song.album;
    case GenreRole:       return song.genre;
    case YearRole:        return song.year;
    case TrackNumberRole: return song.trackNumber;
    case DiscNumberRole:  return song.discNumber;
    case DurationRole:    return song.duration;
    case ArtworkRole:     return song.artworkPath;
    case FavoriteRole:    return song.favorite;
    case PlayCountRole:   return song.playCount;
    case LastPlayedRole:  return song.lastPlayed;
    default:              return QVariant();
    }
}

bool SongListModel::sameDisplayData(const Song &a, const Song &b)
{
    return a.id == b.id
        && a.path == b.path
        && a.title == b.title
        && a.artist == b.artist
        && a.album == b.album
        && a.genre == b.genre
        && a.year == b.year
        && a.trackNumber == b.trackNumber
        && a.discNumber == b.discNumber
        && a.duration == b.duration
        && a.artworkPath == b.artworkPath
        && a.dateAdded == b.dateAdded
        && a.lastPlayed == b.lastPlayed
        && a.playCount == b.playCount
        && a.favorite == b.favorite;
}

void SongListModel::applySnapshot(const QVector<Song> &newSongs)
{
    const int oldCount = m_songs.size();

    QHash<qint64, int> oldRows;
    for (int i = 0; i < m_songs.size(); ++i)
        oldRows.insert(m_songs.at(i).id, i);

    QHash<qint64, int> newRows;
    for (int i = 0; i < newSongs.size(); ++i)
        newRows.insert(newSongs.at(i).id, i);

    // Kept ids, in the order each snapshot lists them.
    QVector<qint64> oldKept;
    QVector<qint64> newKept;
    for (const Song &song : m_songs)
        if (newRows.contains(song.id))
            oldKept.append(song.id);
    for (const Song &song : newSongs)
        if (oldRows.contains(song.id))
            newKept.append(song.id);

    // A kept id whose relative order changed means its sort key (title, path)
    // changed: signal a full reset rather than a misleading in-place update.
    if (oldKept != newKept) {
        beginResetModel();
        m_songs = newSongs;
        endResetModel();
        emit countChanged();
        return;
    }

    // 1) Removals, descending so the not-yet-processed indices stay valid.
    QVector<int> removed;
    for (int i = 0; i < m_songs.size(); ++i)
        if (!newRows.contains(m_songs.at(i).id))
            removed.append(i);
    for (int k = removed.size() - 1; k >= 0; --k) {
        const int row = removed.at(k);
        beginRemoveRows(QModelIndex(), row, row);
        m_songs.remove(row);
        endRemoveRows();
    }

    // 2) Walk the new snapshot once: insert added ids at their position and
    //    refresh changed kept rows in place. Shared ids keep their relative
    //    order (checked above), so a single cursor tracks the next kept row.
    int dst = 0;
    for (int i = 0; i < newSongs.size(); ++i) {
        const Song &song = newSongs.at(i);
        if (!oldRows.contains(song.id)) {
            beginInsertRows(QModelIndex(), dst, dst);
            m_songs.insert(dst, song);
            endInsertRows();
        } else if (!sameDisplayData(m_songs.at(dst), song)) {
            m_songs[dst] = song;
            const QModelIndex idx = index(dst);
            emit dataChanged(idx, idx);
        }
        ++dst;
    }

    if (m_songs.size() != oldCount)
        emit countChanged();
}

void SongListModel::sort(Role role, Qt::SortOrder order)
{
    if (m_songs.size() < 2)
        return;

    auto compares = [role](const Song &a, const Song &b) {
        switch (role) {
        case IdRole:          return a.id < b.id;
        case PathRole:        return a.path < b.path;
        case TitleRole:       return a.title < b.title;
        case ArtistRole:      return a.artist < b.artist;
        case AlbumRole:       return a.album < b.album;
        case GenreRole:       return a.genre < b.genre;
        case YearRole:        return a.year < b.year;
        case TrackNumberRole: return a.trackNumber < b.trackNumber;
        case DiscNumberRole:  return a.discNumber < b.discNumber;
        case DurationRole:    return a.duration < b.duration;
        case ArtworkRole:     return a.artworkPath < b.artworkPath;
        case FavoriteRole:    return a.favorite < b.favorite;
        case PlayCountRole:   return a.playCount < b.playCount;
        case LastPlayedRole:  return a.lastPlayed < b.lastPlayed;
        default:              return false;
        }
    };
    if (order == Qt::AscendingOrder) {
        std::sort(m_songs.begin(), m_songs.end(), compares);
    } else {
        std::sort(m_songs.begin(), m_songs.end(),
                  [compares](const Song &a, const Song &b) { return compares(b, a); });
    }

    beginResetModel();
    endResetModel();
    emit countChanged();
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

QStringList SongListModel::pathsForIds(const QVariantList &ids) const
{
    QStringList paths;
    if (ids.isEmpty()) {
        return paths;
    }
    QSet<qint64> wanted;
    wanted.reserve(ids.size());
    for (const QVariant &id : ids) {
        wanted.insert(id.toLongLong());
    }
    paths.reserve(wanted.size());
    for (const Song &song : m_songs) {
        if (wanted.contains(song.id) && !song.path.isEmpty()) {
            paths.append(song.path);
        }
    }
    return paths;
}

QVariantList SongListModel::toVariantList() const
{
    QVariantList list;
    list.reserve(m_songs.size());
    for (const Song &song : m_songs) {
        list.append(song.toVariantMap());
    }
    return list;
}