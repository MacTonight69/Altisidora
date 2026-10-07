#pragma once

#include <QString>
#include <QDateTime>
#include <QVariantMap>

/**
 * @brief Represents a single music track with all its metadata.
 *
 * This is a plain data structure with no Qt meta-object system dependency,
 * making it lightweight and easy to use in both C++ and QML contexts.
 * Conversion methods are provided for QML interoperability.
 */
struct Song {
    int id = 0;
    QString path;
    QString title;
    QString artist;
    QString album;
    QString genre;
    int year = 0;
    int trackNumber = 0;
    int discNumber = 0;
    qint64 duration = 0; // milliseconds
    QString artworkPath;
    QDateTime dateAdded;
    QDateTime lastPlayed;
    int playCount = 0;
    bool favorite = false;

    QVariantMap toVariantMap() const;
    static Song fromVariantMap(const QVariantMap &map);
};
