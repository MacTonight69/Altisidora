#include "models/Genre.h"

QVariantMap Genre::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("name")] = name;
    map[QStringLiteral("songCount")] = songCount;
    return map;
}

Genre Genre::fromVariantMap(const QVariantMap &map)
{
    Genre genre;
    genre.id = map.value(QStringLiteral("id"), 0).toInt();
    genre.name = map.value(QStringLiteral("name")).toString();
    genre.songCount = map.value(QStringLiteral("songCount"), 0).toInt();
    return genre;
}
