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
     * @return List of found audio file paths
     */
    QVector<QString> scan(const QString &dirPath) const;

    /**
     * @brief Check if a file extension is a supported audio format.
     */
    static bool isSupportedAudioFile(const QString &filePath);

private:
    static QStringList supportedExtensions();
};
