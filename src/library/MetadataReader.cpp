#include "MetadataReader.h"

#include "Scanner.h"

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/taglib.h>

// TagLib 2.0 replaced the per-format artwork APIs with the unified
// complexProperties("PICTURE") interface. Keep the old formats working
// on TagLib 1.x (1.12 is the oldest version we test): the fallback below
// reads pictures from each container's native tag type.
#if TAGLIB_MAJOR_VERSION < 2
#include <taglib/aifffile.h>
#include <taglib/apetag.h>
#include <taglib/asfattribute.h>
#include <taglib/asffile.h>
#include <taglib/asfpicture.h>
#include <taglib/asftag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/id3v2tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mp4item.h>
#include <taglib/mp4tag.h>
#include <taglib/mpegfile.h>
#include <taglib/oggflacfile.h>
#include <taglib/opusfile.h>
#include <taglib/speexfile.h>
#include <taglib/tfile.h>
#include <taglib/vorbisfile.h>
#include <taglib/wavfile.h>
#include <taglib/xiphcomment.h>
#endif

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QStringEncoder>

namespace {

// Recovers tags that a legacy (mostly Japanese-ripper-era) tagger wrote as
// Shift-JIS bytes while declaring the ID3v2 frame ISO-8859-1 (encoding
// byte 0). TagLib then maps every raw byte to the Unicode code point
// U+00xx, e.g. "8e b5 90 6c" (七人) becomes U+008E U+00B5 U+0090 U+006C.
// The original bytes are still fully recoverable — every char is <= U+00FF
// at that point — so we take them back with toLatin1() and re-decode them
// as Shift-JIS. Rejection gates, in order (any failure returns the text
// untouched):
//
//  1. any char above U+00FF: a genuine UTF-8/UTF-16 tag, nothing to recover;
//  2. no char above U+007F: pure ASCII;
//  3. no C1 control (U+0081-U+009F): Shift-JIS double-byte lead bytes for
//     kana and common kanji are 0x81-0x9F, so real mojibake ALWAYS carries
//     at least one C1 char, while printed Latin-1 text (French/German/
//     Spanish accents, "¡¢£") never does — this is the discriminator that
//     keeps genuine Latin-1 tags safe;
//  4. fewer than 4 high bytes while the string is not ALL high bytes: a
//     sparse CP1252 smart quote/em dash, not a double-byte CJK sequence.
//
// The re-decode must then clear three more hurdles:
//  - decoder.hasError(): invalid byte pairs substitute U+001A (not the
//    usual U+FFFD) and raise the error flag;
//  - the result must contain FULL-WIDTH Japanese (hiragana, katakana or
//    CJK unified ideographs): single bytes 0xA1-0xDF also decode
//    "validly" as half-width katakana (U+FF61-U+FF9F), which would let
//    non-Japanese garbage through the gates above;
//  - re-encoding must reproduce the original bytes EXACTLY. This gate is
//    mandatory because a trailing incomplete lead byte is silently buffered
//    by the decoder (output is just truncated, hasError() stays false);
//    only the byte comparison catches it.
//
// The converters are constructed per call: this only runs for strings that
// passed all four gates (rare), and their cost is microseconds against the
// file I/O of a library scan, so caching is not worth the complexity.
QString fixShiftJisMojibake(const QString &text)
{
    const qsizetype total = text.size();
    if (total == 0)
        return text;

    qsizetype highCount = 0;
    bool hasC1 = false;
    for (const QChar ch : text) {
        const char16_t u = ch.unicode();
        if (u > 0x00FF)
            return text;                    // 1. real UTF-8/UTF-16 text
        if (u >= 0x0080)
            ++highCount;
        if (u >= 0x0081 && u <= 0x009F)     // 3. Shift-JIS double-byte lead
            hasC1 = true;
    }
    if (highCount == 0)
        return text;                        // 2. pure ASCII
    if (!hasC1)
        return text;                        // 3. accented Latin-1: no SJIS signature
    if (highCount < 4 && highCount != total)
        return text;                        // 4. sparse CP1252 quotes etc.

    // Lossless here: every char is <= U+00FF.
    const QByteArray bytes = text.toLatin1();

    QStringDecoder decoder("Shift-JIS");
    const QString decoded = decoder.decode(bytes);
    if (decoder.hasError() || decoded.isEmpty())
        return text;

    bool hasFullWidthJapanese = false;
    for (const QChar ch : decoded) {
        const char16_t u = ch.unicode();
        if ((u >= 0x3040 && u <= 0x309F)        // Hiragana
            || (u >= 0x30A0 && u <= 0x30FF)     // Katakana
            || (u >= 0x4E00 && u <= 0x9FFF)) {   // CJK Unified Ideographs
            hasFullWidthJapanese = true;
            break;
        }
    }
    if (!hasFullWidthJapanese)
        return text;                        // half-width kana is not acceptance proof

    QStringEncoder encoder("Shift-JIS");
    const QByteArray reencoded = encoder.encode(decoded);
    if (encoder.hasError() || reencoded != bytes)
        return text;                        // catches silently buffered trailing leads
    return decoded;
}

// Converts a TagLib::String to a QString using its UTF-8 representation.
// NOTE: TagLib::String::toCString() is unreliable in TagLib 2.x (it can
// return a pointer to a not-yet-initialised internal buffer, i.e. garbage
// that even starts with the ID3v2 text-encoding byte). to8Bit(true) always
// returns correct UTF-8 bytes, so we decode from that buffer by size.
// This is the single choke point for every text tag (title/artist/album/
// genre, plus the ASCII-only artwork MIME string), so the Shift-JIS
// fallback is applied here as the last step.
QString tagStringToQString(const TagLib::String &str)
{
    if (str.isEmpty())
        return {};
    const std::string utf8 = str.to8Bit(true);
    return fixShiftJisMojibake(
        QString::fromUtf8(utf8.data(), static_cast<qsizetype>(utf8.size())));
}

// Extension for the declared MIME type (expected lower-case); empty when
// the type is unknown or not an image.
QString imageExtensionFromMime(const QString &mime)
{
    if (mime == "image/jpeg" || mime == "image/jpg") return "jpg";
    if (mime == "image/png")  return "png";
    if (mime == "image/webp") return "webp";
    if (mime == "image/gif")  return "gif";
    if (mime == "image/bmp")  return "bmp";
    return {};
}

// Extension sniffed from the leading magic bytes; empty when they match
// no known image format. Shared by the scan (fallback for tags that lie
// about or omit the MIME type) and by cacheArtwork() (the only check a
// user-picked file gets).
QString imageExtensionFromMagicBytes(const QByteArray &raw)
{
    if (raw.size() >= 4 && (unsigned char)raw[0] == 0x89
        && raw[1] == 'P' && raw[2] == 'N' && raw[3] == 'G') {
        return "png";
    }
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xFF
        && (unsigned char)raw[1] == 0xD8 && (unsigned char)raw[2] == 0xFF) {
        return "jpg";
    }
    if (raw.startsWith("GIF87a") || raw.startsWith("GIF89a"))
        return "gif";
    if (raw.size() >= 2 && raw[1] == 'M' && raw[0] == 'B')
        return "bmp";
    if (raw.size() >= 12 && raw.startsWith("RIFF") && raw.mid(8, 4) == "WEBP")
        return "webp";
    return {};
}

