#include "TestHelpers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QStandardPaths>

namespace TestHelpers {

bool createDummyAudioFile(const QString &filePath)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    // Write a minimal file that looks like an audio file
    // This is a placeholder - real implementation would use TagLib
    QByteArray dummyData;
    dummyData.reserve(1024);

    // Write some header bytes to make it look like a media file
    if (filePath.endsWith(".mp3", Qt::CaseInsensitive)) {
        dummyData.append("\xFF\xFB\x90\x00"); // MPEG frame header
    } else if (filePath.endsWith(".ogg", Qt::CaseInsensitive)) {
        dummyData.append("OggS"); // Ogg container header
    } else if (filePath.endsWith(".flac", Qt::CaseInsensitive)) {
        dummyData.append("fLaC"); // FLAC header
    } else if (filePath.endsWith(".wav", Qt::CaseInsensitive)) {
        dummyData.append("RIFF"); // WAV header
    }

    // Pad with zeros to make it a reasonable size
    dummyData.append(QByteArray(1024 - dummyData.size(), '\0'));

    qint64 written = file.write(dummyData);
    file.close();

    return written > 0;
}

bool createMpegAudioFile(const QString &filePath, int frameCount)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    // MPEG-1 Layer III, 128 kbps, 44.1 kHz, no padding: frame size is 417.
    const QByteArray header("\xFF\xFB\x90\x00", 4);
    const QByteArray payload(417 - header.size(), '\0');
    for (int i = 0; i < frameCount; ++i) {
        if (file.write(header) != header.size()) {
            return false;
        }
        if (file.write(payload) != payload.size()) {
            return false;
        }
    }
    file.close();

    return QFile::exists(filePath);
}

QStringList createTestMusicDirectory(const QString &basePath, int fileCount)
{
    QStringList createdFiles;
    QDir baseDir(basePath);

    QStringList artists = {QStringLiteral("Artist A"), QStringLiteral("Artist B"), QStringLiteral("Artist C")};
    QStringList albums = {QStringLiteral("Album 1"), QStringLiteral("Album 2"), QStringLiteral("Album 3")};
    QStringList extensions = {QStringLiteral(".mp3"), QStringLiteral(".ogg"), QStringLiteral(".flac")};

    for (int i = 0; i < fileCount; ++i) {
        QString artist = artists[i % artists.size()];
        QString album = albums[i % albums.size()];
        QString ext = extensions[i % extensions.size()];

        QString fileName = QStringLiteral("track%1_%2_%3%4")
                               .arg(i + 1, 2, 10, QChar('0'))
                               .arg(artist)
                               .arg(album)
                               .arg(ext);

        QString filePath = baseDir.filePath(fileName);

        if (createDummyAudioFile(filePath)) {
            createdFiles.append(filePath);
        }
    }

    return createdFiles;
}

bool removeDatabaseFiles()
{
    const QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataPath);

    const QStringList suffixes = {
        QString(),
        QStringLiteral("-journal"),
        QStringLiteral("-wal"),
        QStringLiteral("-shm")
    };
    bool ok = true;
    for (const QString &suffix : suffixes) {
        const QString path = dataPath + QStringLiteral("/library.db") + suffix;
        if (QFile::exists(path) && !QFile::remove(path)) {
            ok = false;
        }
    }
    return ok;
}

bool waitForScanFinished(QObject *controller)
{
    // Name the signal instead of taking its address: TestHelpers is linked
    // into every test, including ones that do not link the library core, so
    // a typed connection would create an unwanted link dependency.
    QSignalSpy finishedSpy(controller, SIGNAL(scanFinished(int)));
    return finishedSpy.wait(10000);
}

} // namespace TestHelpers
