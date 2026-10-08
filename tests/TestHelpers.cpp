#include "TestHelpers.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QGlobalStatic>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QDateTime>
#include <QUuid>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <QCoreApplication>

TestHelpers::TestHelpers(QObject *parent)
    : QObject(parent)
{
}

TestHelpers::~TestHelpers() = default;

namespace {

// Tracks directories created by createTempDirectory() so they are removed
// when the test process exits (QTemporaryDir alone cannot be returned by
// value: its destructor would delete the directory on return).
class TempDirectoryRegistry
{
public:
    ~TempDirectoryRegistry()
    {
        for (const QString &path : std::as_const(paths)) {
            QDir(path).removeRecursively();
        }
    }

    void add(const QString &path) { paths.insert(path); }

private:
    QSet<QString> paths;
};

Q_GLOBAL_STATIC(TempDirectoryRegistry, g_tempDirectoryRegistry)

} // namespace

QString TestHelpers::createTempDirectory()
{
    // Create the directory under QDir::tempPath() instead of relying on a
    // QTemporaryDir local (which is destroyed when this function returns).
    const QString path = QDir::temp().filePath(uniqueString(QStringLiteral("musicrep")));
    if (QDir().mkpath(path)) {
        g_tempDirectoryRegistry->add(path);
        return path;
    }
    return QString();
}

QTemporaryDir TestHelpers::createTempDirGuard()
{
    return QTemporaryDir();
}

bool TestHelpers::createDummyAudioFile(const QString &filePath)
{
    return createDummyAudioFile(filePath, QStringLiteral("Test Title"),
                               QStringLiteral("Test Artist"),
                               QStringLiteral("Test Album"));
}

bool TestHelpers::createDummyAudioFile(const QString &filePath,
                                        const QString &title,
                                        const QString &artist,
                                        const QString &album)
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

    // Write metadata as a simple text block at the end
    QString metadata = QString("TITLE=%1\nARTIST=%2\nALBUM=%3\n")
                           .arg(title, artist, album);
    dummyData.append(metadata.toUtf8());

    qint64 written = file.write(dummyData);
    file.close();

    return written > 0;
}

bool TestHelpers::createMpegAudioFile(const QString &filePath, int frameCount)
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

QSqlDatabase TestHelpers::createTestDatabase()
{
    QString connectionName = QStringLiteral("test_db_%1").arg(uniqueString());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(QStringLiteral(":memory:"));

    if (!db.open()) {
        qWarning() << "Failed to open test database:" << db.lastError().text();
        return QSqlDatabase();
    }

    return db;
}

QSqlDatabase TestHelpers::createTestDatabase(const QString &dbPath)
{
    QString connectionName = QStringLiteral("test_db_%1").arg(uniqueString());
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        qWarning() << "Failed to open test database:" << db.lastError().text();
        return QSqlDatabase();
    }

    return db;
}

bool TestHelpers::waitFor(std::function<bool()> condition,
                           int timeoutMs,
                           int pollIntervalMs)
{
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (condition()) {
            return true;
        }
        QThread::msleep(pollIntervalMs);
        QCoreApplication::processEvents();
    }

    return condition();
}

QString TestHelpers::uniqueString(const QString &prefix)
{
    return QStringLiteral("%1_%2").arg(prefix, QUuid::createUuid().toString(QUuid::Id128));
}

QStringList TestHelpers::createTestMusicDirectory(const QString &basePath, int fileCount)
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

        if (createDummyAudioFile(filePath,
                                 QStringLiteral("Track %1").arg(i + 1),
                                 artist,
                                 album)) {
            createdFiles.append(filePath);
        }
    }

    return createdFiles;
}
