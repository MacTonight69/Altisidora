#pragma once

#include <QString>
#include <QDateTime>
#include <QVariantMap>

/**
 * @brief Represents a single music track with all its metadata.
 *
 * This is a plain data structure with no Qt meta-object system dependency,
 * making it lightweight and easy to use in both C++ and QML contexts.
 * toVariantMap() is the single canonical mapping consumed by QML (via
 * Library and Playlists) and by the player.
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
    // File fingerprint captured at scan time. Not exposed to QML (it stays
    // out of toVariantMap); a rescan compares these against the stored values
    // to decide whether the file's tags must be read again.
    qint64 mtimeMs = 0; // last modification time, ms since epoch
    qint64 size = 0;    // file size in bytes
    QString artworkPath;
    QDateTime dateAdded;
    QDateTime lastPlayed;
    int playCount = 0;
    bool favorite = false;

    /**
     * @brief Canonical model-to-QML map.
     *
     * The keys and value types are the contract QML binds to. Note the
     * QML-facing key for artwork is "artwork", while the struct field is
     * named artworkPath after the database column.
     */
    QVariantMap toVariantMap() const;
};
