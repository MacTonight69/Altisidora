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
     * @brief Check if a file can be parsed by TagLib.
     */
    static bool canRead(const QString &filePath);
};