// Writes image bytes into the artwork cache under a content-addressed
// name: identical art across an album's tracks collides into ONE file,
// and re-saving is idempotent. Shared by the scan-time extraction and by
// cacheArtwork(), so hand-picked covers deduplicate with extracted ones.
// Returns the absolute cache file path, or an empty string on failure.
QString cacheArtworkBytes(const QByteArray &raw, const QString &ext)
{
    if (raw.isEmpty() || ext.isEmpty())
        return {};

    const QString cacheDir = QStandardPaths::writableLocation(
                                 QStandardPaths::AppDataLocation) + "/artwork";
    if (!QDir().mkpath(cacheDir))
        return {};

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

QByteArray byteVectorToQByteArray(const TagLib::ByteVector &bytes)
{
    return QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size()));
}

#if TAGLIB_MAJOR_VERSION < 2

struct LegacyPicture
{
    QByteArray data;
    QString mimeType;
    bool frontCover = false;
};

QString imageMimeFromFileName(const QString &fileName)
{
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (suffix == "jpg" || suffix == "jpeg") return QStringLiteral("image/jpeg");
    if (suffix == "png")  return QStringLiteral("image/png");
    if (suffix == "webp") return QStringLiteral("image/webp");
    if (suffix == "gif")  return QStringLiteral("image/gif");
    if (suffix == "bmp")  return QStringLiteral("image/bmp");
    return {};
}

void appendId3v2Pictures(TagLib::ID3v2::Tag *tag,
                         QList<LegacyPicture> &pictures)
{
    if (!tag)
        return;

    const TagLib::ID3v2::FrameList frames = tag->frameList("APIC");
    for (TagLib::ID3v2::Frame *frame : frames) {
        auto *picture = dynamic_cast<TagLib::ID3v2::AttachedPictureFrame *>(frame);
        if (!picture)
            continue;
        const TagLib::ByteVector bytes = picture->picture();
        if (bytes.isEmpty())
            continue;
        pictures.append({
            byteVectorToQByteArray(bytes),
            tagStringToQString(picture->mimeType()).toLower(),
            picture->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover
        });
    }
}

