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
    /**
     * @brief A scanned file path plus its change-detection fingerprint.
     *
     * mtimeMs/size let a rescan tell whether a previously stored file is
     * unchanged and can therefore skip the (expensive) tag read.
     */
    struct FileEntry {
        QString path;
        qint64 mtimeMs = 0;
        qint64 size = 0;
    };

    Scanner();

    /**
     * @brief Scan a directory recursively for audio files.
     * @param dirPath Path to scan
     * @param ignoredPaths Paths (files or directories) to skip. Files
     *        matching an entry and everything below an ignored directory
     *        are not reported.
     * @return List of found audio file paths
     */
    QVector<QString> scan(const QString &dirPath,
                          const QStringList &ignoredPaths) const;

    /**
     * @brief Scan a directory recursively for audio files with file stats.
     *
     * Same walk and filtering rules as scan(), but each result also carries
     * the file's modification time and size so callers can detect changes.
     *
     * @return List of found audio files with their mtime and size
     */
    QVector<FileEntry> scanEntries(const QString &dirPath,
                                   const QStringList &ignoredPaths) const;

    /**
     * @brief Check if a file extension is a supported audio format.
     */
    static bool isSupportedAudioFile(const QString &filePath);
};
