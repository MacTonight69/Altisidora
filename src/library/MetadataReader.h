#pragma once

#include <QSet>
#include <QString>

#include <optional>

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

    /**
     * @brief Delete artwork-cache files that no song references.
     *
     * The counterpart to cacheArtwork()/scan-time extraction: deleting a
     * song, deleting an album or replacing a cover used to leave the old
     * file behind forever, so the cache only ever grew. This is the
     * garbage collector, meant to run once per completed scan (never per
     * song or on a hot path).
     *
     * @p referenced is the authoritative set of cache paths still in use
     * (every non-empty songs.artwork_path). It is an optional on purpose:
     * std::nullopt means the caller could NOT read the reference set (the
     * database query failed), and this method then removes NOTHING. Treating
     * a failed read as "nothing is referenced" would delete the user's whole
     * cover cache, so the failure path fails closed.
     *
     * Only regular files directly inside the cache directory are examined:
     * no recursion, and symlinks are neither followed nor removed. The
     * directory itself is never touched. Individual I/O failures are logged
     * and skipped rather than aborting the pass.
     *
     * @return Number of files removed (0 when nothing qualified, when the
     *         directory does not exist, or when @p referenced is nullopt).
     */
    static int pruneArtworkCache(const std::optional<QSet<QString>> &referenced);
};