void appendFlacPictures(const TagLib::List<TagLib::FLAC::Picture *> &flacPictures,
                        QList<LegacyPicture> &pictures)
{
    for (const TagLib::FLAC::Picture *picture : flacPictures) {
        if (!picture)
            continue;
        const TagLib::ByteVector bytes = picture->data();
        if (bytes.isEmpty())
            continue;
        pictures.append({
            byteVectorToQByteArray(bytes),
            tagStringToQString(picture->mimeType()).toLower(),
            picture->type() == TagLib::FLAC::Picture::FrontCover
        });
    }
}

void appendMp4Pictures(TagLib::MP4::Tag *tag,
                       QList<LegacyPicture> &pictures)
{
    if (!tag || !tag->contains("covr"))
        return;

    const TagLib::MP4::CoverArtList covers = tag->item("covr").toCoverArtList();
    for (const TagLib::MP4::CoverArt &cover : covers) {
        const TagLib::ByteVector bytes = cover.data();
        if (bytes.isEmpty())
            continue;

        QString mime;
        switch (cover.format()) {
        case TagLib::MP4::CoverArt::JPEG: mime = QStringLiteral("image/jpeg"); break;
        case TagLib::MP4::CoverArt::PNG:  mime = QStringLiteral("image/png");  break;
        case TagLib::MP4::CoverArt::BMP:  mime = QStringLiteral("image/bmp");  break;
        case TagLib::MP4::CoverArt::GIF:  mime = QStringLiteral("image/gif");  break;
        case TagLib::MP4::CoverArt::Unknown: break;
        }

        // MP4 has no picture-type field; the first item is the cover.
        pictures.append({
            byteVectorToQByteArray(bytes),
            mime,
            pictures.isEmpty()
        });
    }
}

void appendAsfPictures(TagLib::ASF::Tag *tag,
                       QList<LegacyPicture> &pictures)
{
    if (!tag)
        return;

    const TagLib::ASF::AttributeList attributes = tag->attribute("WM/Picture");
    for (const TagLib::ASF::Attribute &attribute : attributes) {
        const TagLib::ASF::Picture picture = attribute.toPicture();
        if (!picture.isValid())
            continue;
        const TagLib::ByteVector bytes = picture.picture();
        if (bytes.isEmpty())
            continue;
        pictures.append({
            byteVectorToQByteArray(bytes),
            tagStringToQString(picture.mimeType()).toLower(),
            picture.type() == TagLib::ASF::Picture::FrontCover
        });
    }
}

void appendApePictures(TagLib::APE::Tag *tag,
                       QList<LegacyPicture> &pictures)
{
    if (!tag)
        return;

    // APE stores cover art as a binary item whose value starts with the
    // original file name, a NUL separator, and then the image bytes.
    const TagLib::APE::ItemListMap &items = tag->itemListMap();
    for (auto it = items.begin(); it != items.end(); ++it) {
        const TagLib::String key = it->first.upper();
        const bool isFront = key == "COVER ART (FRONT)";
        if (!isFront && key != "COVER ART (BACK)")
            continue;

        const TagLib::APE::Item &item = it->second;
        if (item.type() != TagLib::APE::Item::Binary)
            continue;

        TagLib::ByteVector bytes = item.binaryData();
        QString mime;
        const int separator = bytes.find('\0');
        if (separator >= 0) {
            const auto nameLength = static_cast<unsigned int>(separator);
            const QByteArray name = byteVectorToQByteArray(bytes.mid(0, nameLength));
            mime = imageMimeFromFileName(QString::fromLatin1(name));
            bytes = bytes.mid(nameLength + 1);
        }
        if (bytes.isEmpty())
            continue;

        pictures.append({
            byteVectorToQByteArray(bytes),
            mime,
            isFront
        });
    }
}

void appendXiphPictures(TagLib::Ogg::XiphComment *xiph,
                        QList<LegacyPicture> &pictures)
{
    if (xiph)
        appendFlacPictures(xiph->pictureList(), pictures);
}

