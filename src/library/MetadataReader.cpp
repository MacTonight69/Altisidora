#include "MetadataReader.h"

#include "Scanner.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>

#include <QFileInfo>

namespace {

// Converts a TagLib::String to a QString using its UTF-8 representation.
// NOTE: TagLib::String::toCString() is unreliable in TagLib 2.x (it can
// return a pointer to a not-yet-initialised internal buffer, i.e. garbage
// that even starts with the ID3v2 text-encoding byte). to8Bit(true) always
// returns correct UTF-8 bytes, so we decode from that buffer by size.
QString tagStringToQString(const TagLib::String &str)
{
    if (str.isEmpty())
        return {};
    const std::string utf8 = str.to8Bit(true);
    return QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size()));
}

} // namespace

MetadataReader::MetadataReader()
{
}

Song MetadataReader::read(const QString &filePath) const
{
    Song song;
    song.path = filePath;

    TagLib::FileRef file(filePath.toStdString().c_str());
    if (file.isNull()) {
        // Not parseable by TagLib (unsupported or corrupt). The file is
        // still ingested so it stays playable; only its tags are lost.
        qWarning() << "Failed to read metadata from:" << filePath;
    } else {
        TagLib::Tag *tag = file.tag();
        if (tag) {
            song.title = tagStringToQString(tag->title());
            song.artist = tagStringToQString(tag->artist());
            song.album = tagStringToQString(tag->album());
            song.genre = tagStringToQString(tag->genre());
            song.year = tag->year();
            song.trackNumber = tag->track();
        }

        TagLib::AudioProperties *properties = file.audioProperties();
        if (properties) {
            song.duration = properties->lengthInMilliseconds();
        }
    }

    // Fall back to the file name so songs without tags are still
    // identifiable in the library view. This also covers files TagLib
    // cannot parse at all (e.g. a WebM it does not recognise): as long as
    // the file exists and the scanner would accept it, it keeps a title.
    // Unsupported extensions keep the previous contract (empty title).
    if (song.title.isEmpty()
        && QFileInfo(filePath).exists()
        && Scanner::isSupportedAudioFile(filePath)) {
        song.title = QFileInfo(filePath).completeBaseName();
    }

    return song;
}

