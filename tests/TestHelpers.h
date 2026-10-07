#pragma once

#include <QObject>
#include <QString>
#include <QTemporaryDir>
#include <QSqlDatabase>

/**
 * @brief Common test utilities for MusicRep tests
 *
 * Provides helper methods for creating test data, temporary directories,
 * and database fixtures used across multiple test suites.
 */
class TestHelpers : public QObject
{
    Q_OBJECT

public:
    explicit TestHelpers(QObject *parent = nullptr);
    ~TestHelpers() override;

    /**
     * @brief Creates a temporary directory for test files
     * @return Path to the created temporary directory
     */
    static QString createTempDirectory();

    /**
     * @brief Creates a temporary directory and returns a cleanup guard
     * @return QTemporaryDir instance (must be kept alive for the duration of the test)
     */
    static QTemporaryDir createTempDirGuard();

    /**
     * @brief Creates a minimal valid audio file for testing
     * @param filePath Path where the file should be created
     * @return true if file was created successfully
     */
    static bool createDummyAudioFile(const QString &filePath);

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
     * @brief Creates an in-memory SQLite database for testing
     * @return QSqlDatabase instance (in-memory)
     */
    static QSqlDatabase createTestDatabase();

    /**
     * @brief Creates a file-based SQLite database for testing
     * @param dbPath Path for the database file
     * @return QSqlDatabase instance
     */
    static QSqlDatabase createTestDatabase(const QString &dbPath);

    /**
     * @brief Waits for a condition with timeout
     * @param condition Function that returns true when condition is met
     * @param timeoutMs Maximum wait time in milliseconds
     * @param pollIntervalMs Polling interval in milliseconds
     * @return true if condition was met within timeout
     */
    static bool waitFor(std::function<bool()> condition,
                        int timeoutMs = 5000,
                        int pollIntervalMs = 50);

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
