#include "models/Song.h"

QVariantMap Song::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("path")] = path;
    map[QStringLiteral("title")] = title;
    map[QStringLiteral("artist")] = artist;
    map[QStringLiteral("album")] = album;
    map[QStringLiteral("genre")] = genre;
    map[QStringLiteral("year")] = year;
    map[QStringLiteral("trackNumber")] = trackNumber;
    map[QStringLiteral("discNumber")] = discNumber;
    map[QStringLiteral("duration")] = duration;
    map[QStringLiteral("artworkPath")] = artworkPath;
    map[QStringLiteral("dateAdded")] = dateAdded;
    map[QStringLiteral("lastPlayed")] = lastPlayed;
    map[QStringLiteral("playCount")] = playCount;
    map[QStringLiteral("favorite")] = favorite;
    return map;
}

Song Song::fromVariantMap(const QVariantMap &map)
{
    Song song;
    song.id = map.value(QStringLiteral("id"), 0).toInt();
    song.path = map.value(QStringLiteral("path")).toString();
    song.title = map.value(QStringLiteral("title")).toString();
    song.artist = map.value(QStringLiteral("artist")).toString();
    song.album = map.value(QStringLiteral("album")).toString();
    song.genre = map.value(QStringLiteral("genre")).toString();
    song.year = map.value(QStringLiteral("year"), 0).toInt();
    song.trackNumber = map.value(QStringLiteral("trackNumber"), 0).toInt();
    song.discNumber = map.value(QStringLiteral("discNumber"), 0).toInt();
    song.duration = map.value(QStringLiteral("duration"), 0).toLongLong();
    song.artworkPath = map.value(QStringLiteral("artworkPath")).toString();
    song.dateAdded = map.value(QStringLiteral("dateAdded")).toDateTime();
    song.lastPlayed = map.value(QStringLiteral("lastPlayed")).toDateTime();
    song.playCount = map.value(QStringLiteral("playCount"), 0).toInt();
    song.favorite = map.value(QStringLiteral("favorite"), false).toBool();
    return song;
}
