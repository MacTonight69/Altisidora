#include "Scanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

namespace {

/**
 * @brief Whether a path matches an ignored entry.
 *
 * An entry hides itself (exact match) and, when it is a directory,
 * everything below it (prefix match on the next path segment).
 */
bool isIgnoredEntry(const QString &path, const QStringList &ignoredPaths)
{
    for (const QString &ignored : ignoredPaths) {
        QString dir = ignored;
        while (dir.endsWith(QDir::separator()))
            dir.chop(1);
        if (dir.isEmpty())
            continue;
        if (path == dir || path.startsWith(dir + QDir::separator()))
            return true;
    }
    return false;
}

} // namespace

Scanner::Scanner()
{
}

QStringList Scanner::supportedExtensions()
{
    return {"mp3", "flac", "ogg", "m4a", "wav", "opus", "aac", "wma", "aiff"};
}

bool Scanner::isSupportedAudioFile(const QString &filePath)
{
    QString ext = QFileInfo(filePath).suffix().toLower();
    return supportedExtensions().contains(ext);
}

QVector<QString> Scanner::scan(const QString &dirPath,
                               const QStringList &ignoredPaths) const
{
    QVector<QString> found;
    if (isIgnoredEntry(dirPath, ignoredPaths))
        return found;

    // Walk the tree manually (one directory level at a time) so ignored
    // directories are never descended into. Symlinked directories are not
    // followed, mirroring the previous QDir::NoSymLinks behaviour.
    QDirIterator it(dirPath,
                    QDir::Dirs | QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot,
                    QDirIterator::NoIteratorFlags);

    while (it.hasNext()) {
        const QString entry = it.next();
        // Skip both ignored directories (never descend into them) and
        // ignored files (never report them).
        if (isIgnoredEntry(entry, ignoredPaths))
            continue;
        const QFileInfo info(entry);
        if (info.isSymLink())
            continue;
        if (info.isDir())
            found += scan(entry, ignoredPaths);
        else if (isSupportedAudioFile(entry))
            found.append(entry);
    }

    return found;
}