void collectLegacyPictures(TagLib::File *file,
                           QList<LegacyPicture> &pictures)
{
    if (!file)
        return;

    // TagLib 1.x's generic File::tag() does not necessarily return the
    // format-specific tag object (and its docs warn against casting it),
    // so ask each File subclass for its own tag accessor instead.
    if (auto *mpeg = dynamic_cast<TagLib::MPEG::File *>(file)) {
        appendId3v2Pictures(mpeg->ID3v2Tag(false), pictures);
        appendApePictures(mpeg->APETag(false), pictures);
    } else if (auto *flac = dynamic_cast<TagLib::FLAC::File *>(file)) {
        appendFlacPictures(flac->pictureList(), pictures);
        appendId3v2Pictures(flac->ID3v2Tag(false), pictures);
        appendXiphPictures(flac->xiphComment(false), pictures);
    } else if (auto *vorbis = dynamic_cast<TagLib::Ogg::Vorbis::File *>(file)) {
        appendXiphPictures(vorbis->tag(), pictures);
    } else if (auto *opus = dynamic_cast<TagLib::Ogg::Opus::File *>(file)) {
        appendXiphPictures(opus->tag(), pictures);
    } else if (auto *speex = dynamic_cast<TagLib::Ogg::Speex::File *>(file)) {
        appendXiphPictures(speex->tag(), pictures);
    } else if (auto *oggFlac = dynamic_cast<TagLib::Ogg::FLAC::File *>(file)) {
        appendXiphPictures(oggFlac->tag(), pictures);
    } else if (auto *mp4 = dynamic_cast<TagLib::MP4::File *>(file)) {
        appendMp4Pictures(dynamic_cast<TagLib::MP4::Tag *>(mp4->tag()), pictures);
    } else if (auto *wav = dynamic_cast<TagLib::RIFF::WAV::File *>(file)) {
        appendId3v2Pictures(wav->ID3v2Tag(), pictures);
    } else if (auto *aiff = dynamic_cast<TagLib::RIFF::AIFF::File *>(file)) {
        appendId3v2Pictures(aiff->tag(), pictures);
    } else if (auto *asf = dynamic_cast<TagLib::ASF::File *>(file)) {
        appendAsfPictures(dynamic_cast<TagLib::ASF::Tag *>(asf->tag()), pictures);
    }
}

QString extractArtworkLegacy(const TagLib::FileRef &file)
{
    QList<LegacyPicture> pictures;
    collectLegacyPictures(file.file(), pictures);
    if (pictures.isEmpty())
        return {};

    // Same preference as the TagLib 2.x path: front cover first, otherwise
    // the first picture in tag order.
    const LegacyPicture *chosen = &pictures.first();
    for (const LegacyPicture &picture : pictures) {
        if (picture.frontCover) {
            chosen = &picture;
            break;
        }
    }

    QString ext = imageExtensionFromMime(chosen->mimeType);
    if (ext.isEmpty())
        ext = imageExtensionFromMagicBytes(chosen->data);
    if (ext.isEmpty())
        ext = QStringLiteral("jpg");

    return cacheArtworkBytes(chosen->data, ext);
}

#endif // TAGLIB_MAJOR_VERSION < 2

// Extracts the embedded front-cover picture (falls back to the first
// picture found) and persists it under a content-hash name. Returns the
// absolute cache file path, or an empty string when there is no artwork.
QString extractArtwork(const TagLib::FileRef &file)
{
#if TAGLIB_MAJOR_VERSION < 2
    return extractArtworkLegacy(file);
#else
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
    // the fallback (some tags lie or omit the MIME type). A tag that
    // claims to embed art but decodes to no known magic keeps the
    // historical "jpg" default rather than dropping the picture.
    const QByteArray raw = byteVectorToQByteArray(bytes);
    QString ext = imageExtensionFromMime(chosen->contains("mimeType")
        ? tagStringToQString((*chosen)["mimeType"].toString()).toLower() : QString());
    if (ext.isEmpty())
        ext = imageExtensionFromMagicBytes(raw);
    if (ext.isEmpty())
        ext = QStringLiteral("jpg");

    return cacheArtworkBytes(raw, ext);
#endif
}

} // namespace

MetadataReader::MetadataReader()
{
}

QString MetadataReader::cacheArtwork(const QString &imagePath)
{
    QFile file(imagePath);
    if (imagePath.isEmpty() || !file.open(QIODevice::ReadOnly)) {
        qWarning() << "cacheArtwork: cannot read" << imagePath;
        return {};
    }
    const QByteArray raw = file.readAll();
    // Strict magic-byte sniff: a user-picked file must be a real image.
    // (The scan's "jpg" fallback would cache arbitrary bytes under a
    // wrong extension instead of refusing.)
    const QString ext = imageExtensionFromMagicBytes(raw);
    if (ext.isEmpty()) {
        qWarning() << "cacheArtwork: not a recognized image file:" << imagePath;
        return {};
    }
    return cacheArtworkBytes(raw, ext);
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

