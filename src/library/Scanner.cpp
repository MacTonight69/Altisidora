#include "Scanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

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

QVector<QString> Scanner::scan(const QString &dirPath) const
{
    QVector<QString> found;
    QDirIterator it(dirPath, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filePath = it.next();
        if (isSupportedAudioFile(filePath)) {
            found.append(filePath);
        }
    }

    return found;
}
