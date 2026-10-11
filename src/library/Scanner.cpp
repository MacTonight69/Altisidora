#include "Scanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>

namespace {

// Supported audio extensions. Constructed once per process: the old code
// rebuilt this QStringList on every isSupportedAudioFile() call (~2-3 calls
// per file during a scan), and the extension check was a linear contains().
static const QStringList kSupportedExtensions = {
    "mp3", "flac", "ogg", "m4a", "wav", "opus", "aac", "wma", "aiff",
    // WebM / Matroska carry audio (Opus/Vorbis) that libmpv plays back; the
    // video track, if any, is simply not displayed.
    "webm", "mka", "mkv"
};

static const QSet<QString> kSupportedExtensionSet(kSupportedExtensions.cbegin(),
                                                 kSupportedExtensions.cend());

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
    return kSupportedExtensions; // API preserved for callers and tests
}

bool Scanner::isSupportedAudioFile(const QString &filePath)
{
    return kSupportedExtensionSet.contains(QFileInfo(filePath).suffix().toLower());
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
        // QDir::NoSymLinks already filtered symlinks out of the iteration,
        // so every remaining entry is a real file or directory. Reuse the
        // iterator's cached QFileInfo instead of building a fresh one (which
        // would stat the entry again) just to tell the two apart.
        if (it.fileInfo().isDir())
            found += scan(entry, ignoredPaths);
        else if (isSupportedAudioFile(entry))
            found.append(entry);
    }

    return found;
}
