#pragma once

#include <QString>
#include <QVariantMap>

/**
 * @brief Represents a music album with aggregated information.
 */
struct Album {
    int id = 0;
    QString title;
    QString artist;
    int year = 0;
    int songCount = 0;
    QString artworkPath;

    QVariantMap toVariantMap() const;
    static Album fromVariantMap(const QVariantMap &map);
};
