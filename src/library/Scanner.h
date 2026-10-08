#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Scans directories for audio files.
 */
class Scanner
{
public:
    Scanner();

    struct Result {
        QStringList supportedExtensions;
        QVector<QString> foundFiles;
    };

    /**
     * @brief Scan a directory recursively for audio files.
     * @param dirPath Path to scan
     * @param ignoredPaths Paths (files or directories) to skip. Files
     *        matching an entry and everything below an ignored directory
     *        are not reported.
     * @return List of found audio file paths
     */
    QVector<QString> scan(const QString &dirPath,
                          const QStringList &ignoredPaths = QStringList()) const;

    /**
     * @brief Check if a file extension is a supported audio format.
     */
    static bool isSupportedAudioFile(const QString &filePath);

private:
    static QStringList supportedExtensions();
};
