#include "models/Artist.h"

QVariantMap Artist::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("name")] = name;
    map[QStringLiteral("songCount")] = songCount;
    map[QStringLiteral("albumCount")] = albumCount;
    return map;
}

Artist Artist::fromVariantMap(const QVariantMap &map)
{
    Artist artist;
    artist.id = map.value(QStringLiteral("id"), 0).toInt();
    artist.name = map.value(QStringLiteral("name")).toString();
    artist.songCount = map.value(QStringLiteral("songCount"), 0).toInt();
    artist.albumCount = map.value(QStringLiteral("albumCount"), 0).toInt();
    return artist;
}
