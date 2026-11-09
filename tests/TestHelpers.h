#pragma once

#include <QString>
#include <QStringList>

class QObject;

/**
 * @brief Common test utilities for Altisidora tests
 *
 * Provides helper functions for creating test data used across multiple test
 * suites.
 */
namespace TestHelpers {

/**
 * @brief Creates a minimal valid audio file for testing
 * @param filePath Path where the file should be created
 * @return true if file was created successfully
 */
bool createDummyAudioFile(const QString &filePath);

/**
 * @brief Creates a real MPEG-1 Layer III file with a known duration.
 *
 * Each frame is 417 bytes (128 kbps / 44.1 kHz, no padding) and lasts
 * ~26.12 ms, so frameCount frames play for approximately
 * frameCount * 26.12 ms. Cross-checked by MetadataReader::read().
 *
 * @param filePath Path where the file should be created
 * @param frameCount Number of MPEG frames to write
 * @return true if the file was written successfully
 */
bool createMpegAudioFile(const QString &filePath, int frameCount);

/**
 * @brief Creates a test directory structure with audio files
 * @param basePath Base directory path
 * @param fileCount Number of audio files to create
 * @return List of created file paths
 */
QStringList createTestMusicDirectory(const QString &basePath, int fileCount = 5);

/**
 * @brief Removes the library database and its SQLite sidecar files.
 * @return true if all files were removed or did not exist
 */
bool removeDatabaseFiles();

/**
 * @brief Waits for an asynchronous scan to finish.
 * @param controller Controller whose scanFinished() signal is awaited
 * @return true if the signal arrived within the timeout
 */
bool waitForScanFinished(QObject *controller);

// Minimal valid 1x1 red PNG (69 bytes): small enough to inline, real enough
// for the artwork cache's magic-byte sniff.
inline constexpr const char *kRedPngHex =
    "89504e470d0a1a0a0000000d4948445200000001000000010802000000907753de"
    "0000000c4944415478da63f8cfc0000003010100f70341430000000049454e44ae426082";

} // namespace TestHelpers
