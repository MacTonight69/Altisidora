#pragma once

#include <QString>
#include <QDateTime>

/**
 * @brief Represents a playlist with aggregated information.
 */
struct Playlist {
    qint64 id = 0;
    QString name;
    QString description;
    QDateTime dateCreated;
    int songCount = 0;
};
