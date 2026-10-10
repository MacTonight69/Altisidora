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
    map[QStringLiteral("artwork")] = artworkPath;
    map[QStringLiteral("dateAdded")] = dateAdded;
    map[QStringLiteral("lastPlayed")] = lastPlayed;
    map[QStringLiteral("playCount")] = playCount;
    map[QStringLiteral("favorite")] = favorite;
    return map;
}
