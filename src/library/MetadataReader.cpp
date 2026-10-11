#include "MetadataReader.h"

#include "Scanner.h"

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/audioproperties.h>
#include <taglib/tvariant.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

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

// Extracts the embedded front-cover picture (falls back to the first
// picture found) and persists it under a content-hash name. Returns the
// absolute cache file path, or an empty string when there is no artwork.
QString extractArtwork(const TagLib::FileRef &file)
{
    const TagLib::List<TagLib::VariantMap> pictures =
        file.complexProperties("PICTURE");
    if (pictures.isEmpty())
        return {};

    const TagLib::VariantMap *chosen = nullptr;
    for (const TagLib::VariantMap &pic : pictures) {
        const TagLib::String type = pic.contains("pictureType")
            ? pic["pictureType"].toString() : TagLib::String();
        if (type == "Front Cover") { chosen = &pic; break; }
        if (!chosen) chosen = &pic;
    }

    if (!chosen || !chosen->contains("data"))
        return {};
    bool ok = false;
    const TagLib::ByteVector bytes = (*chosen)["data"].toByteVector(&ok);
    if (!ok || bytes.isEmpty())
        return {};

    // Extension from the declared MIME type, with magic-byte sniffing as
    // the fallback (some tags lie or omit the MIME type).
    QString mime = chosen->contains("mimeType")
        ? tagStringToQString((*chosen)["mimeType"].toString()).toLower() : "";
    QString ext;
    if (mime == "image/jpeg" || mime == "image/jpg") ext = "jpg";
    else if (mime == "image/png")  ext = "png";
    else if (mime == "image/webp") ext = "webp";
    else if (mime == "image/gif")  ext = "gif";
    else if (mime == "image/bmp")  ext = "bmp";
    else ext = (bytes.size() > 3 && (unsigned char)bytes[0] == 0x89) ? "png" : "jpg";

    const QString cacheDir = QStandardPaths::writableLocation(
                                 QStandardPaths::AppDataLocation) + "/artwork";
    if (!QDir().mkpath(cacheDir))
        return {};

    // Content-addressed name: identical art across an album's tracks
    // collides into ONE file, and re-extraction is idempotent.
    const QByteArray raw(bytes.data(), bytes.size());
    const QString name = QString::fromLatin1(QCryptographicHash::hash(
        raw, QCryptographicHash::Sha256).toHex().left(32)) + '.' + ext;
    const QString outPath = cacheDir + '/' + name;
    if (QFileInfo::exists(outPath))
        return outPath;

    QSaveFile f(outPath);
    if (!f.open(QIODevice::WriteOnly) || f.write(raw) != raw.size() || !f.commit())
        return {};
    return outPath;
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

        // Persist the embedded cover art (if any) to the cache at scan
        // time: the QML side only ever loads a local file:// URL.
        song.artworkPath = extractArtwork(file);
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

