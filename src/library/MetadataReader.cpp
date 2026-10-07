#include "MetadataReader.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>

#include <QFileInfo>

MetadataReader::MetadataReader()
{
}

Song MetadataReader::read(const QString &filePath) const
{
    Song song;
    song.path = filePath;

    TagLib::FileRef file(filePath.toStdString().c_str());
    if (file.isNull()) {
        qWarning() << "Failed to read metadata from:" << filePath;
        return song;
    }

    TagLib::Tag *tag = file.tag();
    if (tag) {
        song.title = QString::fromStdString(tag->title().toCString() ? tag->title().toCString() : "");
        song.artist = QString::fromStdString(tag->artist().toCString() ? tag->artist().toCString() : "");
        song.album = QString::fromStdString(tag->album().toCString() ? tag->album().toCString() : "");
        song.genre = QString::fromStdString(tag->genre().toCString() ? tag->genre().toCString() : "");
        song.year = tag->year();
        song.trackNumber = tag->track();
    }

    TagLib::AudioProperties *properties = file.audioProperties();
    if (properties) {
        song.duration = properties->lengthInMilliseconds();
    }

    return song;
}

bool MetadataReader::canRead(const QString &filePath)
{
    TagLib::FileRef file(filePath.toStdString().c_str());
    return !file.isNull();
}
