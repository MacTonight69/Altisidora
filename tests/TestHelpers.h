#pragma once

#include <QObject>
#include <QString>

/**
 * @brief Common test utilities for Altisidora tests
 *
 * Provides helper methods for creating test data used across multiple test
 * suites.
 */
class TestHelpers : public QObject
{
    Q_OBJECT

public:
    explicit TestHelpers(QObject *parent = nullptr);
    ~TestHelpers() override;

    /**
     * @brief Creates a minimal valid audio file for testing
     * @param filePath Path where the file should be created
     * @return true if file was created successfully
     */
    static bool createDummyAudioFile(const QString &filePath);

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
    static bool createMpegAudioFile(const QString &filePath, int frameCount);

    /**
     * @brief Creates a minimal valid audio file with specific metadata
     * @param filePath Path where the file should be created
     * @param title Song title
     * @param artist Artist name
     * @param album Album name
     * @return true if file was created successfully
     */
    static bool createDummyAudioFile(const QString &filePath,
                                     const QString &title,
                                     const QString &artist,
                                     const QString &album);

    /**
     * @brief Generates a unique string for test data
     * @param prefix Optional prefix for the unique string
     * @return Unique string
     */
    static QString uniqueString(const QString &prefix = QStringLiteral("test"));

    /**
     * @brief Creates a test directory structure with audio files
     * @param basePath Base directory path
     * @param fileCount Number of audio files to create
     * @return List of created file paths
     */
    static QStringList createTestMusicDirectory(const QString &basePath, int fileCount = 5);
};
