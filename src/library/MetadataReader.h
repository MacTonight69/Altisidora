#pragma once

#include <QString>

#include "../models/Song.h"

/**
 * @brief Extracts metadata from audio files using TagLib.
 */
class MetadataReader
{
public:
    MetadataReader();

    /**
     * @brief Read metadata from an audio file.
     * @param filePath Path to the audio file
     * @return Song object with extracted metadata, or empty Song on failure
     */
    Song read(const QString &filePath) const;

    /**
     * @brief Copy a user-picked image file into the artwork cache.
     *
     * Same pipeline as scan-time embedded-art extraction (content-hash
     * name, QSaveFile, AppDataLocation/artwork/), so hand-picked covers
     * deduplicate with extracted ones and re-saving is idempotent.
     * Refuses non-image files (magic-byte sniff).
     * @return Absolute cache file path, or an empty string on failure.
     */
    static QString cacheArtwork(const QString &imagePath);
};
