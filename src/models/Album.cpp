#include "models/Album.h"

QVariantMap Album::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("title")] = title;
    map[QStringLiteral("artist")] = artist;
    map[QStringLiteral("year")] = year;
    map[QStringLiteral("songCount")] = songCount;
    map[QStringLiteral("artworkPath")] = artworkPath;
    return map;
}

Album Album::fromVariantMap(const QVariantMap &map)
{
    Album album;
    album.id = map.value(QStringLiteral("id"), 0).toInt();
    album.title = map.value(QStringLiteral("title")).toString();
    album.artist = map.value(QStringLiteral("artist")).toString();
    album.year = map.value(QStringLiteral("year"), 0).toInt();
    album.songCount = map.value(QStringLiteral("songCount"), 0).toInt();
    album.artworkPath = map.value(QStringLiteral("artworkPath")).toString();
    return album;
}
